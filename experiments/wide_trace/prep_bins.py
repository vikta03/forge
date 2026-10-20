#!/usr/bin/env python3
"""Bin the widened trace once, the way the main cache is binned once.

Layout, per kept row: 26 int8 odd signs | 26 uint8 odd bins | 26 uint8 even bins.
Odd features are (White - Black) x orient and carry their sign outside the table;
even features are White + Black on a code centred at 64. Same convention as
section_features/quantize_sections, so the new columns behave like the old ones.

These bin edges are chosen for THIS run only (largest power of two keeping the
99.9th percentile in range, measured on validation). They are not registered in
rescale_shift.json, so a net trained here cannot be exported to the runtime
without adding them there first. Training-only, deliberately. RULES.md R2.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from forge.section_model import SECTION_DTYPE, KNOWN_ENDGAME, BLACK_TO_MOVE
from forge.calibration_model import IN_CHECK

Q = 26
CHUNK = 2_000_000

def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("sections", type=Path)
    ap.add_argument("widetrace", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--scales", type=Path,
                    default=Path(__file__).with_name("wide_scales.json"))
    args = ap.parse_args()

    scales = json.loads(args.scales.read_text())
    names = list(scales)
    assert len(names) == Q, f"{len(names)} scales for {Q} quantities"
    so = np.array([scales[n]["odd"] for n in names], dtype=np.float64)
    se = np.array([scales[n]["even"] for n in names], dtype=np.float64)

    sec = np.memmap(args.sections, mode="r", dtype=SECTION_DTYPE)
    side = np.memmap(args.widetrace, mode="r", dtype="<i2").reshape(-1, 2 * Q)

    # orient, read in chunks so a 7 GB records file is streamed, not materialised
    orients = []
    for start in range(0, len(sec), CHUNK):
        block = sec[start:start + CHUNK]
        flags = np.asarray(block["context"]["flags"])
        keep = ((flags & IN_CHECK) == 0) & ((np.asarray(block["section_flags"]) & KNOWN_ENDGAME) == 0)
        orients.append(np.where((flags[keep] & BLACK_TO_MOVE) != 0, -1, 1).astype(np.int32))
    orient = np.concatenate(orients)
    if len(orient) != len(side):
        raise SystemExit(f"row mismatch: {len(orient)} kept in records, {len(side)} in sidecar")

    out = np.lib.format.open_memmap if False else None
    with open(args.out, "wb") as fh:
        for start in range(0, len(side), CHUNK):
            stop = min(start + CHUNK, len(side))
            blk = np.asarray(side[start:stop], dtype=np.int32)
            o = orient[start:stop][:, None]
            W, B = blk[:, :Q], blk[:, Q:]
            odd = (W - B) * o
            even = W + B
            sign = np.sign(odd).astype(np.int8)
            ob = np.clip(np.rint(np.abs(odd) * so), 0, 128).astype(np.uint8)
            eb = (np.clip(np.rint(even * se), -64, 64) + 64).astype(np.uint8)
            row = np.concatenate([sign.view(np.uint8), ob, eb], axis=1)
            fh.write(np.ascontiguousarray(row).tobytes())
    n = args.out.stat().st_size // (3 * Q)
    print(f"{args.out.name}: {n:,} rows x {3*Q} bytes"
          f"   ({'OK' if n == len(side) else 'ROW COUNT MISMATCH'})")

if __name__ == "__main__":
    main()
