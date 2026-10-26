# J77 — Stockfish already recognises the clearest fortress before searching

Twenty saved roots, unchanged Stockfish 19 and Wide192, full histories, clean
state, one thread, 32 MiB hash. Stockfish table path empty, probe limit zero;
every reported search line has zero tablebase hits. Separate static `eval`
traces precede cold 100 ms, one-second and five-second Stockfish searches.
Wide192 gets native raw evaluation and a fresh one-second search.

## The main answer

In `fortress-81w`, after the losing h-pawn push and the bishop reply, Stockfish's
learned NNUE score is **+0.02 before search**. Its final static score is also
+0.02. Wide192's native static score is **+4.95**, then its one-second search
reports **+5.23**. Stockfish reports +0.35 / +0.19 / +0.11 at 0.1 / 1 / 5 seconds.
The position is an exact tablebase draw from J76, independently established.
The neural evaluator alone already distinguishes this root; its conclusion is
not supplied by table probes, correction-history updates or a long search.

Before the pawn push, `fortress-80w` is an exact win. Stockfish NNUE gives +2.82,
final static +2.70, and five-second search +6.46. This is a useful positive
control: the network is not merely assigning near zero to all rook-versus-bishop
material. The no-search comparison establishes recognition on these roots,
not flawless recognition of every related position or the cause of its learning.

Other positions need search. The eight-piece `corpus-1494057-118` starts at
Stockfish NNUE +1.52 / final static +1.59, then +0.05 / +0.01 / 0.00. In the
directly covered `corpus-2036369-274`, static is -4.74 but a short search finds
the drawing Bxg1; Wide192 finds that draw too. Thus both evaluator quality and
search matter. A near-zero finite search score on a larger position is not a
mathematical proof that every winning attempt fails.

## All roots

All scores use the original side-to-move perspective. Each engine's CP scale
is its own display scale; their magnitudes are not identically calibrated.
`NNUE` is Stockfish's displayed learned evaluation converted to CP, not raw
unscaled activations. `Static` includes its external scaling with zero optimism.
The static interface refuses positions in check, shown as unavailable.

| Root | SF NNUE | SF static | SF 0.1 s | SF 1 s | SF 5 s | W192 raw | W192 1 s |
|---|---:|---:|---:|---:|---:|---:|---:|
| fortress-80w | +2.82 | +2.70 | +3.03 | +4.67 | +6.46 | +5.85 | +5.47 |
| fortress-81w | +0.02 | +0.02 | +0.35 | +0.19 | +0.11 | +4.95 | +5.23 |
| rook-bishop-65w | +2.79 | +2.85 | +2.79 | +3.14 | +7.07 | +4.44 | +1.52 |
| catalan-80w | +1.53 | +1.51 | +3.52 | +4.32 | +5.21 | +0.79 | +2.56 |
| corpus-1332191-109 | +0.54 | +0.57 | +0.14 | +0.00 | +0.00 | +1.62 | +3.28 |
| corpus-364233-225 | +0.04 | +0.04 | +0.03 | +0.02 | +0.00 | +0.07 | +0.68 |
| corpus-1176670-264 | +0.02 | +0.02 | -0.21 | +0.00 | +0.00 | -0.19 | -0.48 |
| corpus-1630699-67 | +0.17 | +0.18 | +0.08 | +0.00 | +0.00 | +0.48 | +1.24 |
| corpus-1494057-118 | +1.52 | +1.59 | +0.05 | +0.01 | +0.00 | +2.92 | +1.03 |
| corpus-2036369-274 | -4.87 | -4.74 | +0.00 | +0.00 | +0.00 | -3.22 | +0.00 |
| corpus-761265-138 | +2.42 | +2.47 | M5 | M5 | M5 | +2.46 | M5 |
| corpus-1421554-75 | -3.17 | -3.41 | -4.26 | -5.13 | -5.72 | -2.05 | -2.11 |
| current-24b | +0.61 | +0.72 | +0.76 | +0.87 | +0.53 | -0.34 | +0.55 |
| current-32b | -0.04 | -0.05 | +0.08 | +0.06 | +0.12 | -0.03 | +0.89 |
| current-44b | — | — | +0.23 | +0.25 | +0.11 | +0.84 | +1.10 |
| current-46b | +0.13 | +0.13 | +0.13 | +0.09 | +0.00 | +1.06 | +1.06 |
| current-54b | +0.08 | +0.08 | +0.05 | +0.18 | +0.00 | +1.06 | +1.09 |
| current-79b | +0.32 | +0.24 | +0.15 | +0.12 | +0.12 | +1.28 | +1.10 |
| current-92b | +0.36 | +0.21 | +0.12 | +0.00 | +0.00 | +0.66 | +1.22 |
| rook-bishop-64w | +5.18 | +4.96 | +2.52 | M23 | M19 | +12.00 | +5.68 |

## The clock input is present and active

On synthetic copies of the drawn h-pawn board, changing only halfmove clock
from 0 → 40 → 80 → 99 changes Wide192 raw evaluation from +4.95 → +2.34 →
+0.89 → +0.57. The input is neither missing nor ignored. It is insufficient to
recognise the defence at a low clock. Stockfish's learned NNUE remains +0.02;
its external clock damping changes final static to +0.01 at clocks 80/99.

Wide192 feeds `floor(clamp(clock,0,255)/2)` into its reader, so adjacent 0/1
clocks intentionally share an input. Alongside it are material phase, own and
opponent castling rights, en-passant square, total piece count, check flag and
a constant 32. Side to move determines perspective. Search separately handles
actual repetitions and the fifty-move rule. Unlike VEGAM, this evaluator does
not receive eight past boards. VEGAM's backend also explicitly receives its
clock as `GetRule50Ply()/100` and history/repetition planes.

## Search mechanisms: presence versus quality

Wide192 already uses correction history in both main search and quiescence.
Default `legacy` has pawn-structure and legacy non-pawn-context tables, with
scale 25. `piece` and `full` modes already add minor-piece, separate white/black
non-pawn and continuation contexts. Enabling an existing mode would be a
search experiment, not fixing a missing feature. FORGE's continuation correction
key also includes the whole current position: that is not equivalent to
Stockfish's more transferable move-pair context. More tables alone do not prove
better learning, and their scale/updates need model-specific validation.

Existing FORGE: aspiration/PVS, TT, null-move verification, ProbCut, singular and
check extensions, LMR, futility/SEE/history pruning, capture/quiet/continuation
history, policy ordering, current-position repetition and terminal draw guards.
J47/J48 repaired demonstrated correctness failures; they did not certify optimal
selectivity, strength, or equality with Stockfish's search.

Concrete remaining differences include advance repetition detection before
selective pruning, recapture-aware qsearch move-count pruning, pawn-structure
quiet-move history, and using correction magnitude in reduction decisions.
These are separate mechanisms with interacting costs. Do not combine them into
an untested claim of state-of-the-art search or silently replace the daily driver.

## Learned relational features

The pinned Stockfish source has explicit threat and pawn-pair feature tables.
Its halfmove adjustment is outside the NNUE; it is not the same learned input
as FORGE's clock. Adding joint features to FORGE is possible but requires trained
weights and incremental-update verification. It cannot be accomplished by
enabling a search option in the existing checkpoint.

Prior FORGE graph, polynomial, ordered-line and local-triplet work already
tested related ideas, with distinct cost/training outcomes. A narrower pawn-pair
cache would differ from the expensive per-piece contact graph, but a pawn-pair
feature has no active pair in the one-pawn fortress. It cannot be presented as
a direct explanation or promised cure for that witness. The comparison shows
that useful recognition exists in another compact CPU evaluator; it does not
identify which representation or training change would reproduce it here.

## Verification and limits

Run completed in 140.43 seconds, with peak aggregate RSS 260.5 MiB. All 80 searches and 37,867 displayed PV plies passed checks. Originals and model hashes are unchanged. No build, training, game or GPU work ran.

3 search records have a final bestmove differing from the last exact displayed PV. Scores in this report refer to the saved displayed root evaluation; do not attach them to a different final move. Raw transcripts and all bound-labelled iterations are retained. The twenty roots are a diagnostic set, not a random strength sample.

Evidence: `comparisons.json`, `clock-sensitivity.json`, `pv-endpoints.json`, engine identities, `audit.json`, `REVIEW.json`, and `logs/`.

Run: workspace/research/forge_stockfish_gap_20261026/runs/20261025T215950Z
