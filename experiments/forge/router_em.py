#!/usr/bin/env python3
"""Learn which route each (phase, pawns) cell belongs to.

The shipped router is a hand-cut cascade on phase, with pawn count consulted
only at full material. It was cut to balance corpus mass, which is not the same
objective as balancing loss, and below phase 24 the pawn axis is invisible to
it entirely.

This replaces the cut with a table over the 425 (phase, pawns) cells, learned
by alternation: train the tables with the assignment fixed, then hold the
tables and move each cell to whichever route reads it best. That is Lloyd's
algorithm with the model's own loss as the distance, and it is exact rather
than approximate because the cell space is small and each cell's population is
fixed -- the reassignment step can simply score every cell under every route.

Three things keep it honest:

  * It starts from the cascade, so epoch 0 is the shipped model exactly and the
    run can only be judged against a partition it began by reproducing.
  * A cell moves only when the improvement clears two standard errors of its
    own mean, so cells do not churn on sampling noise.
  * A Lagrangian per-route penalty holds every route above a mass floor.
    Without it the assignment is free to starve routes, and a starved route is
    capacity paid for and not used.
"""
from __future__ import annotations

import numpy as np
import torch

from .build_cell_sidecar import CELLS, MAX_PAWNS, MAX_PHASE
from .calibration_model import soft_wdl_loss
from .section_lattice_model import ROUTES, balanced_route_numpy


def cascade_lut() -> np.ndarray:
    """The shipped partition, expressed over cells. Also the initial state."""
    context = np.zeros(CELLS, dtype=[("phase", "u1"), ("own_pawns", "u1"),
                                     ("enemy_pawns", "u1")])
    phase = np.arange(CELLS) // (MAX_PAWNS + 1)
    pawns = np.arange(CELLS) % (MAX_PAWNS + 1)
    context["phase"] = phase
    context["own_pawns"] = pawns
    context["enemy_pawns"] = 0
    lut = balanced_route_numpy(context).astype(np.uint8)
    assert phase.max() == MAX_PHASE and pawns.max() == MAX_PAWNS
    return lut


@torch.no_grad()
def _cell_costs(model, corpus, cells, lut_a, lut_c, side, batch, device, stride):
    """Per-cell loss change from moving to each candidate route.

    Every row is scored under its current route and under each candidate, so
    the difference is paired: the same position, same features, one index
    changed. Accumulating the paired difference rather than two independent
    means is what makes the standard error small enough to decide anything --
    the between-position variance, which dominates the raw loss, cancels.
    """
    total = torch.zeros(CELLS, ROUTES, dtype=torch.float64, device=device)
    square = torch.zeros(CELLS, ROUTES, dtype=torch.float64, device=device)
    count = torch.zeros(CELLS, dtype=torch.float64, device=device)
    lut_a_t = torch.from_numpy(lut_a.astype(np.int64)).to(device)
    lut_c_t = torch.from_numpy(lut_c.astype(np.int64)).to(device)
    model.eval()
    for start in range(0, corpus.rows, batch * stride):
        stop = min(start + batch, corpus.rows)
        if stop <= start:
            break
        rows = slice(start, stop)
        cell = torch.from_numpy(np.ascontiguousarray(cells[rows])).to(device).long()
        *inputs, target = corpus.raw_batch(rows, device)
        base_a, base_c = lut_a_t[cell], lut_c_t[cell]

        def loss_at(route_a, route_c):
            log_wdl = model(route_a, *inputs, route_c=route_c)[0]
            return -(target * log_wdl).sum(dim=1).double()

        base = loss_at(base_a, base_c)
        count.index_add_(0, cell, torch.ones_like(cell, dtype=torch.float64))
        for route in range(ROUTES):
            forced = torch.full_like(base_a, route)
            delta = loss_at(forced, base_c) if side == "a" else loss_at(base_a, forced)
            delta = delta - base
            total[:, route].index_add_(0, cell, delta)
            square[:, route].index_add_(0, cell, delta * delta)
    return (total.cpu().numpy(), square.cpu().numpy(), count.cpu().numpy())


def _assign(mean_delta, standard_error, current, mass, floor=0.04, iterations=400):
    """Argmin over the paired loss change, with a mass floor, ignoring noise.

    mean_delta[c, k] is how much cell c's mean loss moves if it is routed to k,
    so the current route's column is zero by construction and any move has to
    be negative by more than two standard errors to be taken.
    """
    scale = float(np.std(mean_delta[mass > 0])) if (mass > 0).any() else 1.0
    scale = max(scale, 1e-6)
    penalty = np.zeros(ROUTES)
    index = np.arange(CELLS)
    assignment = current.copy()
    share = np.zeros(ROUTES)
    for _ in range(iterations):
        adjusted = mean_delta + penalty[None, :]
        best = np.argmin(adjusted, axis=1)
        gain = adjusted[index, current] - adjusted[index, best]
        noise = standard_error[index, best]
        assignment = np.where((gain > 2.0 * noise) & (mass > 0), best, current)
        share = np.array([mass[assignment == r].sum() for r in range(ROUTES)])
        share = share / max(share.sum(), 1.0)
        if share.min() >= floor:
            break
        penalty += 0.05 * scale * (share - 1.0 / ROUTES)
    return assignment.astype(np.uint8), share


def reassign(model, corpus, cells, lut_a, lut_c, batch, device,
             stride=8, split=True, floor=0.04):
    """One E-step. Returns new LUTs and a record of what moved."""
    report = {}
    new = {}
    for side, lut in (("a", lut_a), ("c", lut_c)):
        if not split and side == "c":
            new["c"] = new["a"]
            report["c_follows_a"] = True
            continue
        total, square, count = _cell_costs(
            model, corpus, cells, lut_a if side == "a" else new.get("a", lut_a),
            lut_c, side, batch, device, stride)
        safe = np.maximum(count, 1.0)[:, None]
        mean = total / safe
        variance = np.maximum(square / safe - mean ** 2, 0.0)
        standard_error = np.sqrt(variance / safe)
        current = (lut_a if side == "a" else lut_c).astype(np.int64)
        assignment, share = _assign(mean, standard_error, current, count, floor)
        report[f"{side}_mean_gain"] = float(
            -(mean[np.arange(CELLS), assignment] * count).sum() / max(count.sum(), 1.0))
        new[side] = assignment
        report[f"{side}_moved_cells"] = int((assignment != current).sum())
        report[f"{side}_moved_mass"] = float(
            count[assignment != current].sum() / max(count.sum(), 1.0))
        report[f"{side}_route_share"] = [round(float(x), 4) for x in share]

    return new["a"], np.asarray(new["c"], dtype=np.uint8), report
