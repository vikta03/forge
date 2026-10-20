#!/usr/bin/env python3
"""Re-bin the aggregate features for a handcrafted evaluation that computes
fewer sections.

Boring's aggregated score is FORGE's single strongest input, and it is the sum
of the twelve sections. Dropping sections from the *computation* therefore
changes that aggregate -- which is a different thing from FORGE merely not
reading a section's own features, and it is the part that actually saves time
in the handcrafted evaluation.

`extract_sections.cpp::replay` already reconstructs the aggregate from a trace;
`replay_numpy` below is the same arithmetic over the stored records, so a
candidate section set can be priced without re-extracting a 7 GiB corpus. Only
odd feature 0 (the aggregate) and even feature 0 (its magnitude) depend on it,
which this asserts rather than assumes: every other column is recomputed and
checked against the cache it will be spliced into.

The endgame scale factor moves too -- Eval::scale_factor reads the endgame\nhalf's sign to pick the stronger side -- so even feature 3 is re-binned as\nwell. Output is (odd_sign_0, odd_bin_0, even_bin_0, even_bin_3).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from .build_bin_cache import open_cache
from .calibration_model import IN_CHECK
from .section_lattice_model import quantize_sections
from .section_model import KNOWN_ENDGAME, SECTION_DTYPE, section_features

PHASE_MIDGAME, SCALE_NORMAL, TEMPO = 24, 64, 20
BISHOP_PAIR_MG, BISHOP_PAIR_EG = 28, 58
SCALE_DRAW, SCALE_OPPOSITE_BISHOPS, SCALE_ONE_PAWN = 0, 22, 34
KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE = 350, 370, 550, 1100
OPPOSITE_BISHOPS = 1 << 0
OWN_BISHOP_PAIR, ENEMY_BISHOP_PAIR, BLACK_TO_MOVE = 1 << 2, 1 << 3, 1 << 5
WHITE, BLACK = 0, 1
SECTION_NAMES = ["PAWNS", "KNIGHT_BONUS", "BISHOP_BONUS", "ROOK_BONUS", "QUEEN_BONUS",
                 "KNIGHT_MOB", "BISHOP_MOB", "ROOK_MOB", "QUEEN_MOB",
                 "KING_SAFETY", "THREATS", "PASSERS"]


def _trunc_div(a: np.ndarray, b: int) -> np.ndarray:
    """C++ integer division truncates toward zero; numpy floors."""
    return np.sign(a) * (np.abs(a) // b)


def _white_black(rows: np.ndarray, own, enemy):
    """Records are side-to-move relative; scale_factor is not."""
    black_to_move = (rows["context"]["flags"] & BLACK_TO_MOVE) != 0
    return (np.where(black_to_move, enemy, own).astype(np.int64),
            np.where(black_to_move, own, enemy).astype(np.int64))


def scale_factor_numpy(eg: np.ndarray, rows: np.ndarray) -> np.ndarray:
    """Eval::scale_factor, which reads `eg` only through the sign that picks the
    stronger side. That is the whole reason a leaner evaluation can move it: the
    branches are otherwise pure piece counts."""
    context = rows["context"]
    pawns_w, pawns_b = _white_black(rows, context["own_pawns"], context["enemy_pawns"])
    knights_w, knights_b = _white_black(rows, context["own_knights"], context["enemy_knights"])
    bishops_w, bishops_b = _white_black(rows, context["own_bishops"], context["enemy_bishops"])
    rooks_w, rooks_b = _white_black(rows, context["own_rooks"], context["enemy_rooks"])
    queens_w, queens_b = _white_black(rows, context["queens"] & 15, context["queens"] >> 4)
    npm_w = (knights_w * KNIGHT_VALUE + bishops_w * BISHOP_VALUE
             + rooks_w * ROOK_VALUE + queens_w * QUEEN_VALUE)
    npm_b = (knights_b * KNIGHT_VALUE + bishops_b * BISHOP_VALUE
             + rooks_b * ROOK_VALUE + queens_b * QUEEN_VALUE)

    white_strong = eg > 0
    pawns_strong = np.where(white_strong, pawns_w, pawns_b)
    diff = (np.where(white_strong, npm_w, npm_b) - np.where(white_strong, npm_b, npm_w))

    result = np.full(len(rows), SCALE_NORMAL, dtype=np.int64)
    few = pawns_strong <= 3
    result[few] = SCALE_NORMAL - 8 * (4 - pawns_strong[few])
    one_pawn = (pawns_strong == 1) & (diff <= BISHOP_VALUE)
    result[one_pawn] = SCALE_ONE_PAWN
    no_pawns = pawns_strong == 0
    result[no_pawns & (diff <= ROOK_VALUE)] = 12
    result[no_pawns & (diff <= BISHOP_VALUE)] = SCALE_DRAW
    # The opposite-bishop fortress short-circuits before any of that, and its
    # answer does not depend on which side is stronger.
    opposite = (context["flags"] & OPPOSITE_BISHOPS) != 0
    result[opposite] = 38
    result[opposite & (npm_w == BISHOP_VALUE) & (npm_b == BISHOP_VALUE)] = SCALE_OPPOSITE_BISHOPS
    return result


def replay_numpy(rows: np.ndarray, keep, with_space: bool):
    """Side-to-move-relative aggregate over a subset of sections."""
    context = rows["context"]
    mg = rows["psqt"][:, 0].astype(np.int64)
    eg = rows["psqt"][:, 1].astype(np.int64)
    section = rows["section"].astype(np.int64)
    for index in keep:
        mg += section[:, index, WHITE, 0] - section[:, index, BLACK, 0]
        eg += section[:, index, WHITE, 1] - section[:, index, BLACK, 1]
    if with_space:
        mg += (rows["space"][:, WHITE].astype(np.int64)
               - rows["space"][:, BLACK].astype(np.int64))
    # The bishop pair is piece counts only, so it stays whatever the sections do.
    black_to_move = (context["flags"] & BLACK_TO_MOVE) != 0
    own = (context["flags"] & OWN_BISHOP_PAIR) != 0
    enemy = (context["flags"] & ENEMY_BISHOP_PAIR) != 0
    pair = (np.where(black_to_move, enemy, own).astype(np.int64)
            - np.where(black_to_move, own, enemy).astype(np.int64))
    mg += BISHOP_PAIR_MG * pair
    eg += BISHOP_PAIR_EG * pair

    phase = context["phase"].astype(np.int64)
    # scale_factor reads the endgame half this evaluation produced, not the one
    # the full evaluation produced, so a leaner section set moves it too.
    scale = scale_factor_numpy(eg, rows)
    value = _trunc_div(
        mg * phase + _trunc_div(eg * (PHASE_MIDGAME - phase) * scale, SCALE_NORMAL),
        PHASE_MIDGAME)
    value = _trunc_div(value * (100 - context["rule50"].astype(np.int64)), 100)
    return np.where(black_to_move, -value, value) + TEMPO, scale


def build(records: Path, cache: Path, out: Path, keep, with_space: bool,
          chunk: int = 500_000) -> dict:
    views, meta = open_cache(cache)
    rows_total = meta["rows"]
    raw = np.memmap(records, mode="r", dtype=SECTION_DTYPE)
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as handle:
        handle.truncate(rows_total * 4)
    sidecar = np.memmap(out, mode="r+", dtype=np.uint8, shape=(rows_total, 4))

    written = 0
    replay_error = scale_error = 0
    moved_scale = 0
    for start in range(0, len(raw), chunk):
        block = np.array(raw[start:start + chunk])
        good = (((block["context"]["flags"] & IN_CHECK) == 0)
                & ((block["section_flags"] & KNOWN_ENDGAME) == 0))
        block = block[good]
        if not len(block):
            continue
        end = written + len(block)
        if end > rows_total:
            raise RuntimeError(f"more kept rows than the cache has: {end} > {rows_total}")

        # Gate: with every section kept, the replay must reproduce both the
        # stored score and the stored scale factor. If it does not, the
        # arithmetic here is not Boring's and nothing below can be trusted.
        full_value, full_scale = replay_numpy(block, range(12), with_space=True)
        replay_error = max(replay_error,
                           int(np.abs(full_value - block["context"]["score_cp"]).max()))
        scale_error += int((full_scale != block["scale"]).sum())

        lean_value, lean_scale = replay_numpy(block, keep, with_space)
        moved_scale += int((lean_scale != block["scale"]).sum())
        patched = block.copy()
        patched["context"]["score_cp"] = np.clip(lean_value, -32768, 32767).astype(np.int16)
        patched["scale"] = lean_scale.astype(np.uint8)
        odd, even = section_features(patched)
        sign, odd_bins, even_bins = quantize_sections(odd, even)

        # Only the aggregate and the scale may move. Anything else means a
        # feature depends on them in a way this splice would silently corrupt.
        base_odd, base_even = section_features(block)
        _, base_odd_bins, base_even_bins = quantize_sections(base_odd, base_even)
        moved_odd = list(np.flatnonzero((odd_bins != base_odd_bins).any(axis=0)))
        moved_even = list(np.flatnonzero((even_bins != base_even_bins).any(axis=0)))
        if not set(moved_odd) <= {0} or not set(moved_even) <= {0, 3}:
            raise RuntimeError(
                f"the lean evaluation moved odd columns {moved_odd} and even columns "
                f"{moved_even}; only the aggregate (0) and the scale (even 3) may move")

        sidecar[written:end, 0] = sign[:, 0].astype(np.uint8)
        sidecar[written:end, 1] = odd_bins[:, 0]
        sidecar[written:end, 2] = even_bins[:, 0]
        sidecar[written:end, 3] = even_bins[:, 3]
        written = end
        print(f"  {written:,}/{rows_total:,} rows", flush=True)

    if written != rows_total:
        raise RuntimeError(f"kept {written} rows, cache has {rows_total}")
    # Sections are packed as int16, so an extreme one saturates and the endgame
    # half reconstructed here can differ from the one Boring actually tapered
    # with. That flips which side scale_factor calls stronger on a handful of
    # rows. Measured at 3 in 183,374; anything beyond a rounding rate means the
    # port is wrong rather than the record lossy.
    if scale_error > max(64, rows_total // 10_000):
        raise RuntimeError(
            f"scale_factor port disagrees with the record on {scale_error:,} of "
            f"{rows_total:,} rows -- too many to be int16 saturation")
    sidecar.flush()
    info = {"rows": rows_total, "cache": str(cache), "keep": sorted(int(k) for k in keep),
            "keep_names": [SECTION_NAMES[k] for k in sorted(keep)],
            "with_space": with_space, "full_replay_max_error": replay_error,
            "scale_port_mismatched_rows": scale_error,
            "scale_factor_rows_changed": moved_scale}
    out.with_suffix(out.suffix + ".json").write_text(json.dumps(info) + "\n")
    return info


def load(path: Path, rows: int) -> np.ndarray:
    info = json.loads(path.with_suffix(path.suffix + ".json").read_text())
    if info["rows"] < rows:
        raise RuntimeError(f"aggregate sidecar {path} has {info['rows']} rows, need {rows}")
    return np.memmap(path, mode="r", dtype=np.uint8, shape=(info["rows"], 4))[:rows]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--records", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--keep", type=str, default="0",
                        help="comma-separated section indices the lean eval still computes")
    parser.add_argument("--with-space", action="store_true")
    args = parser.parse_args()
    keep = [int(x) for x in args.keep.split(",") if x.strip() != ""]
    info = build(args.records, args.cache, args.out, keep, args.with_space)
    print(f"{args.out}: {info['rows']:,} rows, keeping {info['keep_names']}, "
          f"full-replay max error {info['full_replay_max_error']}, "
          f"scale factor changed on {info['scale_factor_rows_changed']:,} rows, "
          f"port disagreed on {info['scale_port_mismatched_rows']}")


if __name__ == "__main__":
    main()
