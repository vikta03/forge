#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import math
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F

from .calibration_model import IN_CHECK, expected_logit_from_factors, soft_wdl_loss
from .section_lattice_model import (
    WIDE_KEEP,
    WIDE_Q,
    EVEN_KEEP,
    LEAN,
    MINI,
    ODD_KEEP,
    PSQ_PLANES,
    RoutedFactorLattice,
    RoutedFamilyLattice,
    balanced_route_numpy,
    quantize_sections,
)
from .build_bin_cache import open_cache
from .build_cell_sidecar import load as load_cells
from .build_lean_agg import load as load_agg
from . import router_em
from .section_model import KNOWN_ENDGAME, SECTION_DTYPE, section_features


class Corpus:
    def __init__(
        self, records: Path, targets: Path, limit: int = 0, streaming: bool = False
    ) -> None:
        available = records.stat().st_size // SECTION_DTYPE.itemsize
        source_rows = min(available, limit) if limit else available
        raw = np.memmap(records, mode="r", dtype=SECTION_DTYPE, shape=(available,))[
            :source_rows
        ]
        target_rows = targets.stat().st_size // 6
        if available > target_rows:
            raise RuntimeError(f"record/target mismatch: {available} > {target_rows}")
        target_raw = np.memmap(
            targets, mode="r", dtype=np.float16, shape=(target_rows, 3)
        )[:source_rows]
        keep = ((raw["context"]["flags"] & IN_CHECK) == 0) & (
            (raw["section_flags"] & KNOWN_ENDGAME) == 0
        )
        self.streaming = streaming
        self.raw = raw
        self.target_raw = target_raw
        self.source_indices = np.flatnonzero(keep).astype(np.uint32)
        self.rows = len(self.source_indices)
        if streaming:
            # The complete float section matrix would exceed 20 GiB. Keep only
            # sorted source indices (~201 MiB) and encode each block on demand.
            self.odd_sign = self.odd_bins = self.even_bins = None
            self.route = None
            counts = np.zeros(8, dtype=np.int64)
            for start in range(0, self.rows, 1_000_000):
                source = np.asarray(
                    self.source_indices[start : start + 1_000_000], dtype=np.int64
                )
                counts += np.bincount(
                    balanced_route_numpy(raw["context"][source]), minlength=8
                )
            self.route_counts = counts.tolist()
        else:
            selected = raw[keep]
            odd, even = section_features(selected)
            self.odd_sign, self.odd_bins, self.even_bins = quantize_sections(odd, even)
            self.route = balanced_route_numpy(selected["context"])
            del odd, even
            self.targets = np.asarray(target_raw[keep], dtype=np.float32)
            self.targets /= np.maximum(self.targets.sum(axis=1, keepdims=True), 1e-12)
            self.route_counts = np.bincount(self.route, minlength=8).tolist()

    def batch(self, rows, device: torch.device):
        def tensor(array, dtype=None):
            selected = array if self.streaming else array[rows]
            value = torch.from_numpy(np.array(selected, copy=True))
            if dtype is not None:
                value = value.to(dtype)
            return value.to(device, non_blocking=True)

        if self.streaming:
            source = np.asarray(self.source_indices[rows], dtype=np.int64)
            selected = self.raw[source]
            odd, even = section_features(selected)
            odd_sign, odd_bins, even_bins = quantize_sections(odd, even)
            route = balanced_route_numpy(selected["context"])
            targets = np.asarray(self.target_raw[source], dtype=np.float32)
            targets /= np.maximum(targets.sum(axis=1, keepdims=True), 1e-12)
            arrays = (route, odd_sign, odd_bins, even_bins, targets)
        else:
            arrays = (
                self.route,
                self.odd_sign,
                self.odd_bins,
                self.even_bins,
                self.targets,
            )
        return (
            tensor(arrays[0], torch.long),
            tensor(arrays[1], torch.float32),
            tensor(arrays[2], torch.long),
            tensor(arrays[3], torch.long),
            tensor(arrays[4], torch.float32),
        )


class CachedCorpus:
    """Corpus backed by precomputed bins, with the same interface as Corpus.

    Every batch here is a gather and a host-to-device copy. The bins move as
    uint8 and are widened on the GPU, so the transfer is 137 bytes a row rather
    than eight times that as int64.
    """

    def __init__(self, path: Path, limit: int = 0, with_psq: bool = False,
                 cells: Path | None = None, agg: Path | None = None,
                 wide: Path | None = None, override: Path | None = None,
                 override_mix: float = 0.0) -> None:
        views, meta = open_cache(path)
        # Soft-label distillation: a float16 [rows, 3] WDL array aligned with
        # this cache's rows replaces a fraction `override_mix` of the outcome
        # target. Row alignment is the caller's contract (see
        # experiments/overhaul/keep_mask.py); the shape check is the guard.
        self.override = None
        self.override_mix = 0.0
        if override is not None and override_mix > 0.0:
            soft = np.load(override, mmap_mode="r")
            if soft.shape != (meta["rows"], 3):
                raise RuntimeError(f"{override} has shape {soft.shape}, cache has "
                                   f"{meta['rows']} rows")
            if not 0.0 < override_mix <= 1.0:
                raise RuntimeError("--targets-override-mix must be in (0, 1]")
            self.override = soft
            self.override_mix = float(override_mix)
        if with_psq and "sparse_idx" not in views:
            raise RuntimeError(f"{path} has no piece-square planes; rebuild it")
        self.with_psq = with_psq
        self.views = views
        self.rows = min(limit, meta["rows"]) if limit else meta["rows"]
        self.route_counts = meta["route_counts"]
        self.streaming = False
        # A learned router indexes by cell, not by the cascade's answer.
        self.cells = load_cells(cells, self.rows) if cells else None
        # A lean handcrafted eval changes the aggregate, so its two features
        # cannot be sliced out of the cache -- they come re-binned from here.
        self.agg = load_agg(agg, self.rows) if agg else None
        if LEAN and self.agg is None:
            raise RuntimeError("FORGE_LEAN needs --agg-train/--agg-val; the cached "
                               "aggregate was binned from the full handcrafted eval")
        if self.agg is not None and not (ODD_KEEP[0] == 0 and EVEN_KEEP[0] == 0
                                         and EVEN_KEEP[3] == 3):
            raise RuntimeError("the splice assumes the aggregate survives at column 0 "
                               "of each side and the endgame scale at even column 3")
        # Pre-binned widened-trace columns: 26 signs | 26 odd bins | 26 even bins.
        self.wide = None
        if WIDE_KEEP:
            if wide is None:
                raise RuntimeError("FORGE_WIDE_ARM is set but no --wide-* sidecar given")
            raw = np.memmap(wide, mode="r", dtype=np.uint8).reshape(-1, 3 * WIDE_Q)
            if len(raw) < self.rows:
                raise RuntimeError(f"{wide} has {len(raw)} rows, cache has {self.rows}")
            self.wide = raw
            self.wide_cols = np.asarray(WIDE_KEEP, dtype=np.int64)
        self.lut_a = self.lut_c = None

    def _targets(self, rows, device: torch.device):
        block = np.asarray(self.views["targets"][rows], dtype=np.float32)
        if self.override is not None:
            soft = np.asarray(self.override[rows], dtype=np.float32)
            block = (1.0 - self.override_mix) * block + self.override_mix * soft
        return torch.from_numpy(np.ascontiguousarray(block)).to(device, non_blocking=True)

    def set_router(self, lut_a, lut_c) -> None:
        if self.cells is None:
            raise RuntimeError("corpus has no cell sidecar; pass --cells-*")
        if not self.with_psq:
            raise RuntimeError(
                "the learned router feeds route_c as the seventh positional "
                "argument, which only lines up when the psq inputs are present")
        self.lut_a, self.lut_c = lut_a, lut_c

    def raw_batch(self, rows, device: torch.device):
        """Everything the model needs except the routes, which the caller supplies."""
        def take(array, dtype, keep=None, splice=(), wide_block=None):
            block = array[rows]
            # The cache is full width. Slice the contiguous rows first, then take
            # the kept columns from the small in-memory block: the quantizer is
            # per feature, so this is identical to having binned only these.
            if keep is not None:
                block = block[:, keep]
            # A leaner handcrafted eval changes the aggregate and, through the
            # sign of its endgame half, the scale factor. Those two cannot be
            # sliced out of a cache binned from the full eval.
            if splice and self.agg is not None:
                sidecar = self.agg[rows]
                block = np.array(block, copy=True)
                for source, target in splice:
                    block[:, target] = sidecar[:, source]
            # Widened columns are appended, never spliced: they are new
            # features, not replacements for existing ones.
            if wide_block is not None and self.wide is not None:
                extra = self.wide[rows][:, wide_block * WIDE_Q + self.wide_cols]
                if wide_block == 0:
                    extra = extra.view(np.int8)
                block = np.concatenate([np.asarray(block), extra.astype(block.dtype)], axis=1)
            host = torch.from_numpy(np.ascontiguousarray(block))
            return host.to(device, non_blocking=True).to(dtype)

        odd_keep = ODD_KEEP if (MINI or LEAN) else None
        even_keep = EVEN_KEEP if (MINI or LEAN) else None
        inputs = [
            take(self.views["odd_sign"], torch.float32, odd_keep, ((0, 0),), 0),
            take(self.views["odd_bins"], torch.long, odd_keep, ((1, 0),), 1),
            take(self.views["even_bins"], torch.long, even_keep, ((2, 0), (3, 3)), 2),
        ]
        if self.with_psq:
            inputs.append(take(self.views["sparse_idx"], torch.long))
            inputs.append(take(self.views["sparse_count"], torch.long))
        inputs.append(self._targets(rows, device))
        return tuple(inputs)
    def batch(self, rows, device: torch.device):
        def take(array, dtype, keep=None, splice=(), wide_block=None):
            block = array[rows]
            # The cache is full width. Slice the contiguous rows first, then take
            # the kept columns from the small in-memory block: the quantizer is
            # per feature, so this is identical to having binned only these.
            if keep is not None:
                block = block[:, keep]
            # A leaner handcrafted eval changes the aggregate and, through the
            # sign of its endgame half, the scale factor. Those two cannot be
            # sliced out of a cache binned from the full eval.
            if splice and self.agg is not None:
                sidecar = self.agg[rows]
                block = np.array(block, copy=True)
                for source, target in splice:
                    block[:, target] = sidecar[:, source]
            # Widened columns are appended, never spliced: they are new
            # features, not replacements for existing ones.
            if wide_block is not None and self.wide is not None:
                extra = self.wide[rows][:, wide_block * WIDE_Q + self.wide_cols]
                if wide_block == 0:
                    extra = extra.view(np.int8)
                block = np.concatenate([np.asarray(block), extra.astype(block.dtype)], axis=1)
            host = torch.from_numpy(np.ascontiguousarray(block))
            return host.to(device, non_blocking=True).to(dtype)

        if self.lut_a is not None:
            cell = take(self.cells, torch.long)
            lut_a = torch.as_tensor(self.lut_a, device=device, dtype=torch.long)
            lut_c = torch.as_tensor(self.lut_c, device=device, dtype=torch.long)
            *rest, target = self.raw_batch(rows, device)
            return (lut_a[cell], *rest, lut_c[cell], target)

        inputs = [
            take(self.views["route"], torch.long),
            take(self.views["odd_sign"], torch.float32, ODD_KEEP if (MINI or LEAN) else None, ((0, 0),), 0),
            take(self.views["odd_bins"], torch.long, ODD_KEEP if (MINI or LEAN) else None, ((1, 0),), 1),
            take(self.views["even_bins"], torch.long, EVEN_KEEP if (MINI or LEAN) else None, ((2, 0), (3, 3)), 2),
        ]
        if self.with_psq:
            inputs.append(take(self.views["sparse_idx"], torch.long))
            inputs.append(take(self.views["sparse_count"], torch.long))
        inputs.append(self._targets(rows, device))
        return tuple(inputs)


class DisagreementCorpus:
    """A regular PSQ bin cache paired with fresh-history deep search scores."""

    def __init__(self, cache: Path, deep_scores: Path, cells: Path | None = None,
                 wide: Path | None = None) -> None:
        self.base = CachedCorpus(cache, with_psq=True, cells=cells, wide=wide)
        self.deep = np.load(deep_scores, mmap_mode="r")
        if self.deep.shape != (self.base.rows,):
            raise RuntimeError(
                f"deep score shape {self.deep.shape} does not match {self.base.rows} cache rows")
        self.rows = self.base.rows

    def set_router(self, lut_a, lut_c) -> None:
        self.base.set_router(lut_a, lut_c)

    def batch(self, rows, device: torch.device):
        *inputs, _unused_wdl = self.base.batch(rows, device)
        deep = torch.from_numpy(np.asarray(self.deep[rows], dtype=np.float32).copy())
        return (*inputs, deep.to(device, non_blocking=True))


class PvTraversal:
    """Successive PV views of a root rather than the same root repeatedly.

    The auxiliary loop already cycles every root about sixteen times per epoch
    (201 steps x 4096 rows / 50k roots) and the median stored principal
    variation is fifteen plies, so a root can be shown a different descendant
    on almost every exposure.  Drawing a random ply instead would repeat some
    and never reach others, which throws away the whole reason the expansion
    is worth doing.

    Each root keeps a shuffled cursor over its own descendants and hands out
    the next unused one; the reshuffle draws from the auxiliary generator, so
    nothing here perturbs the main data order.
    """

    def __init__(self, root_id: np.ndarray, ply: np.ndarray,
                 keep: np.ndarray, rng: np.random.Generator) -> None:
        self.rng = rng
        roots = int(root_id.max()) + 1
        selected_root = root_id[keep]
        selected_ply = ply[keep]
        # Ply 0 is the root itself and is served from the original root cache,
        # so only genuine descendants belong in the traversal.
        descendant = selected_ply >= 1
        rows = np.asarray(keep, dtype=np.int64)[descendant]
        owner = selected_root[descendant]
        order = np.argsort(owner, kind="stable")
        rows, owner = rows[order], owner[order]
        bounds = np.searchsorted(owner, np.arange(roots + 1))
        self.table = [rows[bounds[r]:bounds[r + 1]] for r in range(roots)]
        self.permutation: list[np.ndarray | None] = [None] * roots
        self.cursor = np.zeros(roots, dtype=np.int64)

    def draw(self, roots: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
        """Next descendant per root; `available` is False where none exists."""
        out = np.empty(len(roots), dtype=np.int64)
        available = np.ones(len(roots), dtype=bool)
        for index, root in enumerate(roots):
            table = self.table[int(root)]
            if len(table) == 0:
                available[index] = False
                out[index] = -1
                continue
            if self.permutation[root] is None or self.cursor[root] >= len(table):
                self.permutation[root] = self.rng.permutation(len(table))
                self.cursor[root] = 0
            out[index] = table[self.permutation[root][self.cursor[root]]]
            self.cursor[root] += 1
        return out, available


class RootBalancedRows:
    """Draw a root, then one of that root's measured moves.

    Almost every root carries four MultiPV moves, so this is close to a flat
    draw over child rows -- but only close. Cross-split dedup and ineligible
    children leave some roots with fewer rows, and a flat draw would silently
    give those roots less weight. Making the root the sampling unit keeps the
    statistical meaning the same as the rest of the program, where the root
    search is the unit of independence.
    """

    def __init__(self, root_id: np.ndarray, rng: np.random.Generator) -> None:
        self.rng = rng
        order = np.argsort(root_id, kind="stable")
        rows = np.arange(len(root_id), dtype=np.int64)[order]
        owner = np.asarray(root_id)[order]
        self.roots, starts = np.unique(owner, return_index=True)
        bounds = list(starts) + [len(owner)]
        self.table = [rows[bounds[i]:bounds[i + 1]] for i in range(len(self.roots))]
        self.cursor = np.zeros(len(self.roots), dtype=np.int64)
        self.permutation: list[np.ndarray | None] = [None] * len(self.roots)
        self.root_order = rng.permutation(len(self.roots))
        self.root_cursor = 0

    def draw(self, count: int) -> np.ndarray:
        out = np.empty(count, dtype=np.int64)
        for index in range(count):
            if self.root_cursor >= len(self.root_order):
                self.root_order = self.rng.permutation(len(self.roots))
                self.root_cursor = 0
            slot = int(self.root_order[self.root_cursor])
            self.root_cursor += 1
            table = self.table[slot]
            if self.permutation[slot] is None or self.cursor[slot] >= len(table):
                self.permutation[slot] = self.rng.permutation(len(table))
                self.cursor[slot] = 0
            out[index] = table[self.permutation[slot][self.cursor[slot]]]
            self.cursor[slot] += 1
        return out


class SiblingCorpus:
    """Deep-best and shallow-best children of the same parent, interleaved.

    Row 2i is the child after deep search's move and row 2i+1 the child after
    the move a 25k-node search chose.  The parent-facing value of a move is
    -E(child), so preferring deep's move means E(deep child) < E(shallow
    child); the hinge is written against that directly.
    """

    def __init__(self, cache: Path, cells: Path | None,
                 keep_pairs: np.ndarray | None = None) -> None:
        self.base = CachedCorpus(cache, 0, with_psq=True, cells=cells)
        if self.base.rows % 2:
            raise RuntimeError(f"sibling cache has an odd row count: {self.base.rows}")
        total = self.base.rows // 2
        self.pairs = (np.arange(total, dtype=np.int64) if keep_pairs is None
                      else np.asarray(keep_pairs, dtype=np.int64))
        if len(self.pairs) and int(self.pairs.max()) >= total:
            raise RuntimeError("sibling keep-index out of range")
        self.rows = len(self.pairs)

    def set_router(self, lut_a, lut_c) -> None:
        self.base.set_router(lut_a, lut_c)

    def batch(self, pairs: np.ndarray, device: torch.device):
        rows = np.empty(2 * len(pairs), dtype=np.int64)
        rows[0::2] = 2 * pairs
        rows[1::2] = 2 * pairs + 1
        *inputs, _unused_wdl = self.base.batch(rows, device)
        return inputs


@torch.no_grad()
def evaluate(model, corpus: Corpus, batch: int, device: torch.device):
    model.eval()
    totals = dict(ce=0.0, brier=0.0, expected_mse=0.0, sign=0.0, sign_rows=0.0)
    for start in range(0, corpus.rows, batch):
        *inputs, target = corpus.batch(slice(start, min(start + batch, corpus.rows)), device)
        probability = model(*inputs)[0].exp()
        expected = probability[:, 0] + 0.5 * probability[:, 1]
        target_expected = target[:, 0] + 0.5 * target[:, 1]
        totals["ce"] += float((-(target * probability.log()).sum(dim=1)).sum())
        totals["brier"] += float(((probability - target) ** 2).sum())
        totals["expected_mse"] += float(((expected - target_expected) ** 2).sum())
        decisive = (target_expected - 0.5).abs() >= 0.05
        totals["sign"] += float(
            (((expected - 0.5) * (target_expected - 0.5) > 0) & decisive).sum()
        )
        totals["sign_rows"] += float(decisive.sum())
    result = {
        key: value / corpus.rows
        for key, value in totals.items()
        if key not in ("sign", "sign_rows")
    }
    result["sign_accuracy"] = totals["sign"] / max(1.0, totals["sign_rows"])
    return result


@torch.no_grad()
def evaluate_disagreement(model, corpus: DisagreementCorpus,
                          batch: int, device: torch.device) -> dict[str, float]:
    model.eval()
    absolute = 0.0
    squared = 0.0
    huber = 0.0
    for start in range(0, corpus.rows, batch):
        *inputs, deep = corpus.batch(slice(start, min(start + batch, corpus.rows)), device)
        _log_wdl, advantage, decisive = model(*inputs)
        predicted = 187.0 * expected_logit_from_factors(advantage, decisive)
        difference = predicted - deep
        absolute += float(difference.abs().sum())
        squared += float(difference.square().sum())
        huber += float(F.huber_loss(predicted / 187.0, deep / 187.0,
                                    delta=1.0, reduction="sum"))
    return {
        "mae": absolute / corpus.rows,
        "rmse": math.sqrt(squared / corpus.rows),
        "huber": huber / corpus.rows,
    }


def emit(handle, record) -> None:
    line = json.dumps(record, sort_keys=True)
    print(line, flush=True)
    handle.write(line + "\n")
    handle.flush()


def main() -> None:
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[2]
    data = root / "fens_data/data"
    parser.add_argument("--records-train", type=Path)
    parser.add_argument("--records-val", type=Path)
    parser.add_argument("--bincache-train", type=Path,
                        help="precomputed bins from build_bin_cache")
    parser.add_argument("--bincache-val", type=Path)
    parser.add_argument("--wide-train", type=Path,
                        help="pre-binned widened-trace sidecar for training")
    parser.add_argument("--wide-val", type=Path)
    parser.add_argument("--wide-multipv", type=Path,
                        help="pre-binned widened trace for the MultiPV metric set")
    parser.add_argument("--psq-width", type=int, default=0,
                        help="width of the piece-square embedding; 0 disables it")
    parser.add_argument("--king-buckets", type=int, choices=(1, 16, 32), default=1,
                        help="own-king buckets for the unified PSQ branch")
    parser.add_argument("--targets-train", type=Path, default=data / "train.wdl16.bin")
    parser.add_argument("--targets-val", type=Path, default=data / "val.wdl16.bin")
    parser.add_argument("--targets-override", type=Path,
                        help="float16 [rows,3] soft WDL labels aligned with the train bincache")
    parser.add_argument("--targets-override-mix", type=float, default=0.0,
                        help="fraction of the train target taken from --targets-override")
    parser.add_argument("--train-rows", type=int, default=2_000_000)
    parser.add_argument("--val-rows", type=int, default=0)
    parser.add_argument("--streaming", action="store_true")
    parser.add_argument(
        "--shuffle-block",
        type=int,
        default=0,
        help="shuffle contiguous batches inside shuffled blocks; required for large streaming corpora",
    )
    parser.add_argument(
        "--variant",
        choices=("global", "routed", "routed-pairs", "factor4", "factor8", "factor16"),
        default="routed-pairs",
    )
    parser.add_argument("--epochs", type=int, default=12)
    parser.add_argument("--batch", type=int, default=32768)
    parser.add_argument("--lr", type=float, default=3e-3)
    parser.add_argument("--weight-decay", type=float, default=1e-6)
    parser.add_argument("--smoothness", type=float, default=1e-4)
    parser.add_argument("--seed", type=int, default=20260814)
    parser.add_argument("--initial", type=Path)
    parser.add_argument("--cells-train", type=Path,
                        help="cell sidecar from build_cell_sidecar; enables the learned router")
    parser.add_argument("--cells-val", type=Path)
    parser.add_argument("--disagreement-train", type=Path,
                        help="stratified disagreement bin cache")
    parser.add_argument("--disagreement-val", type=Path)
    parser.add_argument("--disagreement-deep-train", type=Path)
    parser.add_argument("--disagreement-deep-val", type=Path)
    parser.add_argument("--disagreement-cells-train", type=Path)
    parser.add_argument("--disagreement-cells-val", type=Path)
    parser.add_argument("--disagreement-weight", type=float, default=0.0,
                        help="scalar search-distillation weight; 0 disables the aux step")
    parser.add_argument("--disagreement-every", type=int, default=8)
    parser.add_argument("--distill-dir", type=Path,
                        help="search-distillation datasets: pv_train, sibling_train, "
                             "sibling_val, moverank_val. Enables PV traversal, the "
                             "sibling objective and the held-out search metrics.")
    parser.add_argument("--pv-mix", type=float, default=0.0,
                        help="fraction of aux exposures taken from a PV descendant "
                             "rather than the root itself; 0 reproduces the root-only arm")
    parser.add_argument("--sibling-weight", type=float, default=0.0,
                        help="hinge weight for deep-best over shallow-best; 0 disables")
    parser.add_argument("--sibling-margin", type=float, default=0.0,
                        help="hinge margin in logits (1 logit = 187 cp, so 2 cp = 0.0107)")
    parser.add_argument("--search-gate-width", type=float, default=0.05,
                        help="allowed |change| in evaluation width p95-p5 before "
                             "best_search.pt is withheld. 0.05 suits a two-epoch "
                             "probe; a full arm legitimately moves much further "
                             "(the strongest net on record widened 44 percent), so "
                             "raise "
                             "it there rather than losing the checkpoint")
    parser.add_argument("--search-gate-calibration", type=float, default=0.02,
                        help="allowed fractional degradation in Brier and "
                             "expected-score MSE")
    parser.add_argument("--multipv-dir", type=Path,
                        help="MultiPV child cache from build_multipv_dataset. Its rows "
                             "carry searched move values, already negated into the "
                             "child's own frame, so they slot into the existing scalar "
                             "auxiliary batch unchanged.")
    parser.add_argument("--multipv-mix", type=float, default=0.0,
                        help="fraction of the auxiliary batch drawn from measured "
                             "MultiPV children rather than root positions. This is the "
                             "only knob: same loss, same batch size, same optimizer "
                             "step, so a result cannot be confounded with a second "
                             "coefficient.")
    parser.add_argument("--flip-mask", type=Path,
                        help="boolean npy, one entry per disagreement root, true where "
                             "the 25k and 500k searches chose different moves")
    parser.add_argument("--flip-emphasis", type=int, default=1,
                        help="how many times over a move-flip root enters the auxiliary "
                             "sampling pool. 1 leaves sampling uniform; 2 raises flip "
                             "roots from 27 to 42 percent of draws, 4 to 60 percent. "
                             "This spends the objective that already produced Elo on "
                             "the positions where shallow search actually went wrong")
    parser.add_argument("--sibling-dir", type=Path,
                        help="sibling pair cache; defaults to <distill-dir>/sibling_train. "
                             "Point at pv_sibling_train to use the PV-wide set instead of "
                             "the root-only pairs, which memorised rather than generalised")
    parser.add_argument("--sibling-batch", type=int, default=96,
                        help="pairs per aux step; 64-128 gives 1-2 sweeps of the set "
                             "per epoch, which avoids memorising 12k pairs")
    parser.add_argument("--disagreement-batch", type=int, default=4096)
    parser.add_argument("--agg-train", type=Path,
                        help="re-binned aggregate from build_lean_agg; required by FORGE_LEAN")
    parser.add_argument("--agg-val", type=Path)
    parser.add_argument("--router-em", type=str, default="",
                        help="comma-separated epochs after which to reassign cells")
    parser.add_argument("--router-split", action="store_true",
                        help="let the advantage and decisiveness heads route differently")
    parser.add_argument("--router-floor", type=float, default=0.04,
                        help="minimum share of corpus mass any route must keep")
    parser.add_argument("--router-stride", type=int, default=8,
                        help="reassignment samples one batch in N")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    if args.bincache_train:
        train = CachedCorpus(args.bincache_train, args.train_rows,
                             wide=args.wide_train,
                             with_psq=bool(args.psq_width), cells=args.cells_train,
                             agg=args.agg_train, override=args.targets_override,
                             override_mix=args.targets_override_mix)
    else:
        train = Corpus(
            args.records_train, args.targets_train, args.train_rows,
            streaming=args.streaming,
        )
    if args.bincache_val:
        val = CachedCorpus(args.bincache_val, args.val_rows, wide=args.wide_val,
                           with_psq=bool(args.psq_width), cells=args.cells_val,
                           agg=args.agg_val)
    else:
        val = Corpus(args.records_val, args.targets_val, args.val_rows)
    disagreement_train = disagreement_val = None
    if bool(args.disagreement_train) != bool(args.disagreement_val):
        parser.error("provide both --disagreement-train and --disagreement-val")
    if args.disagreement_train:
        if not args.disagreement_deep_train or not args.disagreement_deep_val:
            parser.error("disagreement caches require both deep-score sidecars")
        disagreement_train = DisagreementCorpus(
            args.disagreement_train, args.disagreement_deep_train,
            args.disagreement_cells_train)
        disagreement_val = DisagreementCorpus(
            args.disagreement_val, args.disagreement_deep_val,
            args.disagreement_cells_val)
    if args.disagreement_weight and disagreement_train is None:
        parser.error("the auxiliary arm requires disagreement caches")
    pv_train = pv_root_id = pv_ply = pv_keep = None
    sibling_train = None
    if args.distill_dir:
        if disagreement_train is None:
            parser.error("--distill-dir extends the auxiliary arm; pass the "
                         "disagreement caches too")
        pv_directory = args.distill_dir / "pv_train"
        pv_train = DisagreementCorpus(pv_directory / "positions.psq.bincache",
                                      pv_directory / "positions.deep.npy",
                                      pv_directory / "positions.psq.cells")
        pv_root_id = np.load(pv_directory / "root_id.npy")
        pv_ply = np.load(pv_directory / "ply.npy")
        keep_path = pv_directory / "keep_indices.npy"
        # keep_indices removes PV rows that also occur in the validation PVs.
        pv_keep = (np.load(keep_path) if keep_path.exists()
                   else np.arange(pv_train.rows, dtype=np.int64))
        sibling_directory = args.sibling_dir or (args.distill_dir / "sibling_train")
        dedup_path = sibling_directory / "dedup.json"
        sibling_keep = None
        if dedup_path.exists():
            sibling_keep = np.asarray(
                json.loads(dedup_path.read_text())["keep_indices"], dtype=np.int64)
        sibling_train = SiblingCorpus(
            sibling_directory / "positions.psq.bincache",
            sibling_directory / "positions.psq.cells", sibling_keep)
    flip_mask = None
    if args.flip_mask:
        flip_mask = np.load(args.flip_mask)
        if disagreement_train is None or flip_mask.shape != (disagreement_train.rows,):
            parser.error("--flip-mask must have one entry per disagreement root")
    if args.flip_emphasis < 1:
        parser.error("--flip-emphasis must be at least 1")
    if args.flip_emphasis > 1 and flip_mask is None:
        parser.error("--flip-emphasis requires --flip-mask")
    multipv_train = multipv_root_id = None
    if args.multipv_dir:
        multipv_train = DisagreementCorpus(
            args.multipv_dir / "positions.psq.bincache",
            args.multipv_dir / "positions.deep.npy",
            args.multipv_dir / "positions.psq.cells",
            wide=args.wide_multipv)
        multipv_root_id = np.load(args.multipv_dir / "root_id.npy")
    if args.multipv_mix and multipv_train is None:
        parser.error("--multipv-mix requires --multipv-dir")
    if not 0.0 <= args.multipv_mix <= 1.0:
        parser.error("--multipv-mix must be in [0, 1]")
    if args.pv_mix and pv_train is None:
        parser.error("--pv-mix requires --distill-dir")
    if args.sibling_weight and sibling_train is None:
        parser.error("--sibling-weight requires --distill-dir")
    if args.variant.startswith("factor"):
        model = RoutedFactorLattice(
            width=int(args.variant.removeprefix("factor")),
            psq_width=args.psq_width, king_buckets=args.king_buckets)
    else:
        model = RoutedFamilyLattice(
            use_routes=args.variant != "global", use_pairs=args.variant == "routed-pairs"
        )
    model = model.to(device)
    initial_checkpoint = None
    if args.initial:
        initial_checkpoint = torch.load(args.initial, map_location="cpu", weights_only=False)
        initial_state = initial_checkpoint["model"]
        target_rows = getattr(model, "psq_embed", torch.empty(0)).shape[0]
        source_rows = initial_state.get("psq_embed", torch.empty(0)).shape[0]
        if target_rows > source_rows and source_rows == PSQ_PLANES \
                and target_rows % source_rows == 0:
            # Every bucket begins as the exact current Mini embedding. Before
            # the first optimizer step the new model is therefore functionally
            # identical to the v4 one, not merely close to it.
            initial_state = dict(initial_state)
            initial_state["psq_embed"] = initial_state["psq_embed"].repeat(
                target_rows // source_rows, 1)
        model.load_state_dict(initial_state)
        model = model.to(device)
    optimizer = torch.optim.AdamW(
        model.parameters(), lr=args.lr, weight_decay=args.weight_decay
    )
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, args.epochs)
    rng = np.random.default_rng(args.seed)
    # Auxiliary sampling gets its own stream. Sharing one generator made the
    # aux arm consume about sixteen extra draws per epoch that the WDL-only arm
    # did not, so the two arms silently diverged in batch order from epoch two
    # onward -- noise rather than bias, but it stopped them differing by a
    # single field.
    aux_rng = np.random.default_rng(args.seed ^ 0x9E3779B97F4A7C15)
    # Both routers start as the shipped cascade, so a run with no reassignment
    # scheduled reproduces the shared-router model and is the honest control.
    lut_a = lut_c = None
    if args.cells_train:
        if initial_checkpoint is not None and "route_lut_a" in initial_checkpoint:
            lut_a = np.asarray(initial_checkpoint["route_lut_a"], dtype=np.uint8).copy()
            lut_c = np.asarray(initial_checkpoint["route_lut_c"], dtype=np.uint8).copy()
        else:
            lut_a = router_em.cascade_lut()
            lut_c = lut_a.copy()
        train.set_router(lut_a, lut_c)
        val.set_router(lut_a, lut_c)
        if disagreement_train is not None:
            disagreement_train.set_router(lut_a, lut_c)
            disagreement_val.set_router(lut_a, lut_c)
        if pv_train is not None:
            pv_train.set_router(lut_a, lut_c)
        if sibling_train is not None:
            sibling_train.set_router(lut_a, lut_c)
        if multipv_train is not None:
            multipv_train.set_router(lut_a, lut_c)
    em_epochs = {int(x) for x in args.router_em.split(",") if x.strip()}
    if em_epochs and lut_a is None:
        raise SystemExit("--router-em needs --cells-train")
    args.out.mkdir(parents=True, exist_ok=True)
    config = {
        **{
            key: str(value) if isinstance(value, Path) else value
            for key, value in vars(args).items()
        },
        "device": str(device),
        "parameters": sum(parameter.numel() for parameter in model.parameters()),
        "train_rows_after_filter": train.rows,
        "val_rows_after_filter": val.rows,
        "route_counts": train.route_counts,
    }
    (args.out / "config.json").write_text(json.dumps(config, indent=2) + "\n")

    pv_traversal = (PvTraversal(pv_root_id, pv_ply, pv_keep, aux_rng)
                    if pv_train is not None else None)
    # Emphasis is applied by repeating a root in the sampling pool rather than by
    # reweighting its loss, so each exposure stays an ordinary training example
    # and the auxiliary batch size is unchanged.
    aux_pool = (np.arange(disagreement_train.rows, dtype=np.int64)
                if disagreement_train is not None else None)
    if aux_pool is not None and args.flip_emphasis > 1:
        aux_pool = np.concatenate([
            aux_pool,
            np.repeat(np.flatnonzero(flip_mask), args.flip_emphasis - 1)])
    # One continuous sweep across epochs, so the pair set is consumed without
    # replacement rather than resampled.
    sibling_order = (aux_rng.permutation(sibling_train.rows)
                     if sibling_train is not None else None)
    sibling_cursor = 0
    # Its own stream, for two reasons. Constructing the sampler draws a
    # permutation, so sharing aux_rng would make `--multipv-dir` with mix 0
    # differ from not passing it at all. And every mix value then sees the same
    # root sequence, so MPV25 and MPV50 differ only in how many root rows they
    # give up -- not in which roots they saw.
    multipv_rng = np.random.default_rng(args.seed ^ 0x51ED270B7D2C4A19)
    multipv_sampler = (RootBalancedRows(multipv_root_id, multipv_rng)
                       if multipv_train is not None else None)
    # Held-out search metrics. Cross-entropy chose the weaker of the two King32
    # arms, so these carry the selection and CE stays as a calibration guard.
    search_suite = None
    if args.distill_dir:
        from .eval_distill_metrics import full_suite, open_suite
        from .eval_distill_metrics import set_router as set_suite_router
        search_suite = open_suite(args.distill_dir)
        if lut_a is not None:
            set_suite_router(search_suite, lut_a, lut_c)

    def search_metrics():
        if search_suite is None:
            return {}
        model.eval()
        measured = full_suite(model, search_suite, device, pairs=500_000)
        model.train()
        return measured

    with (args.out / "history.jsonl").open("w") as history:
        initial_record = {
            "epoch": 0,
            **{
                f"val_{key}": value
                for key, value in evaluate(model, val, args.batch, device).items()
            },
        }
        if disagreement_val is not None:
            initial_record.update({
                f"disagreement_val_{key}": value
                for key, value in evaluate_disagreement(
                    model, disagreement_val, args.disagreement_batch, device).items()
            })
        initial_record.update(search_metrics())
        emit(
            history,
            initial_record,
        )
        # The scale and calibration gates are relative to where this run began,
        # so a probe cannot quietly widen the evaluation distribution and call
        # an ordering gain a win.
        baseline = {key: initial_record.get(key) for key in
                    ("val_brier", "val_expected_mse", "scale_width")}
        best = math.inf
        best_search_key = (-math.inf, -math.inf)
        for epoch in range(1, args.epochs + 1):
            model.train()
            loss_sum = 0.0
            seen_rows = 0
            disagreement_huber_sum = 0.0
            disagreement_steps = 0
            disagreement_order = (aux_rng.permutation(aux_pool)
                                  if disagreement_train is not None else None)
            disagreement_cursor = 0
            sibling_hinge_sum = 0.0
            pv_exposures = 0
            multipv_exposures = 0
            started = time.perf_counter()
            if args.shuffle_block:
                blocks = np.arange(
                    (train.rows + args.shuffle_block - 1) // args.shuffle_block
                )
                rng.shuffle(blocks)
                batches = []
                for block in blocks:
                    block_start = int(block) * args.shuffle_block
                    block_end = min(block_start + args.shuffle_block, train.rows)
                    starts = np.arange(block_start, block_end, args.batch)
                    rng.shuffle(starts)
                    batches.extend(
                        slice(int(start), min(int(start) + args.batch, block_end))
                        for start in starts
                    )
            else:
                order = rng.permutation(train.rows)
                batches = (
                    order[start : start + args.batch]
                    for start in range(0, train.rows, args.batch)
                )
            # Proof that auxiliary sampling no longer perturbs the data order:
            # two arms differing only in their auxiliary objective must produce
            # the same digest in every epoch. Only the block path is hashed --
            # the fallback is a generator and consuming it here would empty it.
            batch_digest = hashlib.sha256()
            if isinstance(batches, list):
                for entry in batches:
                    batch_digest.update(
                        np.array([entry.start, entry.stop], dtype=np.int64).tobytes())
            for batch_index, selected in enumerate(batches, 1):
                *inputs, target = train.batch(selected, device)
                log_wdl = model(*inputs)[0]
                ce = soft_wdl_loss(log_wdl, target)
                loss = ce + args.smoothness * model.smoothness_loss()
                optimizer.zero_grad(set_to_none=True)
                loss.backward()
                optimizer.step()
                batch_rows = target.shape[0]
                loss_sum += float(ce.detach()) * batch_rows
                seen_rows += batch_rows
                if (args.disagreement_weight and
                        batch_index % args.disagreement_every == 0):
                    if disagreement_cursor + args.disagreement_batch > len(aux_pool):
                        disagreement_order = aux_rng.permutation(aux_pool)
                        disagreement_cursor = 0
                    disagreement_selected = disagreement_order[
                        disagreement_cursor:disagreement_cursor + args.disagreement_batch]
                    disagreement_cursor += len(disagreement_selected)

                    # Row i of the disagreement cache is root i, so the cycle
                    # above is already a root cycle. Each selected root is shown
                    # either its own position or the next unused view along the
                    # principal variation the deep search chose.
                    if pv_traversal is not None and args.pv_mix > 0.0:
                        take_pv = (aux_rng.random(len(disagreement_selected))
                                   < args.pv_mix)
                        chosen = np.flatnonzero(take_pv)
                        pv_rows, available = pv_traversal.draw(
                            disagreement_selected[chosen])
                        take_pv[chosen[~available]] = False
                        pv_rows = pv_rows[available]
                    else:
                        take_pv = np.zeros(len(disagreement_selected), dtype=bool)
                        pv_rows = np.empty(0, dtype=np.int64)
                    root_rows = disagreement_selected[~take_pv]
                    pv_exposures += int(len(pv_rows))

                    # MultiPV children replace part of the same auxiliary batch
                    # rather than adding a term. Same loss, same batch size, same
                    # step -- so the mix fraction is the only thing that varies
                    # and a result cannot be confounded with a second weight.
                    multipv_rows = np.empty(0, dtype=np.int64)
                    if multipv_train is not None and args.multipv_mix > 0.0:
                        wanted = int(round(args.multipv_mix * len(disagreement_selected)))
                        wanted = min(wanted, len(root_rows))
                        if wanted:
                            multipv_rows = multipv_sampler.draw(wanted)
                            # Give up as many root rows as MultiPV rows taken.
                            root_rows = root_rows[:len(root_rows) - len(multipv_rows)]
                    multipv_exposures += int(len(multipv_rows))

                    predicted_parts = []
                    target_parts = []
                    if len(root_rows):
                        *root_inputs, root_deep = disagreement_train.batch(
                            root_rows, device)
                        _wdl, advantage, decisive = model(*root_inputs)
                        predicted_parts.append(
                            187.0 * expected_logit_from_factors(advantage, decisive))
                        target_parts.append(root_deep)
                    if len(pv_rows):
                        *pv_inputs, pv_deep = pv_train.batch(pv_rows, device)
                        _wdl, advantage, decisive = model(*pv_inputs)
                        predicted_parts.append(
                            187.0 * expected_logit_from_factors(advantage, decisive))
                        target_parts.append(pv_deep)
                    if len(multipv_rows):
                        *mpv_inputs, mpv_deep = multipv_train.batch(multipv_rows, device)
                        _wdl, advantage, decisive = model(*mpv_inputs)
                        predicted_parts.append(
                            187.0 * expected_logit_from_factors(advantage, decisive))
                        target_parts.append(mpv_deep)
                    predicted = torch.cat(predicted_parts)
                    deep = torch.cat(target_parts)
                    disagreement_huber = F.huber_loss(
                        predicted / 187.0, deep / 187.0, delta=1.0)
                    search_loss = args.disagreement_weight * disagreement_huber

                    # The sibling hinge rides inside this same step. A third
                    # optimizer step would change the step count, the Adam state
                    # evolution and the number of weight-decay applications, so
                    # the arm would differ by more than its objective.
                    if sibling_train is not None and args.sibling_weight:
                        if sibling_cursor + args.sibling_batch > sibling_train.rows:
                            sibling_order = aux_rng.permutation(sibling_train.rows)
                            sibling_cursor = 0
                        sibling_selected = sibling_order[
                            sibling_cursor:sibling_cursor + args.sibling_batch]
                        sibling_cursor += len(sibling_selected)
                        sibling_inputs = sibling_train.batch(
                            sibling_train.pairs[sibling_selected], device)
                        _wdl, advantage, decisive = model(*sibling_inputs)
                        child = expected_logit_from_factors(advantage, decisive)
                        # U(m) = -E(child(m)); deep's move should lead. A hinge,
                        # not softplus: the search reported which move won, not
                        # by how much, so the loss must stop once separated.
                        sibling_hinge = F.relu(
                            args.sibling_margin + child[0::2] - child[1::2]).mean()
                        search_loss = search_loss + args.sibling_weight * sibling_hinge
                        sibling_hinge_sum += float(sibling_hinge.detach())

                    optimizer.zero_grad(set_to_none=True)
                    search_loss.backward()
                    optimizer.step()
                    disagreement_huber_sum += float(disagreement_huber.detach())
                    disagreement_steps += 1
            scheduler.step()
            metrics = evaluate(model, val, args.batch, device)
            record = {
                "epoch": epoch,
                "train_ce": loss_sum / seen_rows,
                "train_rows_per_second": train.rows / (time.perf_counter() - started),
                "lr": scheduler.get_last_lr()[0],
                "batch_order_sha256": batch_digest.hexdigest()[:16],
                **{f"val_{key}": value for key, value in metrics.items()},
            }
            if disagreement_val is not None:
                record.update({
                    f"disagreement_val_{key}": value
                    for key, value in evaluate_disagreement(
                        model, disagreement_val, args.disagreement_batch, device).items()
                })
                record["disagreement_train_huber"] = (
                    disagreement_huber_sum / max(1, disagreement_steps))
                record["disagreement_steps"] = disagreement_steps
                record["pv_exposures"] = pv_exposures
                record["multipv_exposures"] = multipv_exposures
                if args.sibling_weight:
                    record["sibling_train_hinge"] = (
                        sibling_hinge_sum / max(1, disagreement_steps))
            record.update(search_metrics())
            if epoch in em_epochs:
                lut_a, lut_c, report = router_em.reassign(
                    model, train, train.cells, lut_a, lut_c, args.batch, device,
                    stride=args.router_stride, split=args.router_split,
                    floor=args.router_floor)
                train.set_router(lut_a, lut_c)
                val.set_router(lut_a, lut_c)
                if disagreement_train is not None:
                    disagreement_train.set_router(lut_a, lut_c)
                    disagreement_val.set_router(lut_a, lut_c)
                record["router"] = report
                record["val_ce_after_reassign"] = evaluate(
                    model, val, args.batch, device)["ce"]
            # The gate is decided before the record is written, so the reason a
            # checkpoint was or was not taken is visible in history.jsonl and in
            # the saved checkpoint itself.
            if search_suite is not None:
                # Calibration and scale are safety gates, not the objective:
                # reject drift first, then rank what survives by the ordering
                # the search actually consumes.
                slack = 1.0 + args.search_gate_calibration
                brier_ok = (baseline["val_brier"] is None
                            or record["val_brier"] <= baseline["val_brier"] * slack)
                mse_ok = (baseline["val_expected_mse"] is None
                          or record["val_expected_mse"]
                          <= baseline["val_expected_mse"] * slack)
                width_ok = (not baseline["scale_width"]
                            or abs(record["scale_width"] / baseline["scale_width"] - 1.0)
                            <= args.search_gate_width)
                record["search_gate_brier_ok"] = bool(brier_ok)
                record["search_gate_mse_ok"] = bool(mse_ok)
                record["search_gate_width_ok"] = bool(width_ok)
                record["search_gate_pass"] = bool(brier_ok and mse_ok and width_ok)
            emit(history, record)
            checkpoint = {"model": model.state_dict(), "config": config, "record": record}
            if lut_a is not None:
                checkpoint["route_lut_a"] = lut_a
                checkpoint["route_lut_c"] = lut_c
            torch.save(checkpoint, args.out / "last.pt")
            if metrics["ce"] < best:
                best = metrics["ce"]
                # Kept under the historic name so the export tooling and the
                # existing runs still line up. It is the cross-entropy pick.
                torch.save(checkpoint, args.out / "best.pt")
            if search_suite is not None:
                candidate = (record["sibling_accuracy"], record["ordering_25_100"])
                if record["search_gate_pass"] and candidate > best_search_key:
                    best_search_key = candidate
                    torch.save(checkpoint, args.out / "best_search.pt")


if __name__ == "__main__":
    main()
