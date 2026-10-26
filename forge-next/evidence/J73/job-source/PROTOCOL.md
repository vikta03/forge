# J73 — the new FORGE models on the old draw concerns

Authorized 25 October 2026 by the user: “how do the new kids do on our previous
draw issues? i approve all jobs you need to find the results. i'll come back later”.

Compare the installed Wide160, Wide192/4×32 and Wide192/2×64 Reader64 packages.
Use their exact weights, adopted J47/J48 search and existing policy/search defaults.
No training, builds, engine modifications, downloads, GPU, or tablebases.
Original packages and earlier evidence remain intact. No automatic promotion.

Expected 40–65 minutes; maximum 90 minutes, two physical CPU cores, 4 GiB RAM.
Single-position measurements run sequentially on one fixed core. The final
continuations use two independent lanes, one core per game; their NPS is not a
controlled timing comparison. Charger, shared hardware lock, STOP files, process
timeouts, cgroup limits and a completion/failure notification apply.

## Frozen questions and cases

1. Reuse all 20 historical J64 positions, plus Scotch positions before Black's
   moves 8, 9 and 11 from J50. Preserve complete original move prefixes and clocks.
   Compare raw W/D/L, selected head, 2-second cold search and a separate 2-million
   node cold search. Equal nodes is a diagnostic, not equal playing time.
2. Before seeing new outcomes, select 12 focused positions: the four J64 current
   game roots 46/54/79/92; fortress 80/81; rook+bishop 64/65; Catalan 80; and the
   three Scotch roots. Add 10-second searches, and fresh 30-second searches at
   current54, fortress80/81, rook-bishop64/65, Catalan80 and Scotch11.
   Run 2-second no-history counterfactuals at all 12 (same board and clock).
   These counterfactuals intentionally discard repetition history.
3. Replay the Scotch game's 11 Black turns using its saved completed-iteration
   node budgets, preserving engine state. Replay all 95 Black turns of the
   recent opposite-bishop game at 250,000 nodes each, with 2-second probes at
   the four focus turns. Record fresh versus retained raw outputs and searched
   choices. This approximates state accumulation; it cannot reconstruct the
   GUI's unlogged partial iterations or exact time budgets.
4. Reuse the complete 100-independent-game J66 diagnostic panel. Obtain raw
   evaluations and fresh 2-second searches from all three models. Reuse frozen
   reference estimates and teacher labels with provenance. Report paired changes
   by stratum, including regressions; one position per source game permits a
   game-level bootstrap. This deliberately selected validation panel is neither
   a new untouched test set nor a gameplay failure-rate sample.
   The corpus includes Chess960 source games. Preserve their actual initial
   boards and castling rights; use king-to-rook UCI castling notation consistently
   for both ordinary and Chess960 histories. Do not drop those source games or
   silently reset their starting arrangements.
5. Fresh Stockfish 19, one thread, 128 MiB hash, no tablebases: 20 seconds at
   each of the 12 historical focus roots. Independently probe each distinct
   candidate choice from its longest timed search, the original played move and
   the reference's own move for 5 seconds with searchmoves. Longer unrestricted
   and shorter forced estimates are shown separately, with bounds preserved;
   they are not proofs or automatically trustworthy centipawn-loss labels.
6. Play 18 continuations: three models × six starts, each against the same
   Stockfish defender. Starts: fortress80 (White); fortress81 (White);
   rook-bishop64 (White); current54 (Black); Catalan80 (White); and J66's
   opposite-bishop draw witness corpus-1332191-109 (Black). Both players receive
   one second per move, one thread and 128 MiB hash. Every game starts cold,
   retains search state thereafter and receives full prehistory on every move.
   Stop only on actual chess termination/current threefold/current fifty-move
   claims, or at 200 added plies. No evaluation-based adjudication. A move cap
   is censored/unscored (*), never called a draw. Save every move/PV and PGN.
   Also analyse the final position for 10 seconds if it is still playable.
   One continuation per engine/start is case evidence, not a statistical match.

## Interpretation and checks

The rook-pawn before/after pair tests win/draw discrimination; avoiding h5 alone
does not establish successful conversion. The Catalan is a genuine winning
control, preventing “lower every evaluation” from appearing to solve the task.
No forced win has been established in the Scotch repetition or recent opposite-
bishop game. Continuing there is not automatically better than taking a draw.
Raw W/D/L is a prediction, not a calibrated probability guarantee. FORGE inverse
scalar values used for teacher comparisons are expected-score proxies; searched
scalars do not identify draw mass. Stockfish and FORGE centipawns differ in scale.

Validate all input histories and every printed PV; preserve final bestmove
separately from latest scored/unbounded PV. Check requested searchmoves, no
tablebase hits, engine identities, model hashes, head dimensions, CP/logit
agreement, UCI stop/quit, deterministic small fixed-node repeat, and raw equality
after retained/fresh state. Recheck frozen inputs after work. Audit all 18 PGNs
against move records and independently computed termination. Save partial
records and failures; never modify the old run or silently omit failed cases.

Outputs: RESULTS.md, summary.json, raw UCI logs, records/, continuations/,
audit.json, completion.json. progress.py prints one compact snapshot and ETA;
there is no chat polling loop. This job evaluates existing engines, not a repair.

## Startup compatibility correction

Attempt 20261025T132533Z stopped after 2.19 seconds at the first raw inspection,
before comparison searches. Its Wide160 JSON lacks selected_head; newer engines
print it. Preserve that failure. The reader now marks Wide160's route as derived
from its verified piece-count routing source, never as a printed measurement.
New-engine printed routes still must match. The retained actual JSON and wrong-
route rejection are covered by the smoke check. No engine/model/config changes.
Reviewed retry uses a fresh run directory and deducts all prior attempt seconds
from the same 90-minute allowance. No thresholds or chess checks are relaxed.
