#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

import numpy as np
import torch

from .calibration_model import expected_logit_from_factors
from .export_factor_runtime import dequantized_model, route_tables
from .train_section_lattice import CachedCorpus, Corpus


# Accepts both the classic `forgeeval` line and the FORGE-Graph era line, which
# inserts `base N graphbase N` and a `critical .. expertactive ..` block. The
# capture-group numbering is unchanged (the extra fields are non-capturing).
OUTPUT = re.compile(
    r"forge exact 0 boring (-?\d+) score (-?\d+)(?: base -?\d+ graphbase -?\d+)? "
    r"searchscore (-?\d+) advantage (\S+) decisive (\S+) wdl (\S+) (\S+) (\S+)"
    r"(?: critical \S+ expertgate \S+ expertresidual \S+ symmetryfix \S+ expertactive \S+)?"
    r" route (\d+)$"
)


def applicable_fens(path: Path, source_indices: np.ndarray, wanted: int) -> list[str]:
    selected = iter(int(value) for value in source_indices[:wanted])
    try:
        next_index = next(selected)
    except StopIteration:
        return []
    result: list[str] = []
    with path.open() as handle:
        for index, line in enumerate(handle):
            if index != next_index:
                continue
            fen = line.strip()
            if fen.startswith("fen "):
                fen = fen[4:]
            result.append(fen)
            try:
                next_index = next(selected)
            except StopIteration:
                break
    return result


def read_until(process: subprocess.Popen[str], predicate) -> str:
    assert process.stdout is not None
    while True:
        line = process.stdout.readline()
        if not line:
            raise RuntimeError("engine exited before expected response")
        stripped = line.strip()
        if predicate(stripped):
            return stripped


def round_away(values: np.ndarray) -> np.ndarray:
    return np.sign(values) * np.floor(np.abs(values) + 0.5)


def main() -> None:
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[2]
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--net", type=Path, required=True)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--sections", type=Path, default=Path("/tmp/forge-final-sections.bin"))
    # RECONSTRUCTED 2026-08-26: these two options existed as uncommitted local
    # changes and were lost to a `git checkout` during a debugging session.
    # A king-aware net needs the piece-square planes and the router cells, which
    # the plain sections Corpus does not carry.
    parser.add_argument("--bincache", type=Path,
                        help="precomputed bin cache; required for king-aware nets")
    parser.add_argument("--cells", type=Path,
                        help="router cell assignments matching --bincache")
    parser.add_argument("--targets", type=Path, default=root / "fens_data/data/val.wdl16.bin")
    parser.add_argument("--fens", type=Path, default=root / "fens_data/data/val.fens")
    parser.add_argument("--positions", type=int, default=10_000)
    parser.add_argument("--batch", type=int, default=32768)
    args = parser.parse_args()

    if args.bincache:
        corpus = CachedCorpus(args.bincache, with_psq=True, cells=args.cells)
        # A learned router indexes by cell; without the checkpoint's LUTs the
        # corpus falls back to the old cascade and every route disagrees.
        lut_a, lut_c = route_tables(args.checkpoint)
        if lut_a is not None:
            corpus.set_router(lut_a, lut_c)
        source = Corpus(args.sections, args.targets)
        wanted = min(args.positions, corpus.rows, source.rows)
    else:
        corpus = Corpus(args.sections, args.targets)
        source = corpus
        wanted = min(args.positions, corpus.rows)
    fens = applicable_fens(args.fens, source.source_indices, wanted)
    if len(fens) != wanted:
        raise RuntimeError(f"wanted {wanted} FENs, found {len(fens)}")
    model = dequantized_model(args.checkpoint).eval()
    with torch.no_grad():
        *inputs, _ = corpus.batch(slice(0, wanted), torch.device("cpu"))
        log_wdl, advantage, decisive = model(*inputs)
        probability = log_wdl.exp().numpy()
        score = round_away(
            (187.0 * expected_logit_from_factors(advantage, decisive)).numpy()
        ).astype(np.int64)
        expected_route = inputs[0].numpy()
        advantage = advantage.numpy()
        decisive = decisive.numpy()

    process = subprocess.Popen(
        [str(args.engine)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert process.stdin is not None
    try:
        process.stdin.write("uci\n")
        process.stdin.flush()
        read_until(process, lambda line: line == "uciok")
        process.stdin.write(f"setoption name NetPath value {args.net}\nisready\n")
        process.stdin.flush()
        read_until(process, lambda line: line == "readyok")

        score_mismatches = 0
        search_score_mismatches = 0
        route_mismatches = 0
        maximum_advantage = 0.0
        maximum_decisive = 0.0
        maximum_probability = 0.0
        maximum_score = 0
        for index, fen in enumerate(fens):
            process.stdin.write(f"position fen {fen}\nforgeeval\n")
            process.stdin.flush()
            line = read_until(process, lambda value: value.startswith("forge exact"))
            match = OUTPUT.fullmatch(line)
            if not match:
                raise RuntimeError(f"unexpected engine response: {line}")
            actual_score = int(match.group(2))
            actual_search_score = int(match.group(3))
            actual_advantage = float(match.group(4))
            actual_decisive = float(match.group(5))
            actual_probability = np.asarray(
                [float(match.group(6)), float(match.group(7)), float(match.group(8))]
            )
            actual_route = int(match.group(9))
            maximum_advantage = max(
                maximum_advantage, abs(actual_advantage - advantage[index])
            )
            maximum_decisive = max(
                maximum_decisive, abs(actual_decisive - decisive[index])
            )
            maximum_probability = max(
                maximum_probability,
                float(np.max(np.abs(actual_probability - probability[index]))),
            )
            maximum_score = max(maximum_score, abs(actual_score - int(score[index])))
            score_mismatches += actual_score != int(score[index])
            search_score_mismatches += actual_search_score != int(score[index])
            route_mismatches += actual_route != int(expected_route[index])
    finally:
        if process.poll() is None:
            process.stdin.write("quit\n")
            process.stdin.flush()
            process.wait(timeout=5)

    print(
        f"positions={wanted} score_mismatches={score_mismatches} "
        f"search_score_mismatches={search_score_mismatches} "
        f"route_mismatches={route_mismatches} score_max={maximum_score} "
        f"advantage_max={maximum_advantage:.9g} decisive_max={maximum_decisive:.9g} "
        f"probability_max={maximum_probability:.9g}"
    )

    # ---- parity receipt -------------------------------------------------
    # Written only on a clean pass, keyed by the exact binary+net hashes tested.
    # tools/match/parity_gate.sh refuses to start a match without a matching
    # receipt, so a candidate runtime cannot reach games unverified.
    def _sha(path):
        digest = hashlib.sha256()
        with open(path, "rb") as handle:
            for chunk in iter(lambda: handle.read(1 << 20), b""):
                digest.update(chunk)
        return digest.hexdigest()

    clean = (score_mismatches == 0 and search_score_mismatches == 0 and route_mismatches == 0
             and maximum_advantage < 1e-3 and maximum_decisive < 1e-3)
    receipts = Path(__file__).resolve().parents[2] / "results" / "parity"
    receipts.mkdir(parents=True, exist_ok=True)
    engine_hash, net_hash = _sha(args.engine), _sha(args.net)
    out = receipts / f"{engine_hash[:16]}_{net_hash[:16]}.json"
    out.write_text(json.dumps({
        "engine": str(args.engine), "engine_sha256": engine_hash,
        "net": str(args.net), "net_sha256": net_hash,
        "checkpoint": str(args.checkpoint), "positions": wanted,
        "score_mismatches": score_mismatches,
        "search_score_mismatches": search_score_mismatches,
        "route_mismatches": route_mismatches,
        "advantage_max": float(maximum_advantage),
        "decisive_max": float(maximum_decisive),
        "pass": bool(clean),
    }, indent=1))
    print(f"parity receipt {'PASS' if clean else 'FAIL'} -> {out}")
    if not clean:
        raise SystemExit("parity FAILED; this runtime must not play games")


if __name__ == "__main__":
    main()
