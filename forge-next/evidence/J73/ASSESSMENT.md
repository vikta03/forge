# J73 assessment — capacity did not reliably repair the old conversion weakness

Completed 25 October 2026 at approximately 19:40 local time (Asia/Aqtobe).
The successful run took **72m14s**, or **72m16s** including the preserved initial
startup failure. Peak supervised RSS was about 950 MiB. This exceeded the
40–65-minute estimate but remained within the authorized 90-minute ceiling.
No GPU, training, builds, tablebases or engine modifications occurred.

**Decision: retain Wide160 as the daily driver.** The newer models show individual
improvements and regressions; J73 does not establish a general conversion or
defence advantage for either. It is a targeted comparison, not a strength match.

## The most important gameplay finding

All three models failed to convert the old rook-and-h-pawn versus bishop start
before move 80 within 200 added plies against Stockfish 19, at one second per
move for each side. Earlier independent analysis and the fresh 20-second
reference support a winning starting position.

The first four plies were:

| Model | Beginning of played continuation |
|---|---|
| Wide160 | Kg5 Kg7 h5 Bc2 |
| Wide192 | Rd7+ Kf8 h5 Kg8 |
| Reader64 | Rd7+ Kf8 h5 Bc2 |

All initially avoided the historical immediate h5, but all played h5 on their
second turn. Later they advanced h6 and h7; Black eventually captured the pawn.
The final positions contain only rook versus bishop and kings. Fresh ten-second
reference evaluations were +0.04, +0.09 and 0.00 respectively. Those final
positions support failed conversion at this budget; the exact first move losing
the win was not independently isolated. Do not label the second-turn h5 itself
a proven blunder: its position differs from the original historical witness.

Their last pawn capture occurred at added ply 184, 182 and 138 respectively.
The late pawn moves and captures reset the fifty-move clock, explaining why
these games could reach 200 added plies without a rule draw. Their recorded
result remains **unfinished**, not an adjudicated draw.

Starting after the original h5 and ...Be4 also produced no conversion in any
arm. More importantly, the network/search still misread that independently
investigated fortress:

| Model | Raw predicted win | 2-second search | 30-second search |
|---|---:|---:|---:|
| Wide160 | 93.8% | +6.52 | +6.52 |
| Wide192 | 86.7% | +5.23 | +5.23 |
| Reader64 | 89.6% | +5.91 | +5.94 |

The fresh reference was +0.33, consistent in broad assessment with the earlier
near-zero analyses. FORGE/reference centipawn calibrations differ. Their numerical
ratio is not meaningful. The stronger evidence is the continued very high raw
win prediction and failure to distinguish the winning and fortress neighbours.

## Genuine positives, without a new-model victory claim

Every model checkmated the defender from both the Catalan Be5–Bg7 start and the
rook-and-bishop winning start. Thus there are six actual wins, not just high
evaluations. Added plies to mate were:

| Start | Wide160 | Wide192 | Reader64 |
|---|---:|---:|---:|
| Catalan before White 80 | 95 | 131 | 147 |
| Rook/bishop before White 64 | 47 | 81 | 41 |

One continuation per model/start cannot establish a speed-of-conversion ranking.
The new models find Be5 earlier in the separate cold Catalan searches: at both
two seconds and two million nodes, where Wide160 chooses Rd8. By ten seconds all
choose Be5. Nevertheless, Wide160 also won the played continuation.

The remaining cases produced one rule draw (Reader64's current-game opposite-
bishop continuation, ending with insufficient material) and eleven capped,
unfinished games overall. Every capped final position received a fresh reference
check: all estimates were 0.00 except the +0.04 and +0.09 above. This supports a
draw-like final assessment but does not change the recorded game results.

## The 100-position diagnostic shows no reliable overall gain after search

Each position comes from a different source game. This panel was previously
selected and examined; it is not a fresh population sample or a failure-rate
estimate. Numbers measure disagreement with the saved teacher's expected score.
Search uses the inverse scalar map as a comparison proxy, not a recovered WDL.

| Model | Raw MAE, percentage points | After 2s search | Errors at least 15 points |
|---|---:|---:|---:|
| Wide160 | 16.840 | 4.590 | 5 |
| Wide192 | 17.458 | 4.618 | 7 |
| Reader64 | 15.998 | 4.571 | 11 |

Reader64's lower average raw error does not become an established searched gain.
Versus Wide160, searched changes were +0.027 points for Wide192, with paired 95%
interval [-0.421, +0.482], and -0.019 for Reader64, interval [-0.603, +0.583].
Both intervals include zero. Reader64 also has more cases beyond the selected
large-error threshold; that count is descriptive, not proof of an overall loss.

The old opposite-bishop corpus witness improves with Reader64 (+1.47 versus
Wide160 +2.96) but worsens with Wide192 (+3.43), against reference 0.00. Another
draw-like rook-and-bishop witness reverses that impression: Reader64 +1.72,
Wide160 +0.74, Wide192 +0.75, against reference 0.00. There is no uniform cure.

## Retained state and the Scotch draw

At all **21 matched warm/cold focus comparisons**, raw scores, hidden activations,
value logits and quiet-policy outputs matched exactly. This is evidence against
neural cache/state corruption in these cases. It is not a proof that every TT
bound, selective-search heuristic or history-dependent result is correct.

All three models chose the immediate Bc5 threefold repetition at the final
Scotch root in the retained-state replay. Reader64's occasional cold-search Bb4
did not survive that replay. The fresh reference also chose Bc5 with 0.00; no
forced Black win has been established there. Avoiding the draw would not by
itself demonstrate better chess.

## Score interpretation and independent verification

The generated report flags cases where the final bestmove differs from the last
unbounded printed PV. Never attach that PV's score to a different final move.
The corpus metric is a root/PV score comparison, not forced evaluation of the
final selected move. Such mismatches occur in 2, 2 and 1 corpus cases respectively.

Wide160's displayed +317.53 in the rook/bishop long search equals the source's
`VALUE_MATE_IN_MAX_PLY - 1` fallback value. It must not be treated as a normal
317-pawn neural evaluation or as a mate proof. This review did not trace the
exact internal call producing it; no new search correction is claimed.

The original job audited 590 comparison searches, 369 raw probes, 2,848,990 legal
PV plies and 18 continuation games. The separate saved-data review recomputed
all 100-position mean errors/counts, rechecked all 21 warm/cold raw comparisons,
verified installed binary/model hashes, and replayed every one of the 2,938
continuation plies against its original history and saved PGN. Terminal outcomes
and move caps agree. Reference continuation records report zero tablebase hits.
It launched no engines and did not repeat the original bootstrap or full PV audit.

The user's later Reader64-versus-Wide192 136-move bishop game was supplied after
J73 was frozen; it is saved separately and was not included in these measurements.
Automatic follow-ups remain paused at the user's request.

Evidence: [generated results and PGNs](RESULTS.md), [independent review](review/REVIEW.json),
[completion receipt](completion.json), and [full protocol](job-source/PROTOCOL.md).
