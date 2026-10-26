# J78 — isolated repetition-guard verification

The research candidate uses the unchanged Wide192 weights and J47/J48 baseline. Only a legal repetition that closes a non-PV zero-width window gets an early draw bound. The original daily driver is unchanged.

Completed in 89.76 seconds; cumulative approved job time 127.33 seconds; peak aggregate RSS 409.0 MiB across attempts. No GPU, training or games.
Two retained attempts stopped on runner parsing mistakes: an observation record lacked a test-result field, then a no-move result used the engine’s `(none)` spelling. All 61 contracts passed in the first attempt. The binaries were reused only after exact source and hash checks; no chess check was removed or relaxed.

## Correctness evidence

All 61 retained and new contract results passed, including sanitizer execution. The job checked 3,454 independent legal-move states, 20,400 repetition queries over 3,400 historical states, and exact disabled-option parity against the original on 18 roots. Existing rules, Chess960/perft and accumulator tests passed. Both the direct child-state oracle and independent python-chess history oracle agreed.

These are specified regression checks, not a proof of absence of every possible defect. They establish the intended lower-bound mechanism on the tested domain.

## CPU cost screen

24 positions from 24 independent games, six per stratum; cold 400 ms ABBA/BAAB searches. Median paired NPS change: **-0.27%**; 95% game-bootstrap interval **-0.63% to +0.47%**. Across-position median NPS: 1,716,005 off / 1,702,616 on. Median paired depth change +0.0; worst -3.0 plies.

NPS is affected by which nodes the modified search visits. Equal wall budgets and alternating order reduce timing bias; this small screen is not a strength result. All regressions remain in summary.json, including those hidden by a median.

## Historical roots

Full histories, cold one-second searches, tables disabled. Scores are the saved final exact displayed iteration, in the original side-to-move perspective. A score should not be attached to a different final bestmove.

| Position | Guard off | Guard on | Main cutoffs | Q cutoffs |
|---|---:|---:|---:|---:|
| fortress-80w | +5.47 | +5.56 | 48749 | 3684 |
| fortress-81w | +5.23 | +5.37 | 72462 | 10025 |
| rook-bishop-65w | +1.52 | +1.51 | 41377 | 3812 |
| catalan-80w | +2.56 | +1.18 | 12837 | 1297 |
| corpus-1332191-109 | +3.28 | +3.02 | 25311 | 1741 |
| corpus-364233-225 | +0.68 | +0.80 | 23972 | 5140 |
| corpus-1176670-264 | -0.48 | -0.38 | 22547 | 7399 |
| corpus-1630699-67 | +1.24 | +0.95 | 11063 | 1568 |
| corpus-1494057-118 | +1.97 | +0.63 | 19474 | 2473 |
| corpus-2036369-274 | +0.00 | +0.00 | 93573 | 24851 |
| corpus-761265-138 | M5 | M5 | 36048 | 3608 |
| corpus-1421554-75 | -2.11 | -1.97 | 14354 | 1238 |
| current-24b | +0.55 | +0.61 | 4847 | 476 |
| current-32b | +0.89 | +0.70 | 8701 | 1672 |
| current-44b | +1.10 | +1.10 | 11763 | 2011 |
| current-46b | +1.06 | +1.15 | 13097 | 2219 |
| current-54b | +1.03 | +0.79 | 14380 | 2293 |
| current-79b | +1.10 | +0.97 | 10452 | 1490 |
| current-92b | +0.88 | +0.97 | 11881 | 976 |
| rook-bishop-64w | +5.68 | +3.24 | 56702 | 2745 |

The mechanism fired on 20/20 roots; 553,590 main and 80,718 quiescence cutoffs. Independent review replayed 16,668 PV plies. 2 displayed-score/final-bestmove mismatches are listed in REVIEW.json.

On the 7 roots with saved exact tablebase move outcomes, off preserved the root outcome in 7 and on in 7. This is an offline lookup of already-saved move outcomes; neither search accessed a tablebase.

**The main fortress error remains.** Its one-second score changes +5.23 → +5.37, while the exact outcome is a draw. Another eight-piece root moves +1.97 → +0.63, closer to Stockfish’s near-zero search score, but finite-search agreement does not prove its game-theoretic outcome. Root scores also fall on winning examples, so lower scores cannot be counted uniformly as improvements.

Two ordinary cost roots lose three completed plies in both on/off repetitions; the worst NPS change is −12.82%. These outliers are repeatable within this short screen, despite the small median cost. This supports retaining a research candidate rather than declaring an overall upgrade without a strength test.

An active cutoff counter does not show that an earlier fortress mistake was repaired. Use the known win/draw controls and saved continuations to interpret root changes. No match has established a strength gain and no automatic promotion occurred.

## Files

Frozen run: workspace/research/forge_repetition_guard_20261026/runs/20261025T222642Z
Research binary: workspace/research/forge_repetition_guard_20261026/runs/20261025T222642Z/candidate-engine
Weights: workspace/engines/forge-capacity-wide192/models/capacity.fgc
Use `--model` with that exact checkpoint. The research option defaults off; `setoption name RepetitionGuard value true` enables the candidate.
