#!/usr/bin/env python3
"""Export a trained factor lattice to FORGE's fixed packed runtime format."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

import numpy as np
import torch

from .build_cell_sidecar import CELLS as ROUTE_CELLS
from .router_em import cascade_lut
from .section_lattice_model import (
    EVEN_KEEP,
    LEAN,
    MINI,
    ODD_KEEP,
    PSQ_CROSSGATE,
    EVEN_FEATURES,
    ODD_FEATURES,
    ROUTES,
    UNARY_BINS,
    RoutedFactorLattice,
)


MAGIC = b"FORGEF08"
# 4: the router itself is now data. Versions 1-3 carried a partition that was
# hard-coded twice, once in Python and once in C++, with nothing tying the two
# copies together; a v4 file ships the (phase, pawns) -> route tables it was
# trained under, and the two heads may use different ones. A v3 file has no
# such tables, so the version check is what keeps the two apart.
# 5 adds an explicit own-king bucket count and a bucket-conditioned PSQ table.
# One-bucket checkpoints deliberately remain v4 so the existing Mini artifact
# can still be reproduced and loaded by the same runtime.
LEGACY_VERSION = 4
KING_VERSION = 5
MLP_VERSION = 6
# Compatibility name for tests and callers that export an ordinary one-bucket
# checkpoint. King-aware exports select KING_VERSION dynamically below.
VERSION = LEGACY_VERSION
# Defaults retained for callers that construct synthetic checkpoints without a
# full trainer config.  Real checkpoints carry both dimensions and the exporter
# treats those as authoritative; otherwise a valid factor4 run could only be
# exported by lying about its architecture through environment variables.
WIDTH = 8
PSQ_WIDTH = int(__import__("os").environ.get("FORGE_PSQ_WIDTH", "16"))
READOUT_SCALE = 187.0
HEADER = struct.Struct("<8sIIIIIIIfQII8s")


def bin_layout_digest() -> int:
    """CRC32 over the (numerator, shift) bin table, matching the runtime.

    A net trained under different bin edges has the right shape, the right
    payload size and a valid CRC, so nothing else in the header catches it --
    it simply evaluates wrong. Mixing a pre-rebin net with the rebinned engine
    moved the start position from 12 to 8 centipawns with no diagnostic.
    Both sides digest the same generated artifact.
    """
    plan = json.loads((Path(__file__).resolve().parents[2]
                       / "rescale_shift.json").read_text())
    odd, even = plan["odd"], plan["even"]
    full_odd, full_even = list(odd), list(even)
    if MINI or LEAN:
        odd = [odd[i] for i in ODD_KEEP]
        even = [even[i] for i in EVEN_KEEP]
    elif EVEN_FEATURES != len(even):
        even = [e for i, e in enumerate(even) if not (40 <= i < 64)]
    if len(odd) != ODD_FEATURES or len(even) != EVEN_FEATURES:
        raise ValueError(f"bin plan has {len(odd)}/{len(even)} entries, "
                         f"model wants {ODD_FEATURES}/{EVEN_FEATURES}")
    blob = b"".join(struct.pack("<ii", n, sh) for n, sh in odd + even)
    if MINI or LEAN:
        # The runtime digests its own copy of the mask for the same reason: two
        # different masks could select the same ratio sequence, and a net
        # trained on a different feature set must be rejected, not misread.
        keep_odd = set(int(i) for i in ODD_KEEP)
        keep_even = set(int(i) for i in EVEN_KEEP)
        blob += b"".join(struct.pack("<i", int(i in keep_odd))
                         for i in range(len(full_odd)))
        blob += b"".join(struct.pack("<i", int(i in keep_even))
                         for i in range(len(full_even)))
    return zlib.crc32(blob) & 0xFFFF_FFFF
assert HEADER.size == 64


def _quantize(
    values: np.ndarray, levels: int, reduce_axes: tuple[int, ...]
) -> tuple[np.ndarray, np.ndarray]:
    maximum = np.max(np.abs(values), axis=reduce_axes, keepdims=True)
    scale = np.maximum(maximum / levels, 1e-12)
    quantized = np.rint(values / scale).clip(-levels, levels)
    dtype = np.int8 if levels <= 127 else np.dtype("<i2")
    squeezed = np.squeeze(scale, axis=reduce_axes).astype("<f4")
    return quantized.astype(dtype), squeezed


def checkpoint_dimensions(checkpoint: dict) -> tuple[int, int, int]:
    config = checkpoint.get("config", {})
    variant = config.get("variant", "factor8")
    if not isinstance(variant, str) or not variant.startswith("factor"):
        raise ValueError(f"checkpoint is not a factor lattice: {variant!r}")
    try:
        width = int(variant.removeprefix("factor"))
    except ValueError as exc:
        raise ValueError(f"invalid factor variant: {variant!r}") from exc
    psq_width = int(config.get("psq_width", PSQ_WIDTH))
    king_buckets = int(config.get("king_buckets", 1))
    if width not in (4, 8, 16):
        raise ValueError(f"unsupported factor width: {width}")
    if psq_width not in (8, 16):
        raise ValueError(f"unsupported piece-square width: {psq_width}")
    if king_buckets not in (1, 16, 32):
        raise ValueError(f"unsupported own-king bucket count: {king_buckets}")
    return width, psq_width, king_buckets


def checkpoint_mlp_product(checkpoint: dict) -> bool:
    """True when the head keeps tanh(odd)*sigmoid(even) as a mixer input.

    Read from the weight shape, like the hidden width: z is 2*width+psq for the
    plain mixer and 3*width+psq when the product block is present.
    """
    weights = checkpoint.get("model", {})
    if "mlp_w1" not in weights:
        return False
    z = int(weights["mlp_w1"].shape[1])
    width, psq_width, _ = checkpoint_dimensions(checkpoint)
    return z == 3 * width + psq_width


def checkpoint_mlp_hidden(checkpoint: dict) -> int:
    """Hidden width of the replacement-hybrid head, 0 if the net has none.

    The trainer selects the head through FORGE_MLP_HIDDEN rather than a flag,
    so the config may not record it. The weights always do, and they are the
    authority: reading the shape means a checkpoint cannot be exported against
    the wrong head because an environment variable was set differently.
    """
    weights = checkpoint.get("model", {})
    if "mlp_w1" in weights:
        return int(weights["mlp_w1"].shape[-1])
    return int(checkpoint.get("config", {}).get("mlp_hidden", 0) or 0)


def packed_arrays(checkpoint_path: Path) -> dict[str, np.ndarray]:
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    width, psq_width, king_buckets = checkpoint_dimensions(checkpoint)
    mlp_hidden = checkpoint_mlp_hidden(checkpoint)
    model = RoutedFactorLattice(width, psq_width=psq_width,
                                king_buckets=king_buckets, mlp_hidden=mlp_hidden,
                                mlp_product=checkpoint_mlp_product(checkpoint))
    model.load_state_dict(checkpoint["model"])
    state = {name: value.detach().cpu().numpy() for name, value in model.named_parameters()}
    advantage = state["advantage_shared"][None] + state["advantage_route"]
    decisive = state["decisive_shared"][None] + state["decisive_route"]
    decisive_bias = (
        state["decisive_bias_shared"] + state["decisive_bias_route"]
    ).astype("<f4")
    advantage_q, advantage_scale = _quantize(advantage, 32767, (1, 2))
    decisive_q, decisive_scale = _quantize(decisive, 32767, (1, 2))
    odd_q, odd_scale = _quantize(state["odd_latent"], 127, (0, 1))
    even_q, even_scale = _quantize(state["even_latent"], 127, (0, 1))
    # per-channel, matching the latent books
    psq_q, psq_scale = _quantize(state["psq_embed"], 127, (0,))
    return {
        "advantage": advantage_q,
        "decisive": decisive_q,
        "odd_latent": odd_q,
        "even_latent": even_q,
        "advantage_scale": advantage_scale,
        "decisive_scale": decisive_scale,
        "odd_scale": odd_scale,
        "even_scale": even_scale,
        "decisive_bias": decisive_bias,
        "advantage_out": state["advantage_out"].astype("<f4"),
        "decisive_out": state["decisive_out"].astype("<f4"),
        "psq_embed": psq_q,
        "psq_scale": psq_scale,
        "psq_advantage_out": state["psq_advantage_out"].astype("<f4"),
        "psq_decisive_out": state["psq_decisive_out"].astype("<f4"),
        **({
            # Replacement-hybrid head. Emitted as float32 for the first
            # runtime: the equal-node comparison does not depend on inference
            # speed, so int8 is deferred until the kernel is on the critical
            # path for an equal-TIME result.
            "mlp_w1": state["mlp_w1"].astype("<f4"),
            "mlp_b1": state["mlp_b1"].astype("<f4"),
            "mlp_adv": state["mlp_adv"].astype("<f4"),
            "mlp_dec": state["mlp_dec"].astype("<f4"),
            "mlp_scale": state["mlp_scale"].astype("<f4"),
        } if mlp_hidden else {}),
    }


def dequantized_model(checkpoint_path: Path) -> RoutedFactorLattice:
    """Build the exact PyTorch analogue of the packed inference weights."""

    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    width, psq_width, king_buckets = checkpoint_dimensions(checkpoint)
    arrays = packed_arrays(checkpoint_path)
    # The reference must be built with the SAME head the packed weights carry.
    # Building it without these silently compares a v6 runtime against the v5
    # fixed head, which looks exactly like a catastrophic kernel bug.
    model = RoutedFactorLattice(width, psq_width=psq_width,
                                king_buckets=king_buckets,
                                mlp_hidden=checkpoint_mlp_hidden(checkpoint),
                                mlp_product=checkpoint_mlp_product(checkpoint))
    with torch.no_grad():
        model.advantage_shared.zero_()
        model.decisive_shared.zero_()
        model.advantage_route.copy_(torch.from_numpy(
            arrays["advantage"].astype(np.float32)
            * arrays["advantage_scale"][:, None, None]
        ))
        model.decisive_route.copy_(torch.from_numpy(
            arrays["decisive"].astype(np.float32)
            * arrays["decisive_scale"][:, None, None]
        ))
        model.decisive_bias_shared.zero_()
        model.decisive_bias_route.copy_(torch.from_numpy(arrays["decisive_bias"]))
        model.odd_latent.copy_(torch.from_numpy(
            arrays["odd_latent"].astype(np.float32)
            * arrays["odd_scale"][None, None, :]
        ))
        model.even_latent.copy_(torch.from_numpy(
            arrays["even_latent"].astype(np.float32)
            * arrays["even_scale"][None, None, :]
        ))
        model.advantage_out.copy_(torch.from_numpy(arrays["advantage_out"]))
        model.decisive_out.copy_(torch.from_numpy(arrays["decisive_out"]))
        model.psq_embed.copy_(torch.from_numpy(
            arrays["psq_embed"].astype(np.float32) * arrays["psq_scale"][None, :]))
        model.psq_advantage_out.copy_(torch.from_numpy(arrays["psq_advantage_out"]))
        model.psq_decisive_out.copy_(torch.from_numpy(arrays["psq_decisive_out"]))
        if "mlp_w1" in arrays:
            model.mlp_w1.copy_(torch.from_numpy(arrays["mlp_w1"]))
            model.mlp_b1.copy_(torch.from_numpy(arrays["mlp_b1"]))
            model.mlp_adv.copy_(torch.from_numpy(arrays["mlp_adv"]))
            model.mlp_dec.copy_(torch.from_numpy(arrays["mlp_dec"]))
            model.mlp_scale.copy_(torch.from_numpy(arrays["mlp_scale"]))
    return model


def route_tables(checkpoint_path: Path) -> tuple[np.ndarray, np.ndarray]:
    """The (phase, pawns) -> route tables the checkpoint was trained under.

    A checkpoint from before the learned router has none, and the partition it
    used was the cascade, so that is what gets written. The runtime then has a
    single source for the routing rule either way.
    """
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    fallback = cascade_lut()
    lut_a = np.asarray(checkpoint.get("route_lut_a", fallback), dtype=np.uint8)
    lut_c = np.asarray(checkpoint.get("route_lut_c", fallback), dtype=np.uint8)
    for name, lut in (("a", lut_a), ("c", lut_c)):
        if lut.shape != (ROUTE_CELLS,):
            raise ValueError(f"route table {name} has shape {lut.shape}, want {ROUTE_CELLS}")
        if int(lut.max()) >= ROUTES:
            raise ValueError(f"route table {name} names route {lut.max()}, only {ROUTES} exist")
    return lut_a, lut_c


def export(checkpoint_path: Path, output_path: Path) -> dict[str, object]:
    checkpoint = torch.load(checkpoint_path, map_location="cpu", weights_only=True)
    width, psq_width, king_buckets = checkpoint_dimensions(checkpoint)
    arrays = packed_arrays(checkpoint_path)
    lut_a, lut_c = route_tables(checkpoint_path)
    arrays["route_lut_a"], arrays["route_lut_c"] = lut_a, lut_c
    order = (
        "advantage",
        "decisive",
        "odd_latent",
        "even_latent",
        "advantage_scale",
        "decisive_scale",
        "odd_scale",
        "even_scale",
        "decisive_bias",
        "advantage_out",
        "decisive_out",
        "psq_embed",
        "psq_scale",
        "psq_advantage_out",
        "psq_decisive_out",
        "route_lut_a",
        "route_lut_c",
    )
    mlp_hidden = checkpoint_mlp_hidden(checkpoint)
    if mlp_hidden:
        # appended after the v5 payload so a v6 file is a v5 file plus a head
        order = order + ("mlp_w1", "mlp_b1", "mlp_adv", "mlp_dec", "mlp_scale")
    payload = b"".join(np.ascontiguousarray(arrays[name]).tobytes() for name in order)
    checksum = zlib.crc32(payload) & 0xFFFF_FFFF
    version = LEGACY_VERSION if king_buckets == 1 else KING_VERSION
    if mlp_hidden:
        version = MLP_VERSION
    header = HEADER.pack(
        MAGIC,
        version,
        HEADER.size,
        ROUTES,
        ODD_FEATURES,
        EVEN_FEATURES,
        UNARY_BINS,
        width,
        READOUT_SCALE,
        len(payload),
        checksum,
        # bit 0: cross-gated psq head; bit 1: mixer keeps the product term;
        # bits 8-15: MLP hidden width
        (1 if PSQ_CROSSGATE and not mlp_hidden else 0)
        | (2 if checkpoint_mlp_product(checkpoint) else 0)
        | (mlp_hidden << 8),
        struct.pack("<II", bin_layout_digest(),
                    0 if version == LEGACY_VERSION else king_buckets),
    )
    encoded = header + payload
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(encoded)
    manifest = {
        "format": MAGIC.decode("ascii"),
        "version": version,
        "checkpoint": str(checkpoint_path),
        "checkpoint_sha256": hashlib.sha256(checkpoint_path.read_bytes()).hexdigest(),
        "output": str(output_path),
        "sha256": hashlib.sha256(encoded).hexdigest(),
        "bytes": len(encoded),
        "payload_bytes": len(payload),
        "payload_crc32": f"{checksum:08x}",
        "routes": ROUTES,
        "odd_features": ODD_FEATURES,
        "even_features": EVEN_FEATURES,
        "bins": UNARY_BINS,
        "width": width,
        "psq_width": psq_width,
        "king_buckets": king_buckets,
        "psq_crossgate": PSQ_CROSSGATE,
        "readout_scale": READOUT_SCALE,
        "latent_quantization": "signed int8 per channel",
        "direct_quantization": "signed int16 per route",
        "bin_layout_digest": f"{bin_layout_digest():08x}",
        "route_cells": ROUTE_CELLS,
        "route_split": bool((lut_a != lut_c).any()),
        "route_cells_moved_from_cascade": int((lut_a != cascade_lut()).sum()),
        "route_share_a": np.bincount(lut_a, minlength=ROUTES).tolist(),
        "route_share_c": np.bincount(lut_c, minlength=ROUTES).tolist(),
    }
    output_path.with_suffix(output_path.suffix + ".json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(export(args.checkpoint, args.output), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
