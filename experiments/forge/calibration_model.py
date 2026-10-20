from __future__ import annotations

import math

import torch
from torch import nn
from torch.nn import functional as F


RECORD_DTYPE = [
    ("score_cp", "<i2"),
    ("non_pawn_delta", "<i2"),
    ("phase", "u1"),
    ("rule50", "u1"),
    ("own_pawns", "u1"),
    ("enemy_pawns", "u1"),
    ("own_knights", "u1"),
    ("enemy_knights", "u1"),
    ("own_bishops", "u1"),
    ("enemy_bishops", "u1"),
    ("own_rooks", "u1"),
    ("enemy_rooks", "u1"),
    ("queens", "u1"),
    ("flags", "u1"),
]

IN_CHECK = 1 << 4
ROUTES = 27
SCORE_BINS = 65


def inverse_softplus(value: float) -> float:
    return math.log(math.expm1(value))


def expected_logit_from_factors(
    advantage: torch.Tensor, decisive: torch.Tensor
) -> torch.Tensor:
    """Stable logit(W + D/2) for FORGE's advantage/decisiveness factors.

    With x=exp(advantage) and y=exp(decisive), the exact odds are
    (1+x+2xy)/(1+x+2y).  Keeping both sums in log space avoids ever forming an
    expected score so close to one that it rounds to one.
    """
    one_plus_advantage = F.softplus(advantage)
    log_two = math.log(2.0)
    return torch.logaddexp(
        one_plus_advantage, advantage + decisive + log_two
    ) - torch.logaddexp(one_plus_advantage, decisive + log_two)


def route_index(records: torch.Tensor) -> torch.Tensor:
    """Symmetric 3x3x3 material route: phase, queens, total pawns."""
    phase = records[:, 2]
    pawns = records[:, 4] + records[:, 5]
    queens = (records[:, 12] & 15) + (records[:, 12] >> 4)
    phase_bucket = torch.where(phase <= 6, 0, torch.where(phase <= 14, 1, 2))
    pawn_bucket = torch.where(pawns <= 4, 0, torch.where(pawns <= 10, 1, 2))
    queen_bucket = queens.clamp(max=2)
    return phase_bucket * 9 + queen_bucket * 3 + pawn_bucket


class ForgeCalibration(nn.Module):
    """Tiny WDL calibrator for the untouched Boring semantic core.

    Advantage corrections are odd in signed chess evidence.  Decisiveness sees
    only symmetric totals and flags.  The model is deliberately all lookups
    and scalar arithmetic so a good result remains deployable in a few dozen
    integer operations.
    """

    def __init__(self) -> None:
        super().__init__()
        self.global_scale_raw = nn.Parameter(torch.tensor(inverse_softplus(1.0)))
        self.route_scale_raw = nn.Parameter(torch.full((ROUTES,), inverse_softplus(0.05)))
        self.route_residual = nn.Embedding(ROUTES * SCORE_BINS, 1)
        self.advantage_counts = nn.Parameter(torch.zeros(5))

        self.decisive_bias = nn.Parameter(torch.zeros(()))
        self.decisive_abs_score = nn.Embedding(SCORE_BINS, 1)
        self.decisive_phase = nn.Embedding(25, 1)
        self.decisive_rule50 = nn.Embedding(21, 1)
        self.decisive_pawns = nn.Embedding(17, 1)
        self.decisive_pawn_imbalance = nn.Embedding(9, 1)
        self.decisive_knights = nn.Embedding(5, 1)
        self.decisive_bishops = nn.Embedding(5, 1)
        self.decisive_rooks = nn.Embedding(5, 1)
        self.decisive_queens = nn.Embedding(3, 1)
        self.decisive_flags = nn.Embedding(16, 1)
        self.decisive_route_score = nn.Embedding(ROUTES * SCORE_BINS, 1)
        self.decisive_route_rule50 = nn.Embedding(ROUTES * 21, 1)
        self.reset_parameters()

    def reset_parameters(self) -> None:
        for module in self.modules():
            if isinstance(module, nn.Embedding):
                nn.init.zeros_(module.weight)

    def forward(self, records: torch.Tensor):
        records = records.long()
        score = records[:, 0].float()
        route = route_index(records)
        abs_bin = (score.abs().long() // 32).clamp(max=SCORE_BINS - 1)
        sign = score.sign()

        scale = F.softplus(self.global_scale_raw) + F.softplus(
            self.route_scale_raw[route]
        )
        advantage = score / 187.0 * scale
        advantage = advantage + sign * self.route_residual(
            route * SCORE_BINS + abs_bin
        ).squeeze(1)

        own_queens = records[:, 12] & 15
        enemy_queens = records[:, 12] >> 4
        signed_counts = torch.stack(
            [
                records[:, 4] - records[:, 5],
                records[:, 6] - records[:, 7],
                records[:, 8] - records[:, 9],
                records[:, 10] - records[:, 11],
                own_queens - enemy_queens,
            ],
            dim=1,
        ).float()
        advantage = advantage + signed_counts @ self.advantage_counts

        pawns = records[:, 4] + records[:, 5]
        pawn_imbalance = (records[:, 4] - records[:, 5]).abs()
        knights = (records[:, 6] + records[:, 7]).clamp(max=4)
        bishops = (records[:, 8] + records[:, 9]).clamp(max=4)
        rooks = (records[:, 10] + records[:, 11]).clamp(max=4)
        queens = (own_queens + enemy_queens).clamp(max=2)
        phase = records[:, 2].clamp(max=24)
        rule50 = (records[:, 3] // 5).clamp(max=20)
        raw_flags = records[:, 13]
        pair_count = ((raw_flags >> 2) & 1) + ((raw_flags >> 3) & 1)
        even_flags = (raw_flags & 3) | (pair_count << 2)

        decisive = self.decisive_bias
        decisive = decisive + self.decisive_abs_score(abs_bin).squeeze(1)
        decisive = decisive + self.decisive_phase(phase).squeeze(1)
        decisive = decisive + self.decisive_rule50(rule50).squeeze(1)
        decisive = decisive + self.decisive_pawns(pawns.clamp(max=16)).squeeze(1)
        decisive = decisive + self.decisive_pawn_imbalance(
            pawn_imbalance.clamp(max=8)
        ).squeeze(1)
        decisive = decisive + self.decisive_knights(knights).squeeze(1)
        decisive = decisive + self.decisive_bishops(bishops).squeeze(1)
        decisive = decisive + self.decisive_rooks(rooks).squeeze(1)
        decisive = decisive + self.decisive_queens(queens).squeeze(1)
        decisive = decisive + self.decisive_flags(even_flags).squeeze(1)
        decisive = decisive + self.decisive_route_score(
            route * SCORE_BINS + abs_bin
        ).squeeze(1)
        decisive = decisive + self.decisive_route_rule50(
            route * 21 + rule50
        ).squeeze(1)

        log_q = F.logsigmoid(advantage)
        log_one_minus_q = F.logsigmoid(-advantage)
        log_c = F.logsigmoid(decisive)
        log_draw = F.logsigmoid(-decisive)
        log_wdl = torch.stack(
            [log_c + log_q, log_draw, log_c + log_one_minus_q], dim=1
        )
        return log_wdl, advantage, decisive


def soft_wdl_loss(log_wdl: torch.Tensor, target: torch.Tensor) -> torch.Tensor:
    return -(target * log_wdl).sum(dim=1).mean()
