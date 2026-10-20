#!/usr/bin/env python3
"""Export one wide-trace checkpoint with the arm's exact bin-layout digest.

The ordinary exporter remains untouched. net0 uses its existing MINI contract;
netA/B/C append the experiment's selected ratios and mask to that contract.
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import zlib
from pathlib import Path


ARMS = {"net0", "netA", "netB", "netC"}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("arm", choices=sorted(ARMS))
    parser.add_argument("checkpoint", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    # These are the settings the four checkpoints were trained under. Set them
    # before importing the env-configured model module.
    os.environ["FORGE_MINI"] = "1"
    os.environ["FORGE_PSQ_CROSSGATE"] = "1"
    os.environ["FORGE_MLP_HIDDEN"] = "16"
    os.environ["FORGE_MLP_PRODUCT"] = "1"
    os.environ["FORGE_WIDE_ARM"] = args.arm

    from experiments.forge import export_factor_runtime as runtime
    from experiments.forge import section_lattice_model as model

    original_digest = runtime.bin_layout_digest

    def wide_digest() -> int:
        if args.arm == "net0":
            return original_digest()

        root = Path(__file__).resolve().parents[2]
        base = json.loads((root / "rescale_shift.json").read_text())
        wide = json.loads((Path(__file__).with_name("wide_ratios.json")).read_text())
        wide_odd = [entry["odd"] for entry in wide.values()]
        wide_even = [entry["even"] for entry in wide.values()]
        if len(wide_odd) != model.WIDE_Q:
            raise ValueError(f"wide ratio plan has {len(wide_odd)} entries, want {model.WIDE_Q}")

        base_odd_keep = [int(value) for value in model.ODD_KEEP]
        base_even_keep = [int(value) for value in model.EVEN_KEEP]
        wide_keep = [int(value) for value in model.WIDE_KEEP]
        odd = [base["odd"][i] for i in base_odd_keep] + [wide_odd[i] for i in wide_keep]
        even = [base["even"][i] for i in base_even_keep] + [wide_even[i] for i in wide_keep]
        if len(odd) != model.ODD_FEATURES or len(even) != model.EVEN_FEATURES:
            raise ValueError(
                f"arm {args.arm} bin plan is {len(odd)}/{len(even)}, "
                f"model wants {model.ODD_FEATURES}/{model.EVEN_FEATURES}"
            )

        blob = b"".join(struct.pack("<ii", n, shift) for n, shift in odd + even)
        base_odd_set = set(base_odd_keep)
        base_even_set = set(base_even_keep)
        wide_set = set(wide_keep)
        # Exact order used by the C++ runtime: base+wide odd mask, then
        # base+wide even mask.
        mask = (
            [int(i in base_odd_set) for i in range(len(base["odd"]))]
            + [int(i in wide_set) for i in range(model.WIDE_Q)]
            + [int(i in base_even_set) for i in range(len(base["even"]))]
            + [int(i in wide_set) for i in range(model.WIDE_Q)]
        )
        blob += b"".join(struct.pack("<i", value) for value in mask)
        return zlib.crc32(blob) & 0xFFFF_FFFF

    runtime.bin_layout_digest = wide_digest
    manifest = runtime.export(args.checkpoint, args.output)
    manifest["wide_arm"] = args.arm
    manifest["wide_quantities"] = list(model.WIDE_KEEP)
    args.output.with_suffix(args.output.suffix + ".json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    print(json.dumps(manifest, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
