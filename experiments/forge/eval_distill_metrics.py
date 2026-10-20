#!/usr/bin/env python3
"""Search-aligned validation metrics for FORGE Mini checkpoints.

Validation cross-entropy is not a usable selector here: across the two King32
arms it preferred the net that lost by 20.9 Elo.  What search consumes is a
scalar evaluation, through move ordering, alpha/beta comparisons and absolute
centipawn thresholds, so the metrics below measure ordering and move choice
directly and keep the calibration numbers only as guardrails.

Every metric is computed on data held out from the distillation training sets:

  deep ordering    random position pairs from the 10K deep-validation roots,
                   split by |deep-score gap| because a pooled figure is
                   dominated by pairs no model gets wrong.
  sibling          does the evaluator rank deep search's move above the move a
                   25K-node search chose, on the 10K validation roots.
  move ranking     where does deep search's move sit among all eligible legal
                   children, by top-1, MRR and top-k.
  scale            evaluation spread, as a drift guard.  W = p95 - p5 is the
                   pre-registered width: p50 sits near zero, so its percentage
                   change is meaningless.

The functions are split so the training loop can call them against corpora it
already holds open, rather than reopening caches every epoch.

Usage:
    python3 -m experiments.forge.eval_distill_metrics \\
        --checkpoint ../forge_training/king-aware/run-king32-aux/last.pt \\
        [--label aux-last] [--device cpu] [--json out.json]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import torch

from .calibration_model import expected_logit_from_factors
from .section_lattice_model import RoutedFactorLattice
from .train_section_lattice import CachedCorpus, DisagreementCorpus

READOUT = 187.0
GAP_BANDS = ((1, 25), (25, 100), (100, 400), (400, 10**9))
DISTILL = Path("results/next_strength/distill")
DEEP_VAL = Path("results/next_strength/disagreement/cache_validation")
MULTIPV_VAL = Path("results/next_strength/multipv/dataset/val")


# ----------------------------------------------------------------- loading

def load_model(path: Path, device: torch.device):
    checkpoint = torch.load(path, map_location="cpu", weights_only=False)
    model = RoutedFactorLattice(width=8, psq_width=16, king_buckets=32)
    model.load_state_dict(checkpoint["model"])
    model.eval().to(device)
    lut = None
    if "route_lut_a" in checkpoint:
        lut = (np.asarray(checkpoint["route_lut_a"], dtype=np.uint8).copy(),
               np.asarray(checkpoint["route_lut_c"], dtype=np.uint8).copy())
    return model, lut


# Pre-binned widened-trace sidecars, one per held-out cache. Used only when an
# arm reads extra columns; None otherwise, so default behaviour is unchanged.
WIDE_DIR = Path("../forge_training/wide")

def _wide(key: str):
    from .section_lattice_model import WIDE_KEEP
    if not WIDE_KEEP:
        return None
    path = WIDE_DIR / f"{key}.widebins"
    if not path.exists():
        raise RuntimeError(f"widened arm needs {path}; run widen_aux.sh")
    return path


def open_suite(distill: Path = DISTILL, deep_val: Path = DEEP_VAL) -> dict:
    """Open every held-out cache once; the training loop reuses the result."""
    moverank = distill / "moverank_val"
    return {
        "deep": DisagreementCorpus(deep_val / "positions.psq.bincache",
                                   deep_val / "positions.deep.npy",
                                   deep_val / "positions.psq.cells",
                                   wide=_wide("disagreement_cache_validation")),
        "sibling": CachedCorpus(distill / "sibling_val" / "positions.psq.bincache", 0,
                                with_psq=True,
                                cells=distill / "sibling_val" / "positions.psq.cells",
                                wide=_wide("distill_sibling_val")),
        "moverank": CachedCorpus(moverank / "positions.psq.bincache", 0,
                                 with_psq=True, cells=moverank / "positions.psq.cells",
                                 wide=_wide("distill_moverank_val")),
        "moverank_offsets": np.load(moverank / "offsets.npy"),
        "moverank_best": np.load(moverank / "best_index.npy"),
        "multipv": CachedCorpus(MULTIPV_VAL / "positions.psq.bincache", 0,
                                with_psq=True,
                                cells=MULTIPV_VAL / "positions.psq.cells",
                                wide=_wide("multipv_dataset_val"))
                   if (MULTIPV_VAL / "positions.psq.bincache").exists() else None,
        "multipv_target": (np.load(MULTIPV_VAL / "positions.deep.npy")
                           if (MULTIPV_VAL / "positions.deep.npy").exists() else None),
        "multipv_root": (np.load(MULTIPV_VAL / "root_id.npy")
                         if (MULTIPV_VAL / "root_id.npy").exists() else None),
    }


def set_router(suite: dict, lut_a, lut_c) -> None:
    suite["deep"].set_router(lut_a, lut_c)
    suite["sibling"].set_router(lut_a, lut_c)
    suite["moverank"].set_router(lut_a, lut_c)
    if suite.get("multipv") is not None:
        suite["multipv"].set_router(lut_a, lut_c)


# ------------------------------------------------------------- primitives

@torch.no_grad()
def evaluations(model, corpus, device: torch.device, batch: int = 8192) -> np.ndarray:
    """Centipawn-scale evaluation for every row, side-to-move relative."""
    out = []
    for start in range(0, corpus.rows, batch):
        rows = slice(start, min(start + batch, corpus.rows))
        *inputs, _ = corpus.batch(rows, device)
        _log_wdl, advantage, decisive = model(*inputs)
        out.append((READOUT * expected_logit_from_factors(advantage, decisive)).cpu())
    return torch.cat(out).numpy()


def ordering_metrics(predicted: np.ndarray, deep: np.ndarray,
                     pairs: int = 2_000_000, seed: int = 20260823) -> dict:
    rng = np.random.default_rng(seed)
    a = rng.integers(0, len(predicted), pairs)
    b = rng.integers(0, len(predicted), pairs)
    keep = a != b
    a, b = a[keep], b[keep]
    gap = deep[a] - deep[b]
    agree = np.sign(gap) == np.sign(predicted[a] - predicted[b])
    result = {"ordering_all": float(agree[gap != 0].mean())}
    for low, high in GAP_BANDS:
        mask = (np.abs(gap) >= low) & (np.abs(gap) < high)
        name = f"ordering_{low}_{high}" if high < 10**9 else f"ordering_{low}_plus"
        result[name] = float(agree[mask].mean()) if mask.sum() else float("nan")
    return result


def sibling_metrics(child: np.ndarray) -> dict:
    """Rows are interleaved [deep_child, shallow_child] per pair.

    U(m) = -E(child(m)): the child's score belongs to the opponent, so the
    parent-facing advantage of deep's move over shallow's is E(c_s) - E(c_d).
    """
    advantage = child[1::2] - child[0::2]
    result = {
        "sibling_accuracy": float((advantage > 0).mean()),
        "sibling_median_cp": float(np.median(advantage)),
        "sibling_pairs": int(len(advantage)),
    }
    for margin in (0, 2, 5):
        result[f"sibling_violations_{margin}cp"] = float((advantage <= margin).mean())
    return result


def moverank_metrics(child: np.ndarray, offsets: np.ndarray, best: np.ndarray) -> dict:
    ranks = np.empty(len(offsets), dtype=np.int64)
    for index, (start, count) in enumerate(offsets):
        # Lower child score is better for the parent, so rank ascending.
        block = child[start:start + count]
        ranks[index] = 1 + int((block < block[best[index]]).sum())
    return {
        "moverank_parents": int(len(ranks)),
        "moverank_top1": float((ranks == 1).mean()),
        "moverank_top3": float((ranks <= 3).mean()),
        "moverank_top5": float((ranks <= 5).mean()),
        "moverank_mrr": float((1.0 / ranks).mean()),
        "moverank_median": float(np.median(ranks)),
    }


def multipv_metrics(child: np.ndarray, target: np.ndarray, root_id: np.ndarray,
                    bootstrap: int = 400, seed: int = 20260823) -> dict:
    """Accuracy against move values the search actually measured.

    `child` is the evaluator's score for each child position and `target` the
    searched value already negated into the child's frame, so they are directly
    comparable.  Ordering is scored only among moves of the *same* root, which
    is the comparison a search makes.

    Intervals bootstrap over roots.  Four children of one root are four labels
    from one search, not four independent observations, and treating them as
    independent is what made every earlier interval in this program too narrow.
    """
    error = np.abs(child - target)
    roots, inverse = np.unique(root_id, return_inverse=True)

    # Ordering, within root only, split by the gap the search actually measured.
    order = np.lexsort((target, root_id))
    bands = ((0, 10), (10, 25), (25, 50), (50, 100), (100, 10 ** 9))
    hits = {band: [0, 0] for band in bands}
    correct_by_root = np.zeros(len(roots))
    total_by_root = np.zeros(len(roots))
    start = 0
    while start < len(order):
        stop = start
        while stop < len(order) and root_id[order[stop]] == root_id[order[start]]:
            stop += 1
        block = order[start:stop]
        for i in range(len(block)):
            for j in range(i + 1, len(block)):
                a, b = block[i], block[j]
                gap = abs(target[a] - target[b])
                agree = np.sign(target[a] - target[b]) == np.sign(child[a] - child[b])
                for band in bands:
                    if band[0] <= gap < band[1]:
                        hits[band][0] += int(agree)
                        hits[band][1] += 1
                        break
                correct_by_root[inverse[a]] += int(agree)
                total_by_root[inverse[a]] += 1
        start = stop

    rng = np.random.default_rng(seed)
    samples = []
    for _ in range(bootstrap):
        pick = rng.integers(0, len(roots), len(roots))
        denominator = total_by_root[pick].sum()
        if denominator:
            samples.append(correct_by_root[pick].sum() / denominator)
    overall = correct_by_root.sum() / max(1.0, total_by_root.sum())

    result = {
        "multipv_rows": int(len(child)),
        "multipv_roots": int(len(roots)),
        "multipv_mae": float(error.mean()),
        "multipv_median_ae": float(np.median(error)),
        "multipv_pair_accuracy": float(overall),
        "multipv_pair_ci_lo": float(np.percentile(samples, 2.5)) if samples else float("nan"),
        "multipv_pair_ci_hi": float(np.percentile(samples, 97.5)) if samples else float("nan"),
    }
    for (low, high), (good, total) in hits.items():
        name = f"multipv_pair_{low}_{high}" if high < 10 ** 9 else f"multipv_pair_{low}_plus"
        result[name] = float(good / total) if total else float("nan")
        result[name + "_n"] = int(total)
    return result


def scale_metrics(predicted: np.ndarray) -> dict:
    p5, p50, p95 = np.percentile(predicted, (5, 50, 95))
    return {
        "scale_mean": float(predicted.mean()),
        "scale_std": float(predicted.std()),
        "scale_p5": float(p5),
        "scale_p50": float(p50),
        "scale_p95": float(p95),
        "scale_width": float(p95 - p5),
    }


def full_suite(model, suite: dict, device: torch.device,
               pairs: int = 2_000_000) -> dict:
    """Every held-out metric, from corpora that are already open."""
    deep_predicted = evaluations(model, suite["deep"].base, device)
    metrics = ordering_metrics(deep_predicted,
                               np.asarray(suite["deep"].deep, dtype=np.float64), pairs)
    metrics.update(scale_metrics(deep_predicted))
    metrics.update(sibling_metrics(evaluations(model, suite["sibling"], device)))
    metrics.update(moverank_metrics(evaluations(model, suite["moverank"], device),
                                    suite["moverank_offsets"], suite["moverank_best"]))
    if suite.get("multipv") is not None:
        metrics.update(multipv_metrics(evaluations(model, suite["multipv"], device),
                                       suite["multipv_target"], suite["multipv_root"]))
    return metrics


# ------------------------------------------------------------------- CLI

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path, required=True, action="append")
    parser.add_argument("--label", action="append", default=[])
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--pairs", type=int, default=2_000_000)
    parser.add_argument("--distill-dir", type=Path, default=DISTILL)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    device = torch.device(args.device)
    suite = open_suite(args.distill_dir)
    results = {}
    for index, path in enumerate(args.checkpoint):
        label = (args.label[index] if index < len(args.label)
                 else f"{path.parent.name}/{path.name}")
        model, lut = load_model(path, device)
        if lut is not None:
            set_router(suite, *lut)
        metrics = full_suite(model, suite, device, args.pairs)
        results[label] = metrics
        print(f"\n=== {label} ===")
        for key, value in metrics.items():
            if (key.endswith(("pairs", "parents", "median", "_cp", "rows", "roots",
                              "_n", "mae", "median_ae"))
                    or key.startswith("scale")):
                print(f"  {key:<28} {value:>12.4g}")
            else:
                print(f"  {key:<28} {100 * value:>11.2f}%")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")
        print(f"\nwrote {args.json}")


if __name__ == "__main__":
    main()
