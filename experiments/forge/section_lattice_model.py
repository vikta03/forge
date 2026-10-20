from __future__ import annotations

import numpy as np
import torch
from torch import nn
from torch.nn import functional as F


# What the extractor and the bin cache produce. The model may see fewer.
FULL_ODD_FEATURES = 34
FULL_EVEN_FEATURES = 69

# FORGE-mini keeps only the features that measurably move the evaluation.
# Ranked on the bins real positions occupy -- a curve can be steep in a bin
# that never occurs -- the 54 features dropped here hold 1.44% of the total
# contribution variance between them, while costing half of all table lookups.
# The cut falls almost entirely on the even side: the advantage head's inputs
# survive nearly intact, while the decisiveness head turns out to need score
# magnitude, phase, rule50, endgame scale, a little material, king safety,
# threats, passers and pawn asymmetry -- and nothing else.
MINI = __import__("os").environ.get("FORGE_MINI") == "1"

# FORGE-lean drops the single most expensive thing Boring computes: pieces<>(),
# which is 98.6 ns of its 187.8 and bundles the four piece bonuses with the four
# mobility terms -- they cannot be separated, mobility is accumulated inside it.
#
# Everything else stays. An earlier attempt cut the whole attack-map tier for
# 183 ns and lost 398 +/- 175 Elo at fixed depth; per-term pricing showed why.
# Building the maps is only ~10 ns, and king safety, threats and passers are the
# terms that carry the evaluation: removing one moves the aggregate by 144, 79
# and 77 respectively, against 50 for the whole of pieces<>(). So this keeps the
# expensive-but-load-bearing terms and drops the expensive-but-not.
#
# The aggregate is their sum, so it changes; its two features and the endgame
# scale are re-binned from a sidecar rather than sliced from the cache.
LEAN = __import__("os").environ.get("FORGE_LEAN") == "1"
LEAN_ODD_KEEP = np.asarray([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 27, 28, 29, 30, 31, 32, 33], dtype=np.int64)
LEAN_EVEN_KEEP = np.asarray([0, 1, 2, 3, 4, 7, 9, 12, 14, 15, 35, 36, 37, 39, 40, 41, 58, 63], dtype=np.int64)
ODD_KEEP = np.asarray([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33], dtype=np.int64)
EVEN_KEEP = np.asarray([0, 1, 2, 3, 4, 7, 9, 12, 14, 15, 35, 36, 37, 39, 40, 41, 58, 63], dtype=np.int64)

ODD_FEATURES = 34
from .section_model import DROP_ABSDIFF as _DROP_ABSDIFF
EVEN_FEATURES = 45 if _DROP_ABSDIFF else 69
if sum((MINI, LEAN, _DROP_ABSDIFF)) > 1:
    raise RuntimeError("FORGE_MINI, FORGE_LEAN and FORGE_DROP_ABSDIFF each pick a "
                       "feature set; set at most one")
if LEAN:
    ODD_KEEP, EVEN_KEEP = LEAN_ODD_KEEP, LEAN_EVEN_KEEP
ROUTES = 8
CLIPPED_RELU_MAX = 8.0
UNARY_BINS = 129
# The latent books are the hot 106 KB of the ~133 KB per-evaluation working
# set, and their learned curves are the flattest thing in the model: the
# median feature moves in 18 of 128 bin steps, and decimating them 4x touches
# 23.4%/14.4% of entries at mean absolute error 2.5/1.2 on an int8 scale of
# +/-127. Coarsening only the latent side takes them to 27 KB. The direct
# books keep full resolution. FORGE_COARSE_LATENT=1 enables it.
COARSE_LATENT = __import__("os").environ.get("FORGE_COARSE_LATENT") == "1"
LATENT_SHIFT = 2 if COARSE_LATENT else 0
LATENT_BINS = ((UNARY_BINS - 1) >> LATENT_SHIFT) + 1
PAIR_BINS = 17
# 12 piece types x 64 squares, side-to-move relative (squares flipped for
# Black), the same encoding widetable's sparse branch consumes.
PSQ_PLANES = 768
PSQ_MAX_PIECES = 32
# The piece-square channels are self-gated by default: tanh(v) into
# advantage, SiLU(v) into decisiveness. That is nonlinear and
# route-conditioned but additive with respect to the section features --
# placement never multiplies against king safety, threats or activity.
# Cross-gating pairs psq channel k with even latent channel k mod width,
# the same index-wise structure the existing latent already uses.
PSQ_CROSSGATE = __import__("os").environ.get("FORGE_PSQ_CROSSGATE") == "1"

# The inputs produced by section_features() are already normalized by semantic
# family.  These multipliers turn their common discrete steps back into small
# integer buckets.  Runtime export therefore needs multiply/round/clamp, not a
# learned threshold search.
# Per-feature, not per-family. Bin occupancy measured over real positions showed
# 99% of them landing in 9.4 of the 129 bins, with 76 of 103 features carrying
# four times more distinct raw states than bins they reached. These are the
# former family scales multiplied by a power of two per feature, chosen as the
# largest that keeps the 99.9th percentile inside the bin range. Powers of two
# keep the runtime quantizer a shift, so inference cost is unchanged.
# These must stay in lockstep with ODD_BIN / EVEN_BIN in forge_factor_net.cpp:
# runtime ratio num/den satisfies normalised = raw*num/(den*2*SCALE).
ODD_SCALES = np.asarray(
    [
        32, 32, 4, 4, 4, 4, 4, 64, 32, 128, 128, 512,
        512, 256, 128, 256, 1024, 16, 16, 256, 256, 256, 256, 512,
        128, 512, 256, 16, 128, 64, 64, 128, 32, 16,
    ],
    dtype=np.float32,
)
EVEN_SCALES = np.asarray(
    [
        16, 24, 20.3125, 16, 16, 4, 4, 4, 2, 4, 4, 4,
        4, 4, 32, 32, 32, 32, 128, 128, 64, 32, 128, 256,
        8, 8, 128, 128, 64, 64, 128, 32, 128, 64, 8, 32,
        16, 16, 32, 8, 64, 64, 256, 256, 128, 64, 128, 512,
        8, 8, 128, 128, 128, 128, 256, 64, 256, 128, 8, 64,
        32, 32, 64, 16, 4, 8, 1, 1, 1,
    ],
    dtype=np.float32,
)

# Control switch for the rebinning experiment: FORGE_LEGACY_SCALES=1 restores
# the former per-family scales, so the old and new binning can be trained under
# identical from-scratch settings and the difference attributed to the binning.
if __import__("os").environ.get("FORGE_LEGACY_SCALES") == "1":
    ODD_SCALES = np.asarray(
        [32, 32] + [4] * 5 + [32] * 2 + [16] * 24 + [8], dtype=np.float32
    )
    EVEN_SCALES = np.asarray(
        [16, 24, 20, 8]
        + [16, 4, 4, 4, 2]
        + [4] * 5
        + [32] * 2
        + [8] * 48
        + [2, 4, 1, 1, 1],
        dtype=np.float32,
    )

# Each odd family is conditioned only on its matching amount of symmetric
# activity.  The section pairs are (side-to-move difference, both-side sum),
# which retains information discarded by a single global pool.
ADVANTAGE_PAIRS = tuple(
    [(odd, even) for odd, even in zip(range(2, 7), range(4, 9))]
    + [(odd, even) for odd, even in zip(range(9, 33), range(16, 40))]
    + [(33, 64)]
)

# A few chess-specific convertibility interactions.  These are deliberately
# sparse rather than an all-pairs net: score magnitude interacts with phase,
# rule-50 and the endgame scale; pawn mass with pawn imbalance; bishop mass
# with opposite-bishop flags; phase and rule-50 with remaining pawns.
DECISIVENESS_PAIRS = (
    (0, 1),
    (0, 2),
    (0, 3),
    (4, 9),
    (6, 66),
    (6, 67),
    (1, 4),
    (2, 4),
)

if _DROP_ABSDIFF:
    # drop even features 40..63 and shift the later ones down
    EVEN_SCALES = np.delete(EVEN_SCALES, np.arange(40, 64))
    _shift = lambda i: i - 24 if i >= 64 else i
    ADVANTAGE_PAIRS = tuple((o, _shift(e)) for o, e in ADVANTAGE_PAIRS)
    DECISIVENESS_PAIRS = tuple((a, _shift(b)) for a, b in DECISIVENESS_PAIRS)

# Kept unfiltered so quantize_sections can still bin what the extractor and the
# bin cache produce, which is always full width.
ODD_FULL_SCALES = ODD_SCALES
EVEN_FULL_SCALES = EVEN_SCALES
if MINI or LEAN:
    ODD_SCALES = ODD_SCALES[ODD_KEEP]
    EVEN_SCALES = EVEN_SCALES[EVEN_KEEP]
    ODD_FEATURES = len(ODD_KEEP)
    EVEN_FEATURES = len(EVEN_KEEP)

# ---- wide-trace arms -------------------------------------------------------
# The widened Boring trace adds 26 quantities; each becomes one odd feature
# (W-B, signed) and one even feature (W+B). An arm selects a subset. Unset means
# the shipped behaviour, unchanged.
WIDE_Q = 26
WIDE_EMITTED = tuple(range(0, 17))    # already computed today, discarded
WIDE_NEW = tuple(range(17, 26))       # newly computed in the widened eval
WIDE_ARMS = {"net0": (), "netA": WIDE_EMITTED,
             "netB": WIDE_EMITTED + WIDE_NEW, "netC": WIDE_NEW}
WIDE_ARM = __import__("os").environ.get("FORGE_WIDE_ARM", "")
if WIDE_ARM and WIDE_ARM not in WIDE_ARMS:
    raise RuntimeError(f"FORGE_WIDE_ARM must be one of {sorted(WIDE_ARMS)}")
WIDE_KEEP = WIDE_ARMS.get(WIDE_ARM, ())
if WIDE_KEEP:
    # The extra columns arrive pre-binned, so these scales are never consulted;
    # they exist so the shape assertions below still describe the model.
    ODD_SCALES = np.concatenate([ODD_SCALES, np.ones(len(WIDE_KEEP), np.float32)])
    EVEN_SCALES = np.concatenate([EVEN_SCALES, np.ones(len(WIDE_KEEP), np.float32)])
    ODD_FEATURES += len(WIDE_KEEP)
    EVEN_FEATURES += len(WIDE_KEEP)

assert ODD_SCALES.shape == (ODD_FEATURES,)
assert EVEN_SCALES.shape == (EVEN_FEATURES,)
assert len(ADVANTAGE_PAIRS) == 30


def balanced_route_numpy(context: np.ndarray) -> np.ndarray:
    """Eight symmetric material-life-cycle routes with balanced corpus mass.

    Phase is stable on quiet moves, making the active table set sticky inside
    a search subtree.  The only extra split is the full-material opening,
    where all sixteen pawns is common enough to deserve its own book.
    """

    phase = context["phase"]
    pawns = context["own_pawns"].astype(np.int16) + context[
        "enemy_pawns"
    ].astype(np.int16)
    route = np.empty(len(context), dtype=np.uint8)
    route[phase <= 4] = 0
    route[(phase >= 5) & (phase <= 7)] = 1
    route[(phase >= 8) & (phase <= 11)] = 2
    route[(phase >= 12) & (phase <= 16)] = 3
    route[(phase >= 17) & (phase <= 20)] = 4
    route[(phase >= 21) & (phase <= 23)] = 5
    route[(phase >= 24) & (pawns < 16)] = 6
    route[(phase >= 24) & (pawns >= 16)] = 7
    return route


def quantize_sections(
    odd: np.ndarray, even: np.ndarray
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    if MINI or LEAN:
        # The bin cache stores full-width bins and the quantizer is per feature,
        # so selecting columns after quantizing is identical to quantizing the
        # selected columns -- and it means no 11 GiB cache rebuild.
        want_odd, want_even = FULL_ODD_FEATURES, FULL_EVEN_FEATURES
        scale_odd = ODD_FULL_SCALES
        scale_even = EVEN_FULL_SCALES
    else:
        want_odd, want_even = ODD_FEATURES, EVEN_FEATURES
        scale_odd, scale_even = ODD_SCALES, EVEN_SCALES
    if odd.shape[1] != want_odd or even.shape[1] != want_even:
        raise ValueError(f"unexpected section shape: odd={odd.shape}, even={even.shape}")
    odd_sign = np.sign(odd).astype(np.int8)
    # Half-step resolution kept 65-bin tables small, but an audit against the
    # dense section head found that it cost 0.00304 validation CE.  Doubling
    # the bins reduced that loss to 0.00091 without changing the lookup count;
    # a route's deployed int16 working set remains under 50 KiB.
    odd_bins = (
        np.rint(np.abs(odd) * (2.0 * scale_odd)).clip(0, 128).astype(np.uint8)
    )
    # Even under colour exchange does not mean non-negative. Preserve the sign
    # of section sums in a centered [-64, 64] code.
    even_bins = (
        np.rint(even * (2.0 * scale_even)).clip(-64, 64).astype(np.int16) + 64
    ).astype(np.uint8)
    return odd_sign, odd_bins, even_bins


class RoutedFamilyLattice(nn.Module):
    """A parity-exact, hard-routed generalized additive evaluator.

    Global backoff and route residuals are separate while learning, which lets
    rare cells borrow strength.  They are algebraically folded into a single
    active route table for deployment, so the sharing has zero inference cost.
    """

    def __init__(self, use_routes: bool = True, use_pairs: bool = True) -> None:
        super().__init__()
        self.use_routes = use_routes
        self.use_pairs = use_pairs
        self.advantage_shared = nn.Parameter(
            torch.zeros(ODD_FEATURES, UNARY_BINS)
        )
        self.decisive_shared = nn.Parameter(
            torch.zeros(EVEN_FEATURES, UNARY_BINS)
        )
        self.decisive_bias_shared = nn.Parameter(torch.zeros(()))
        if use_routes:
            self.advantage_route = nn.Parameter(
                torch.zeros(ROUTES, ODD_FEATURES, UNARY_BINS)
            )
            self.decisive_route = nn.Parameter(
                torch.zeros(ROUTES, EVEN_FEATURES, UNARY_BINS)
            )
            self.decisive_bias_route = nn.Parameter(torch.zeros(ROUTES))
        else:
            self.register_parameter("advantage_route", None)
            self.register_parameter("decisive_route", None)
            self.register_parameter("decisive_bias_route", None)

        if use_pairs:
            self.advantage_pair_shared = nn.Parameter(
                torch.zeros(len(ADVANTAGE_PAIRS), PAIR_BINS, PAIR_BINS)
            )
            self.decisive_pair_shared = nn.Parameter(
                torch.zeros(len(DECISIVENESS_PAIRS), PAIR_BINS, PAIR_BINS)
            )
            if use_routes:
                self.advantage_pair_route = nn.Parameter(
                    torch.zeros(
                        ROUTES, len(ADVANTAGE_PAIRS), PAIR_BINS, PAIR_BINS
                    )
                )
                self.decisive_pair_route = nn.Parameter(
                    torch.zeros(
                        ROUTES, len(DECISIVENESS_PAIRS), PAIR_BINS, PAIR_BINS
                    )
                )
            else:
                self.register_parameter("advantage_pair_route", None)
                self.register_parameter("decisive_pair_route", None)
        else:
            self.register_parameter("advantage_pair_shared", None)
            self.register_parameter("decisive_pair_shared", None)
            self.register_parameter("advantage_pair_route", None)
            self.register_parameter("decisive_pair_route", None)

        self.register_buffer(
            "advantage_pair_odd",
            torch.tensor([item[0] for item in ADVANTAGE_PAIRS]),
            persistent=False,
        )
        self.register_buffer(
            "advantage_pair_even",
            torch.tensor([item[1] for item in ADVANTAGE_PAIRS]),
            persistent=False,
        )
        self.register_buffer(
            "decisive_pair_left",
            torch.tensor([item[0] for item in DECISIVENESS_PAIRS]),
            persistent=False,
        )
        self.register_buffer(
            "decisive_pair_right",
            torch.tensor([item[1] for item in DECISIVENESS_PAIRS]),
            persistent=False,
        )

    @staticmethod
    def _shared_lookup(table: torch.Tensor, bins: torch.Tensor) -> torch.Tensor:
        feature = torch.arange(bins.shape[1], device=bins.device)
        indices = feature.unsqueeze(0) * table.shape[-1] + bins
        return table.reshape(-1)[indices]

    @staticmethod
    def _route_lookup(
        table: torch.Tensor, route: torch.Tensor, bins: torch.Tensor
    ) -> torch.Tensor:
        feature = torch.arange(bins.shape[1], device=bins.device)
        indices = (
            (route.unsqueeze(1) * bins.shape[1] + feature.unsqueeze(0))
            * table.shape[-1]
            + bins
        )
        return table.reshape(-1)[indices]

    @staticmethod
    def _shared_pair_lookup(
        table: torch.Tensor, left: torch.Tensor, right: torch.Tensor
    ) -> torch.Tensor:
        pair = torch.arange(left.shape[1], device=left.device)
        indices = (pair.unsqueeze(0) * PAIR_BINS + left) * PAIR_BINS + right
        return table.reshape(-1)[indices]

    @staticmethod
    def _route_pair_lookup(
        table: torch.Tensor,
        route: torch.Tensor,
        left: torch.Tensor,
        right: torch.Tensor,
    ) -> torch.Tensor:
        pair = torch.arange(left.shape[1], device=left.device)
        indices = (
            ((route.unsqueeze(1) * left.shape[1] + pair.unsqueeze(0)) * PAIR_BINS + left)
            * PAIR_BINS
            + right
        )
        return table.reshape(-1)[indices]

    def forward(
        self,
        route: torch.Tensor,
        odd_sign: torch.Tensor,
        odd_bins: torch.Tensor,
        even_bins: torch.Tensor,
    ):
        route = route.long()
        odd_sign = odd_sign.float()
        odd_bins = odd_bins.long()
        even_bins = even_bins.long()

        advantage_terms = self._shared_lookup(self.advantage_shared, odd_bins)
        decisive_terms = self._shared_lookup(self.decisive_shared, even_bins)
        decisive = self.decisive_bias_shared + decisive_terms.sum(dim=1)
        if self.use_routes:
            advantage_terms = advantage_terms + self._route_lookup(
                self.advantage_route, route, odd_bins
            )
            decisive = (
                decisive
                + self.decisive_bias_route[route]
                + self._route_lookup(self.decisive_route, route, even_bins).sum(dim=1)
            )
        advantage = (advantage_terms * odd_sign).sum(dim=1)

        if self.use_pairs:
            pair_sign = odd_sign[:, self.advantage_pair_odd]
            odd_pair_bin = odd_bins[:, self.advantage_pair_odd] // 8
            even_pair_bin = even_bins[:, self.advantage_pair_even] // 8
            advantage_pair = self._shared_pair_lookup(
                self.advantage_pair_shared, odd_pair_bin, even_pair_bin
            )
            decisive_left = even_bins[:, self.decisive_pair_left] // 8
            decisive_right = even_bins[:, self.decisive_pair_right] // 8
            decisive_pair = self._shared_pair_lookup(
                self.decisive_pair_shared, decisive_left, decisive_right
            )
            if self.use_routes:
                advantage_pair = advantage_pair + self._route_pair_lookup(
                    self.advantage_pair_route,
                    route,
                    odd_pair_bin,
                    even_pair_bin,
                )
                decisive_pair = decisive_pair + self._route_pair_lookup(
                    self.decisive_pair_route,
                    route,
                    decisive_left,
                    decisive_right,
                )
            advantage = advantage + (advantage_pair * pair_sign).sum(dim=1)
            decisive = decisive + decisive_pair.sum(dim=1)

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

    def smoothness_loss(self) -> torch.Tensor:
        tables = [self.advantage_shared, self.decisive_shared]
        if self.use_routes:
            tables.extend([self.advantage_route, self.decisive_route])
        total = sum((table[..., 1:] - table[..., :-1]).square().mean() for table in tables)
        if self.use_pairs:
            pair_tables = [self.advantage_pair_shared, self.decisive_pair_shared]
            if self.use_routes:
                pair_tables.extend(
                    [self.advantage_pair_route, self.decisive_pair_route]
                )
            total = total + sum(
                (table[..., 1:, :] - table[..., :-1, :]).square().mean()
                + (table[..., :, 1:] - table[..., :, :-1]).square().mean()
                for table in pair_tables
            )
        return total

    @torch.no_grad()
    def folded_tables(self) -> dict[str, torch.Tensor]:
        """Return the one-lookup-per-term deployment representation."""

        if self.use_routes:
            result = {
                "advantage": self.advantage_shared.unsqueeze(0)
                + self.advantage_route,
                "decisive": self.decisive_shared.unsqueeze(0) + self.decisive_route,
                "decisive_bias": self.decisive_bias_shared
                + self.decisive_bias_route,
            }
            if self.use_pairs:
                result["advantage_pair"] = self.advantage_pair_shared.unsqueeze(0) + self.advantage_pair_route
                result["decisive_pair"] = self.decisive_pair_shared.unsqueeze(0) + self.decisive_pair_route
            return result
        result = {
            "advantage": self.advantage_shared.unsqueeze(0),
            "decisive": self.decisive_shared.unsqueeze(0),
            "decisive_bias": self.decisive_bias_shared.reshape(1),
        }
        if self.use_pairs:
            result["advantage_pair"] = self.advantage_pair_shared.unsqueeze(0)
            result["decisive_pair"] = self.decisive_pair_shared.unsqueeze(0)
        return result


class RoutedFactorLattice(nn.Module):
    """Routed unary books plus a shared low-rank interaction lattice.

    A row lookup returns a short contiguous vector.  Summing 34 odd and 69 even
    rows costs only integer vector additions at deployment; the eight (or four)
    gated products are the only cross-feature work.  Shared latent books see
    the whole corpus, while the cheap scalar unary books remain route-local.
    """

    def __init__(self, width: int = 4, psq_width: int = 0,
                 king_buckets: int = 1, mlp_hidden: int | None = None,
                 mlp_product: bool | None = None) -> None:
        super().__init__()
        if width not in (4, 8, 16):
            raise ValueError("factor lattice width must be 4, 8 or 16")
        # Replacement hybrid: a tiny per-route MLP mixes the pooled state in
        # place of the fixed tanh(odd)*sigmoid(even) gate. Zero disables it and
        # the model is byte-identical to before.
        self.mlp_hidden = (int(__import__("os").environ.get("FORGE_MLP_HIDDEN", "0"))
                           if mlp_hidden is None else int(mlp_hidden))
        # The fixed head's tanh(odd)*sigmoid(even) is the only multiplicative
        # coupling in the model. Replacing it with clamp(W@z) trades 40 products
        # for learned linear mixing -- strictly fewer nonlinearities. With this
        # set, the product is fed INTO the mixer instead, so the head keeps both.
        self.mlp_product = (__import__("os").environ.get("FORGE_MLP_PRODUCT") == "1"
                            if mlp_product is None else bool(mlp_product))
        self.width = width
        # Optional piece-square branch. Factor8 otherwise sees placement only
        # as psqt.mg and psqt.eg -- two scalars standing in for 768 planes --
        # so two positions with the same section totals and the same tapered
        # psqt sum are literally the same input to it. The planes are already
        # side-to-move relative, so a colour mirror maps to the same plane set
        # and the model's mirror-invariance survives unchanged.
        self.psq_width = psq_width
        if king_buckets not in (1, 16, 32):
            raise ValueError("own-king bucket count must be 1, 16, or 32")
        if not psq_width and king_buckets != 1:
            raise ValueError("king buckets require a piece-square branch")
        self.king_buckets = king_buckets
        if psq_width:
            self.psq_embed = nn.Parameter(
                torch.zeros(king_buckets * PSQ_PLANES, psq_width))
            nn.init.normal_(self.psq_embed, std=0.05)
            self.psq_advantage_out = nn.Parameter(torch.empty(ROUTES, psq_width))
            self.psq_decisive_out = nn.Parameter(torch.empty(ROUTES, psq_width))
            nn.init.normal_(self.psq_advantage_out, std=0.02)
            nn.init.normal_(self.psq_decisive_out, std=0.02)
        self.advantage_shared = nn.Parameter(
            torch.zeros(ODD_FEATURES, UNARY_BINS)
        )
        self.advantage_route = nn.Parameter(
            torch.zeros(ROUTES, ODD_FEATURES, UNARY_BINS)
        )
        self.decisive_shared = nn.Parameter(
            torch.zeros(EVEN_FEATURES, UNARY_BINS)
        )
        self.decisive_route = nn.Parameter(
            torch.zeros(ROUTES, EVEN_FEATURES, UNARY_BINS)
        )
        self.decisive_bias_shared = nn.Parameter(torch.zeros(()))
        self.decisive_bias_route = nn.Parameter(torch.zeros(ROUTES))
        self.odd_latent = nn.Parameter(
            torch.zeros(ODD_FEATURES, LATENT_BINS, width)
        )
        self.even_latent = nn.Parameter(
            torch.zeros(EVEN_FEATURES, LATENT_BINS, width)
        )
        if self.mlp_hidden:
            # z = [odd_latent | even_latent | placement]; placement currently
            # reaches the score only through a gate derived from the EVEN bank,
            # so concatenating gives the mixer a placement x odd path the fixed
            # head structurally cannot express.
            z_dim = 2 * width + (psq_width if psq_width else 0)
            if self.mlp_product:
                z_dim += width
            h = self.mlp_hidden
            self.mlp_w1 = nn.Parameter(torch.empty(ROUTES, z_dim, h))
            self.mlp_b1 = nn.Parameter(torch.zeros(ROUTES, h))
            self.mlp_adv = nn.Parameter(torch.zeros(ROUTES, h))
            self.mlp_dec = nn.Parameter(torch.zeros(ROUTES, h))
            nn.init.normal_(self.mlp_w1, std=(1.0 / z_dim) ** 0.5)
            # per-block input scales: the three blocks are summed over 31, 18
            # and a variable piece count, so their magnitudes differ a lot.
            self.mlp_scale = nn.Parameter(torch.ones(3))
        self.advantage_out = nn.Parameter(torch.empty(ROUTES, width))
        self.decisive_out = nn.Parameter(torch.empty(ROUTES, width))
        nn.init.normal_(self.advantage_out, std=0.02)
        nn.init.normal_(self.decisive_out, std=0.02)

    @staticmethod
    def _shared_lookup(table: torch.Tensor, bins: torch.Tensor) -> torch.Tensor:
        feature = torch.arange(bins.shape[1], device=bins.device)
        indices = feature.unsqueeze(0) * table.shape[-1] + bins
        return table.reshape(-1)[indices]

    @staticmethod
    def _route_lookup(
        table: torch.Tensor, route: torch.Tensor, bins: torch.Tensor
    ) -> torch.Tensor:
        feature = torch.arange(bins.shape[1], device=bins.device)
        indices = (
            (route.unsqueeze(1) * bins.shape[1] + feature.unsqueeze(0))
            * table.shape[-1]
            + bins
        )
        return table.reshape(-1)[indices]

    @staticmethod
    def _vector_lookup(table: torch.Tensor, bins: torch.Tensor) -> torch.Tensor:
        feature = torch.arange(bins.shape[1], device=bins.device)
        indices = feature.unsqueeze(0) * table.shape[1] + bins
        return table.reshape(-1, table.shape[-1])[indices].sum(dim=1)

    def forward(
        self,
        route: torch.Tensor,
        odd_sign: torch.Tensor,
        odd_bins: torch.Tensor,
        even_bins: torch.Tensor,
        psq_idx: torch.Tensor | None = None,
        psq_count: torch.Tensor | None = None,
        route_c: torch.Tensor | None = None,
    ):
        # The two heads mean different things -- who is better, and how sharply
        # -- so they need not be conditioned on the same thing. `route` drives
        # the advantage side and `route_c` the decisiveness side; passing one
        # route reproduces the shared-router model exactly.
        route = route.long()
        route_c = route if route_c is None else route_c.long()
        odd_sign = odd_sign.float()
        odd_bins = odd_bins.long()
        even_bins = even_bins.long()

        # Every lookup here is "gather a row per feature, then sum over
        # features", which is exactly embedding_bag. Writing it as an explicit
        # gather materialises a (batch, features, width) tensor only to reduce
        # it away -- 1.08 GiB at batch 512k -- and its backward is an atomic
        # scatter-add that degrades badly when bins are concentrated, which
        # measurement says they are: 99% of real positions occupy ~9 of the 129
        # bins. embedding_bag fuses both directions and measured 1.73x faster
        # on the realistic bin distribution, flat where the manual path is not.
        odd_index = (torch.arange(ODD_FEATURES, device=odd_bins.device)
                     * UNARY_BINS + odd_bins)
        even_index = (torch.arange(EVEN_FEATURES, device=even_bins.device)
                      * UNARY_BINS + even_bins)
        odd_route_index = (route.unsqueeze(1) * (ODD_FEATURES * UNARY_BINS)
                           + odd_index)
        even_route_index = (route_c.unsqueeze(1) * (EVEN_FEATURES * UNARY_BINS)
                            + even_index)

        def bag(table, index, weights=None, width=1):
            return F.embedding_bag(
                index, table.reshape(-1, width), mode="sum",
                per_sample_weights=weights,
            )

        advantage = (
            bag(self.advantage_shared, odd_index, odd_sign)
            + bag(self.advantage_route, odd_route_index, odd_sign)
        ).squeeze(1)
        decisive = (
            self.decisive_bias_shared
            + self.decisive_bias_route[route_c]
            + bag(self.decisive_shared, even_index).squeeze(1)
            + bag(self.decisive_route, even_route_index).squeeze(1)
        )
        if LATENT_SHIFT:
            odd_latent_index = (
                torch.arange(ODD_FEATURES, device=odd_bins.device) * LATENT_BINS
                + (odd_bins >> LATENT_SHIFT))
            even_latent_index = (
                torch.arange(EVEN_FEATURES, device=even_bins.device) * LATENT_BINS
                + (even_bins >> LATENT_SHIFT))
        else:
            odd_latent_index, even_latent_index = odd_index, even_index
        odd_latent = bag(self.odd_latent, odd_latent_index, odd_sign, self.width)
        even_latent = bag(self.even_latent, even_latent_index, None, self.width)
        gate = torch.sigmoid(even_latent)
        if not self.mlp_hidden:
            advantage = advantage + (
                torch.tanh(odd_latent) * gate * self.advantage_out[route]
            ).sum(dim=1)
            decisive = decisive + (
                F.silu(even_latent) * self.decisive_out[route_c]
            ).sum(dim=1)

        if self.psq_width and psq_idx is not None:
            # Slots past `count` are zero-padded; weight them out rather than
            # letting plane 0 be counted for every short position.
            slots = torch.arange(psq_idx.shape[1], device=psq_idx.device)
            mask = (slots.unsqueeze(0) < psq_count.long().unsqueeze(1)).float()
            conditioned_idx = psq_idx.long()
            if self.king_buckets > 1:
                # Own king is plane 5, hence indices 320..383. The planes are
                # already side-to-move-relative, so this square is relative to
                # the perspective whose position is being evaluated.
                own_king = ((conditioned_idx >= 320) & (conditioned_idx < 384)
                            & mask.bool())
                if torch.any(own_king.sum(dim=1) != 1):
                    raise ValueError("each PSQ row must contain exactly one own king")
                king_square = ((conditioned_idx - 320) * own_king).sum(dim=1)
                rank_band = king_square // 16
                file = king_square % 8
                bucket = (rank_band * (8 if self.king_buckets == 32 else 4)
                          + (file if self.king_buckets == 32 else file // 2))
                conditioned_idx = conditioned_idx + bucket.unsqueeze(1) * PSQ_PLANES
            placement = F.embedding_bag(
                conditioned_idx, self.psq_embed, mode="sum", per_sample_weights=mask)
            if self.mlp_hidden:
                pass          # placement is consumed by the mixer below
            elif PSQ_CROSSGATE:
                pair = (torch.arange(self.psq_width, device=placement.device)
                        % self.width)
                psq_gate = gate[:, pair]
                advantage = advantage + (
                    torch.tanh(placement) * psq_gate
                    * self.psq_advantage_out[route]).sum(dim=1)
                decisive = decisive + (
                    F.silu(placement) * psq_gate
                    * self.psq_decisive_out[route_c]).sum(dim=1)
            else:
                advantage = advantage + (
                    torch.tanh(placement) * self.psq_advantage_out[route]).sum(dim=1)
                decisive = decisive + (
                    F.silu(placement) * self.psq_decisive_out[route_c]).sum(dim=1)
        else:
            placement = None

        if self.mlp_hidden:
            # Replacement hybrid: one tiny per-route MLP mixes the pooled state
            # in place of tanh(odd)*sigmoid(even) and the psq crossgate. The
            # tables keep doing cheap per-feature nonlinear lookup; the mixer
            # does the one job the fixed head cannot -- arbitrary combinations.
            blocks = [odd_latent * self.mlp_scale[0], even_latent * self.mlp_scale[1]]
            if placement is not None:
                blocks.append(placement * self.mlp_scale[2])
            if self.mlp_product:
                # the coupling the fixed head had, handed to the mixer as input
                blocks.append(torch.tanh(odd_latent) * gate)
            z = torch.cat(blocks, dim=1)
            # clipped ReLU: the cheapest CPU-friendly activation and the one
            # int8 NNUE heads are built around.
            hidden = torch.clamp(
                torch.einsum('bz,bzh->bh', z, self.mlp_w1[route]) + self.mlp_b1[route],
                min=0.0, max=CLIPPED_RELU_MAX)
            advantage = advantage + (hidden * self.mlp_adv[route]).sum(dim=1)
            decisive = decisive + (hidden * self.mlp_dec[route_c]).sum(dim=1)

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

    def smoothness_loss(self) -> torch.Tensor:
        # psq_embed is deliberately excluded: adjacent plane indices are
        # adjacent squares, not adjacent values on a response curve, so the
        # bin-smoothness prior would be meaningless there.
        tables = (
            self.advantage_shared,
            self.advantage_route,
            self.decisive_shared,
            self.decisive_route,
            self.odd_latent,
            self.even_latent,
        )
        return sum(
            (table[..., 1:, :] - table[..., :-1, :]).square().mean()
            if table.ndim == 3 and table.shape[-1] == self.width
            else (table[..., 1:] - table[..., :-1]).square().mean()
            for table in tables
        )
