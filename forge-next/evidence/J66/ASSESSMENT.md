# J66 assessment — 25 October 2026

**Search resolves much of the raw disagreement, while a smaller set of
conversion and defensive-value disagreements survives.** The results justify
more selective learning data, not a blanket endgame correction or a claim of a
new search bug. They also show why raw validation loss alone cannot tell us
which errors matter during play.

J66 completed in **688.70 seconds (11m29s)** with **412.11 MiB** peak monitored
aggregate RSS. All 13 diagnostic gates passed. The isolated probe reproduced
the installed engine's evaluation and fixed-node search trajectories on eight
controls. All 100 corpus games and four historical witnesses were measured;
no histories were excluded and every bounded quiescence probe completed.
No installed engine changed, and no training, GPU work or games occurred.

## What ordinary search repairs

The table measures disagreement with the stored Lc0 teacher in expected-score
percentage points. For searched scores, the inverse FORGE scalar map gives a
comparison proxy, not a calibrated searched WDL distribution. A lower number
means closer teacher agreement, not directly more wins.

| Frozen stratum | Games | Raw | Quiescence | 250 ms search | 2 s search |
|---|---:|---:|---:|---:|---:|
| J65's largest selected raw errors | 8 | 54.22 | 22.98 | 6.59 | 6.58 |
| Quiet disagreements | 24 | 18.81 | 18.81 | 8.34 | 6.52 |
| Target-family controls | 20 | 14.94 | 3.01 | 1.33 | 1.60 |
| J65 category-random panel | 24 | 10.59 | 7.35 | 6.25 | 5.56 |
| Ordinary controls | 24 | 10.24 | 9.70 | 5.87 | 5.55 |

Of 44 cases with at least 15 points of raw disagreement, **36 fall below that
threshold after two seconds**; **23 come within five points** of the teacher.
Quiescence alone brings 12 of the original 44 within five points. These groups
overlap. Eight original large disagreements remain, and one other case grows
past the threshold during search: nine residual cases altogether.

All eight extreme cases from J65 fall below the 15-point threshold. Their
mean error drops from 54.22 to 6.58 points. Treating them as eight new gameplay
failures would have substantially overstated the evidence.

The quiet panel also improves with full search. Its quiescence values barely
change because no immediate legal captures or promotions are available.
That does not make those positions tactically or strategically trivial.

## Persistent overconfidence is real in this panel

Two new independent corpus examples are useful witnesses:

**Game 1332191, frame 109: opposite bishops.** The teacher predicts W/D/L
4.94/94.87/0.19%. Wide160 starts at +0.95, reaches +2.70 in quiescence, then
reports +2.96 at both full-search budgets, including completed depth 41 at the
longer budget. The reference reports 0.00 at both four and twelve seconds.
Both engines choose ...f5, so this is a strong value disagreement, not evidence
that FORGE's first move is wrong. The reference continuation begins
...f5 fxe4 fxe4 Ba2, with the opposite bishops maintaining different-colour
control. Finite search supports a drawn assessment but is not a proof against
every possible continuation.

FEN: `8/8/5p1k/8/4p2p/p3PP1P/1b6/1B1K4 b - - 0 55`.

**Game 1494057, frame 118: rook+bishop versus rook with pawns.** Teacher draw
probability is 96.55%. FORGE's raw +3.48 decreases to +1.55 after two seconds,
while the reference stays at 0.00. The saved reference line shows why material
alone is misleading: after Bg3 Rf1+ Ke2 Kb6 Rc4 Ra1 Bd6 a3 Rxb4+ Kc6 Be5 axb2
Bxb2 Rb1 Bc3 Rxb4 Bxb4, the pieces simplify into king+bishop versus king.
That is a legal defensive example, not an exhaustive demonstration that every
White try fails.

FEN: `8/k1B5/8/2R5/pp6/5K2/1P6/4r3 w - - 0 60`.

The old rook-pawn fortress remains a particularly clear unresolved blind spot:
after the critical pawn push and bishop reply, raw WDL still assigns **93.81%
win probability**, quiescence returns +6.43, and two-second search returns
**+6.72**, while the independent reference gives **+0.10**. This agrees with the
earlier diagnosis. J66 does not repair it.

There is a second caution: a pawnless rook+bishop versus rook corpus position
starts close to the teacher (+0.14, only 1.83 points of disagreement), but search
raises it to +1.07, versus reference +0.10. It stays below the prespecified
15-point flag but is relevant. Root-only raw-error mining would miss it.

These are consistent with errors among evaluated descendants being selected
and propagated by search. That is a hypothesis about mechanism, not a traced
causal finding: J66 did not record every leaf, disable pruning, or isolate
correction-history contributions. The evidence does not distinguish all those
possibilities or establish a new implementation defect.

## Underconfidence matters too

Of the nine residual cases, **two are teacher/reference draw-like positions
that FORGE overvalues; seven are decisive positions whose scalar confidence is
less extreme than the teacher's**. Several involve opposite bishops with rooks,
where the teacher and reference agree there is a substantial winning advantage.
For example, game 680658/frame 103 has teacher W/D/L 82.73/15.98/1.29%; FORGE
reports +1.37 after two seconds and the reference +3.21. Both select ...g5.
The reference continuation uses the advancing pawns, active king and rook;
opposite-coloured bishops do not make the whole position drawn.

**Six of the nine residual cases share the reference's final first move.**
Their differing values must not be counted as six demonstrated move errors.
The other three move disagreements are investigation leads, not automatically
three blunders: J66 did not independently compare forced alternatives.

Historical winning witnesses are also retained. At the older rook+bishop
position, FORGE chooses Kf6 and reports +1.88; the reference chooses Rh7+ and
reports +6.98. In the Catalan witness, FORGE chooses Rd8 at +1.45; the reference
chooses Be5 at +5.08. These results show why training only a stronger draw
preference would be an inadequate response. They do not prove that FORGE's
alternative move throws away the win.

## Limits and checks

The sample deliberately overrepresents prior errors and rare material families.
**Nine out of 100 is not an estimated gameplay failure rate.** No match or
strength claim follows. The four historical positions come from three games
and are excluded from corpus statistics.

The full-search budgets ran for about 0.24 and 1.99 seconds, allowing the
engine's normal 10 ms overhead. Median completed depths across 104 positions
were 19 and 28; these are selective alpha-beta depth values. Quiescence took
at most 33 nodes and one reported millisecond in this panel, with no interrupted
results. All searches began with cold TT/heuristic state and retained actual
game histories. Warm gameplay behaviour was not measured here.

Reference analysis uses a different engine and calibration. Its baseline
budgets were four seconds or retained fifteen-second J65 analyses; selected
follow-ups were fresh twelve-second searches. Centipawn magnitudes between
engines are not interchangeable. The nine residual cases have reference
outcomes broadly consistent with the teacher's draw/decisive direction, but no
label-error prevalence or automatically corrected labels are established.

The saved-evidence review matched 104 raw and 104 quiescence outputs to native
logs, checked 216 original-engine search records, eight probe controls and 84
new reference searches, and legally replayed **156,548 PV plies**. All **113
frozen input files** remained unchanged. Three other budgeted searches returned
a final bestmove differing from the latest unbounded PV start; those are kept
separate, and none belongs to the nine residual cases or historical witnesses.
Generated reports and original run artifacts have been preserved.

## Recommended next decision

Prepare a **matched-budget targeted-learning experiment using the same Wide160
architecture**, preserving runtime arithmetic. The data already contain many
relevant games. The intervention worth testing is greater learning emphasis on
difficult quiet conversion/defence examples, balanced with genuine wins and
ordinary play. It is not established that this will beat the current engine.

Selection should include both high raw disagreements and positions where
search amplifies an initially small error. This panel can guide the mining
criteria but must not be recycled into the training split. Mine independent
training games, keep histories/provenance, and reserve separate games for
candidate selection and final assessment. Use existing searched soft WDL as
supervision where defensible; uncertain independent disagreements need review,
not automatic relabelling from short reference searches.

Compare the candidate with both an unchanged daily driver and a control trained
for the same budget on ordinary data. Judge held-out conversion and defence as
well as general quality. A merely lower training loss, or beating a regressed
control while remaining worse than the daily driver, is not success.

No new heavy data-mining or training job is prepared or authorized by this
assessment. J66 is complete; the installed daily driver remains intact.

## Evidence

- [Complete generated results](RESULTS.md)
- [Saved-data review](review/REVIEW.json)
- [Native FORGE records](forge.json)
- [Independent reference records](reference.json)
- [Full histories](contexts.json)
- [Execution receipt](completion.json)
- [Original protocol](job-source/PROTOCOL.md)
