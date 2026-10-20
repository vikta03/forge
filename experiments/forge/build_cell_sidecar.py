#!/usr/bin/env python3
"""Emit the router's cell index for every row of a bin cache.

The cache stores `route`, which is what balanced_route_numpy already decided.
A learned router needs the input that decision was made from, not its output,
so this recovers (phase, pawns) per kept row and packs it into one index.

Written as a sidecar rather than a new cache column deliberately: the psq cache
is 11 GiB and took twenty minutes to build, and rebuilding it to add two bytes
a row would invalidate every net trained against it. The sidecar is 2 bytes a
row and is checked against the cache's row count and route column on load, so a
mismatched pair fails loudly instead of training on misaligned cells.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .build_bin_cache import open_cache
from .calibration_model import IN_CHECK
from .section_lattice_model import balanced_route_numpy
from .section_model import KNOWN_ENDGAME, SECTION_DTYPE

MAX_PHASE = 24
MAX_PAWNS = 16
CELLS = (MAX_PHASE + 1) * (MAX_PAWNS + 1)


def cells_from_context(context: np.ndarray) -> np.ndarray:
    """phase x total pawns, clamped the way the runtime LUT index is."""
    phase = np.minimum(context["phase"].astype(np.int32), MAX_PHASE)
    pawns = (context["own_pawns"].astype(np.int32)
             + context["enemy_pawns"].astype(np.int32))
    if pawns.max(initial=0) > MAX_PAWNS:
        raise RuntimeError(f"pawn total {pawns.max()} exceeds {MAX_PAWNS}")
    return (phase * (MAX_PAWNS + 1) + pawns).astype(np.uint16)


def build(records: Path, cache: Path, out: Path, chunk: int = 1_000_000) -> dict:
    views, meta = open_cache(cache)
    rows = meta["rows"]
    total = records.stat().st_size // SECTION_DTYPE.itemsize
    raw = np.memmap(records, mode="r", dtype=SECTION_DTYPE, shape=(total,))

    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as handle:
        handle.truncate(rows * 2)
    sidecar = np.memmap(out, mode="r+", dtype=np.uint16, shape=(rows,))

    written = 0
    for start in range(0, total, chunk):
        block = raw[start:start + chunk]
        keep = (((block["context"]["flags"] & IN_CHECK) == 0)
                & ((block["section_flags"] & KNOWN_ENDGAME) == 0))
        selected = block[keep]
        if not len(selected):
            continue
        end = written + len(selected)
        if end > rows:
            raise RuntimeError(f"more kept rows than the cache has: {end} > {rows}")
        sidecar[written:end] = cells_from_context(selected["context"])
        # The same filter and the same order must have produced the cache, so
        # the route it stored has to be the route these cells imply.
        expected = balanced_route_numpy(selected["context"])
        stored = np.asarray(views["route"][written:end])
        if not np.array_equal(expected, stored):
            bad = int(np.flatnonzero(expected != stored)[0])
            raise RuntimeError(
                f"sidecar is misaligned with {cache} at row {written + bad}: "
                f"cache says route {stored[bad]}, records say {expected[bad]}")
        written = end
        print(f"  {written:,}/{rows:,} rows", flush=True)
    if written != rows:
        raise RuntimeError(f"kept {written} rows, cache has {rows}")
    sidecar.flush()

    counts = np.bincount(np.asarray(sidecar), minlength=CELLS)
    info = {"rows": rows, "cells": CELLS, "cache": str(cache),
            "occupied_cells": int((counts > 0).sum()),
            "cell_counts": counts.tolist()}
    out.with_suffix(out.suffix + ".json").write_text(json.dumps(info) + "\n")
    return info


def load(path: Path, rows: int) -> np.ndarray:
    info = json.loads(path.with_suffix(path.suffix + ".json").read_text())
    if info["rows"] < rows:
        raise RuntimeError(f"cell sidecar {path} has {info['rows']} rows, need {rows}")
    if info["cells"] != CELLS:
        raise RuntimeError(f"cell sidecar {path} uses {info['cells']} cells, not {CELLS}")
    return np.memmap(path, mode="r", dtype=np.uint16, shape=(info["rows"],))[:rows]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--records", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    info = build(args.records, args.cache, args.out)
    print(f"{args.out}: {info['rows']:,} rows, "
          f"{info['occupied_cells']} of {CELLS} cells occupied")


if __name__ == "__main__":
    main()
