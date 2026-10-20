#!/usr/bin/env python3
"""Precompute the quantized training inputs once instead of every epoch.

The streaming path rebuilt `section_features` + `quantize_sections` for every
batch, which is ~15 numpy column_stacks and a pile of float32 temporaries over
32k rows, single-threaded, on the same thread that feeds the GPU. With a 226k
parameter model the GPU finishes long before the next batch exists, so the run
is bound by numpy rather than by arithmetic.

The quantizer is deterministic given the scale constants, so its output can be
computed once and streamed as plain uint8 afterwards. A pass then costs a gather
and a host-to-device copy, with no float math at all.

The cache is keyed to a digest of ODD_SCALES/EVEN_SCALES. Changing the binning
invalidates it loudly rather than silently training on stale bins - the same
failure mode the quantizer-agreement gate exists to prevent.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

from .section_lattice_model import (
    EVEN_FEATURES,
    FULL_EVEN_FEATURES,
    FULL_ODD_FEATURES,
    LEAN,
    MINI,
    EVEN_FULL_SCALES,
    EVEN_SCALES,
    ODD_FEATURES,
    ODD_FULL_SCALES,
    ODD_SCALES,
    balanced_route_numpy,
    quantize_sections,
)
from .calibration_model import IN_CHECK
from .section_model import KNOWN_ENDGAME, SECTION_DTYPE, section_features

MAGIC = b"FBINCACH"
MAX_PIECES = 32
SPARSE_DIM = 768
SPARSE_HEADER = 20
SPARSE_ROW = np.dtype([("count", "<u2"), ("idx", "<u2", (MAX_PIECES,))])
VERSION = 3


def scales_digest() -> str:
    """Keyed on the scales the cache was written with, not the model's.

    The cache always stores full-width bins; a model that consumes a subset of
    them selects columns at load time. Digesting the reduced scales would
    reject a perfectly valid cache, while the reduced feature set is already
    protected where it matters -- the net file's own bin-layout digest.
    """
    h = hashlib.sha256()
    h.update(np.ascontiguousarray(ODD_FULL_SCALES, dtype=np.float64).tobytes())
    h.update(np.ascontiguousarray(EVEN_FULL_SCALES, dtype=np.float64).tobytes())
    return h.hexdigest()


# The width the cache was written with, which is not the width the model reads.
# FORGE-mini and FORGE-lean select a subset of columns at load time, so the file
# on disk is still full width; FORGE_DROP_ABSDIFF instead changed what got written, and its
# caches really are narrower. The two are mutually exclusive.
CACHE_ODD_FEATURES = FULL_ODD_FEATURES if (MINI or LEAN) else ODD_FEATURES
CACHE_EVEN_FEATURES = FULL_EVEN_FEATURES if (MINI or LEAN) else EVEN_FEATURES


def layout(rows: int) -> dict[str, tuple[str, tuple[int, ...]]]:
    return {
        "route": ("u1", (rows,)),
        "odd_sign": ("i1", (rows, CACHE_ODD_FEATURES)),
        "odd_bins": ("u1", (rows, CACHE_ODD_FEATURES)),
        "even_bins": ("u1", (rows, CACHE_EVEN_FEATURES)),
        # Side-to-move-relative piece-square planes, row-aligned with the
        # section corpus. Factor8 otherwise sees placement only as the two
        # tapered psqt scalars.
        "sparse_count": ("<u2", (rows,)),
        "sparse_idx": ("<u2", (rows, MAX_PIECES)),
        "targets": ("<f2", (rows, 3)),
    }


def open_cache(path: Path, mode: str = "r"):
    meta = json.loads(path.with_suffix(path.suffix + ".json").read_text())
    if meta["magic"] != MAGIC.decode() or meta["version"] != VERSION:
        raise RuntimeError(f"incompatible bin cache: {path}")
    if meta["scales_digest"] != scales_digest():
        raise RuntimeError(
            f"bin cache {path} was built for different scale constants "
            f"({meta['scales_digest'][:12]} vs {scales_digest()[:12]}); rebuild it")
    rows = meta["rows"]
    out, offset = {}, 0
    for name, (dtype, shape) in layout(rows).items():
        out[name] = np.memmap(path, mode=mode, dtype=dtype, shape=shape, offset=offset)
        offset += int(np.prod(shape)) * np.dtype(dtype).itemsize
    return out, meta


def build(records: Path, targets: Path, out: Path, sparse: Path,
          chunk: int = 1_000_000) -> dict:
    total = records.stat().st_size // SECTION_DTYPE.itemsize
    raw = np.memmap(records, mode="r", dtype=SECTION_DTYPE, shape=(total,))
    target_rows = targets.stat().st_size // 6
    if total > target_rows:
        raise RuntimeError(f"record/target mismatch: {total} > {target_rows}")
    tgt = np.memmap(targets, mode="r", dtype=np.float16, shape=(target_rows, 3))[:total]
    with open(sparse, "rb") as handle:
        if handle.read(4) != b"SPRS":
            raise RuntimeError(f"not a sparse plane file: {sparse}")
        version, sparse_rows, max_pieces, dim = struct.unpack("<IIII", handle.read(16))
    if (version, max_pieces, dim) != (1, MAX_PIECES, SPARSE_DIM):
        raise RuntimeError(f"unexpected sparse header in {sparse}")
    if sparse_rows < total:
        raise RuntimeError(f"sparse/record mismatch: {sparse_rows} < {total}")
    spr = np.memmap(sparse, mode="r", dtype=SPARSE_ROW, offset=SPARSE_HEADER,
                    shape=(sparse_rows,))[:total]

    keep_count = 0
    for start in range(0, total, chunk):
        block = raw[start:start + chunk]
        keep_count += int((((block["context"]["flags"] & IN_CHECK) == 0)
                           & ((block["section_flags"] & KNOWN_ENDGAME) == 0)).sum())

    spec = layout(keep_count)
    nbytes = sum(int(np.prod(s)) * np.dtype(d).itemsize for d, s in spec.values())
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as handle:
        handle.truncate(nbytes)

    views, offset = {}, 0
    for name, (dtype, shape) in spec.items():
        views[name] = np.memmap(out, mode="r+", dtype=dtype, shape=shape, offset=offset)
        offset += int(np.prod(shape)) * np.dtype(dtype).itemsize

    written = 0
    route_counts = np.zeros(8, dtype=np.int64)
    for start in range(0, total, chunk):
        block = raw[start:start + chunk]
        keep = (((block["context"]["flags"] & IN_CHECK) == 0)
                & ((block["section_flags"] & KNOWN_ENDGAME) == 0))
        selected = block[keep]
        if not len(selected):
            continue
        odd, even = section_features(selected)
        sign, obin, ebin = quantize_sections(odd, even)
        route = balanced_route_numpy(selected["context"])
        t = np.asarray(tgt[start:start + chunk][keep], dtype=np.float32)
        t /= np.maximum(t.sum(axis=1, keepdims=True), 1e-12)
        end = written + len(selected)
        views["route"][written:end] = route
        views["odd_sign"][written:end] = sign
        views["odd_bins"][written:end] = obin
        views["even_bins"][written:end] = ebin
        block_sparse = spr[start:start + chunk][keep]
        views["sparse_count"][written:end] = block_sparse["count"]
        views["sparse_idx"][written:end] = block_sparse["idx"]
        views["targets"][written:end] = t.astype(np.float16)
        route_counts += np.bincount(route, minlength=8)
        written = end
        print(f"  {written:,}/{keep_count:,} rows", flush=True)
    assert written == keep_count, (written, keep_count)
    for view in views.values():
        view.flush()

    meta = {
        "magic": MAGIC.decode(),
        "version": VERSION,
        "rows": keep_count,
        "source_rows": total,
        "records": str(records.resolve()),
        "targets": str(targets.resolve()),
        "sparse": str(sparse.resolve()),
        "scales_digest": scales_digest(),
        "bytes": nbytes,
        "route_counts": route_counts.tolist(),
    }
    out.with_suffix(out.suffix + ".json").write_text(json.dumps(meta, indent=2) + "\n")
    return meta


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("records", type=Path)
    parser.add_argument("targets", type=Path)
    parser.add_argument("out", type=Path)
    parser.add_argument("sparse", type=Path)
    parser.add_argument("--chunk", type=int, default=1_000_000)
    args = parser.parse_args()
    print(json.dumps(build(args.records, args.targets, args.out, args.sparse, args.chunk), indent=2))


if __name__ == "__main__":
    main()
