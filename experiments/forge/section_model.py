from __future__ import annotations

import numpy as np
import torch
from torch import nn
from torch.nn import functional as F

from .calibration_model import RECORD_DTYPE


SECTION_DTYPE = np.dtype(
    [
        ("context", np.dtype(RECORD_DTYPE, align=False)),
        ("psqt", "<i2", (2,)),
        ("section", "<i2", (12, 2, 2)),
        ("space", "<i2", (2,)),
        ("scale", "u1"),
        ("section_flags", "u1"),
    ],
    align=False,
)

KNOWN_ENDGAME = 1 << 0
REPLAY_MISMATCH = 1 << 1
BLACK_TO_MOVE = 1 << 5


# The 24 per-section |W-B| features re-encode a quantity the matching odd
# feature already carries as sign+bin, so they add no information -- only a
# separate response curve for decisiveness. They also cost 48 of the 206
# table lookups per evaluation. FORGE_DROP_ABSDIFF=1 removes them so that
# trade can be priced before any runtime format change.
DROP_ABSDIFF = __import__("os").environ.get("FORGE_DROP_ABSDIFF") == "1"


def section_features(rows: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Convert White/Black section traces to odd/even STM features."""
    context = rows["context"]
    black = (context["flags"] & BLACK_TO_MOVE) != 0
    orient = np.where(black, -1.0, 1.0).astype(np.float32)
    queens_own = context["queens"] & 15
    queens_enemy = context["queens"] >> 4

    signed_counts = np.column_stack(
        [
            context["own_pawns"].astype(np.int16) - context["enemy_pawns"],
            context["own_knights"].astype(np.int16) - context["enemy_knights"],
            context["own_bishops"].astype(np.int16) - context["enemy_bishops"],
            context["own_rooks"].astype(np.int16) - context["enemy_rooks"],
            queens_own.astype(np.int16) - queens_enemy,
        ]
    ).astype(np.float32)
    psqt = rows["psqt"].astype(np.float32)
    section = rows["section"].astype(np.float32)
    section_diff = (section[:, :, 0, :] - section[:, :, 1, :]) * orient[:, None, None]
    space_diff = (rows["space"][:, 0].astype(np.float32)
                  - rows["space"][:, 1]) * orient

    odd = np.column_stack(
        [
            context["score_cp"].astype(np.float32) / 1024.0,
            context["non_pawn_delta"].astype(np.float32) / 1024.0,
            signed_counts / 4.0,
            psqt * orient[:, None] / 2048.0,
            section_diff.reshape(len(rows), -1) / 512.0,
            space_diff / 64.0,
        ]
    ).astype(np.float32)

    total_counts = np.column_stack(
        [
            context["own_pawns"].astype(np.int16) + context["enemy_pawns"],
            context["own_knights"].astype(np.int16) + context["enemy_knights"],
            context["own_bishops"].astype(np.int16) + context["enemy_bishops"],
            context["own_rooks"].astype(np.int16) + context["enemy_rooks"],
            queens_own.astype(np.int16) + queens_enemy,
        ]
    ).astype(np.float32)
    count_imbalance = np.abs(signed_counts)
    section_sum = section[:, :, 0, :] + section[:, :, 1, :]
    section_absdiff = np.abs(section[:, :, 0, :] - section[:, :, 1, :])
    space_sum = rows["space"].sum(axis=1, dtype=np.int32).astype(np.float32)
    space_absdiff = np.abs(
        rows["space"][:, 0].astype(np.int32) - rows["space"][:, 1]
    ).astype(np.float32)
    flags = context["flags"]
    symmetric_flags = np.column_stack(
        [
            (flags & 1) != 0,
            (flags & 2) != 0,
            ((flags >> 2) & 1) + ((flags >> 3) & 1),
        ]
    ).astype(np.float32)
    even = np.column_stack(
        [
            np.abs(context["score_cp"].astype(np.float32)) / 1024.0,
            context["phase"].astype(np.float32) / 24.0,
            context["rule50"].astype(np.float32) / 100.0,
            rows["scale"].astype(np.float32) / 64.0,
            total_counts / np.asarray([16, 4, 4, 4, 2], dtype=np.float32),
            count_imbalance / 4.0,
            np.abs(psqt) / 2048.0,
            section_sum.reshape(len(rows), -1) / 512.0,
            *([] if DROP_ABSDIFF else
              [section_absdiff.reshape(len(rows), -1) / 512.0]),
            space_sum / 64.0,
            space_absdiff / 64.0,
            symmetric_flags,
        ]
    ).astype(np.float32)
    return odd, even


class ForgeSections(nn.Module):
    def __init__(self, odd_dim: int = 34, even_dim: int = 69, width: int = 24) -> None:
        super().__init__()
        self.odd = nn.Linear(odd_dim, width, bias=False)
        self.gate = nn.Linear(even_dim, width)
        self.advantage_out = nn.Linear(width, 1, bias=False)
        self.decisiveness = nn.Sequential(
            nn.Linear(even_dim, width), nn.ReLU(), nn.Linear(width, 1)
        )

    def forward(self, odd: torch.Tensor, even: torch.Tensor):
        odd_latent = torch.tanh(self.odd(odd))
        even_gate = torch.sigmoid(self.gate(even))
        advantage = self.advantage_out(odd_latent * even_gate).squeeze(1)
        decisive = self.decisiveness(even).squeeze(1)
        log_c = F.logsigmoid(decisive)
        log_wdl = torch.stack(
            [
                log_c + F.logsigmoid(advantage),
                F.logsigmoid(-decisive),
                log_c + F.logsigmoid(-advantage),
            ],
            dim=1,
        )
        return log_wdl, advantage, decisive
