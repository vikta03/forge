# J48 assessment: terminal correction passed, negligible median speed change

J48 completed successfully in **302.463 seconds (5 minutes 2 seconds)** on its
approved two-core CPU allowance, with peak aggregate RSS about 432 MiB. No GPU,
training or games ran. The candidate remains isolated; the daily driver and
published engines have not been replaced.

The comparison baseline is **the corrected J47 engine**, not the original
daily driver. These results measure the additional terminal-recognition change.
They must not be interpreted as removing J47's earlier measured 0.74% cost or
as a direct cumulative comparison against the original engine.

## Correctness evidence

| Gate | Result |
|---|---|
| Direct search contracts | Candidate 39/39; J47 baseline 32/39 |
| Same candidate contracts under UBSan trap mode | 39/39, no trap |
| Retained J47 contracts | All 29 passed on both engines |
| New terminal contracts | Candidate 10/10; baseline 3/10 |
| Independent legal-existence and complete move-set comparison | Zero mismatches in 3,454 states, on baseline, candidate and sanitized candidate |
| Unchanged neural outputs | 122/122 exact comparisons, original versus J47 versus candidate |
| Existing native regression suite | Passed, including 28 perft positions and Chess960 |
| Incremental evaluator tests | 4,632 checks passed for each of Specialist and placement |
| Ponder/infinite lifecycle | All six cases passed |
| Legal searches, PV legality, MultiPV, stop/quit | Passed |

Seven newly failing assertions on the baseline are **not seven independent
bugs**. They exercise the same terminal-recognition problem through different
search exits, signs, cached data and stack limits.

The concrete terminal witness has Black Kh8, White Qf7 and Kg6, Black to move:
Black is not in check but has no legal move. With seeded static/cache scores,
the baseline returned **+200 or -200 internal evaluation units** in qsearch,
and +200 through demonstrated main-search shortcuts. The candidate returned
**0** in every one of these cases. At the artificial stack limit, the baseline
returned the actual neural score -1775; the candidate returned the terminal
draw. The negative controls preserved checkmate and a real king escape.

These seeded tests establish the control flow; they are not observations of
how often these cache values or positions occur in ordinary games. There is
also a production UCI child-search check: from
`7k/4Q3/6K1/8/8/8/8/8 w - - 0 1`, a depth-one search restricted to `e7f7`
returned `bestmove e7f7`, `score cp 0`. The root itself is nonterminal, so this
exercises the actual search of its stalemated child.

The 3,454 independent-oracle states include all 960 initial layouts, special
moves, mirrored cases and saved game prefixes. Their legal-move sets agree
with python-chess, and the helper leaves the board state unchanged. This is
finite verification of the exact algorithm, not exhaustive proof over chess.

## Cost screen

48 positions from 48 independent games, excluding J47 games, with twelve in
each opening/forcing/quiet/endgame stratum. Specialist model, quiet policy on,
Threads 1, Hash 32 MiB, one pinned CPU, cleared search state and equal warm-up.
Two rounds per position per engine at each budget, with ABBA/BAAB ordering and
alternating budget order: **384 timed searches**. These are position searches,
not a match.

| External time per search | Paired median wall-NPS change | Game-bootstrap 95% interval | Median completed depth, old → new |
|---|---:|---:|---:|
| 0.25 seconds | +0.015% | -0.281% to +0.199% | 16 → 16 |
| 1.00 second | -0.079% | -0.203% to +0.081% | 21 → 21 |

Both intervals include zero. The appropriate conclusion is **no detectable
median throughput penalty in this screen**, not a demonstrated speedup or a
guarantee of zero cost. The intervals resample these independent games; they
do not cover changes of hardware, thermal state, time control or all positions.

| Absolute descriptive median | J47 baseline | Candidate |
|---|---:|---:|
| Wall NPS, 0.25 s | 1,853,094 | 1,862,173 |
| Last displayed UCI NPS, 0.25 s | 1,871,960 | 1,875,130 |
| Wall NPS, 1.00 s | 1,826,208 | 1,824,509 |
| Last displayed UCI NPS, 1.00 s | 1,833,436 | 1,835,994 |

The headline percentages pair each position before taking the median; they
are not ratios of these unpaired descriptive medians. Wall NPS uses actual
nodes at stop divided by measured wall time. Displayed NPS comes from the last
completed-iteration UCI line. The very high native perft rate is move generation
only and must not be presented as neural search NPS.

## Regressions and changed choices

There were no positions with average NPS slowdown above the 5% review threshold
at either budget. The worst per-position average was **-2.91%** at 0.25 s and
**-4.32%** at 1 s. Median depth stayed unchanged, but individual trees changed:

- At 0.25 s: 42/48 positions had unchanged mean completed depth; four increased
  and two decreased. The largest decrease was two plies.
- At 1 s: 36/48 were unchanged, five increased and seven decreased. The largest
  decreases were five and three plies. These are reported depth differences,
  not adjudicated strength losses.
- **P311, rook ending:** depth **36 → 31** at 1 s in both repeats, still choosing
  **Ke3**. NPS changed -2.58%; the score changed from -66 to -51 cp. Selective
  depth changed **40 → 42**, showing why nominal depth alone is incomplete.
- **P039, minor-piece ending:** depth **40 → 37** at 1 s, still choosing hxg5.
- At 0.25 s, P367 changed `f2f3` to `h2h3` in both repeats. At 1 s both engines
  chose `f6a6` there.
- At 1 s, P158 changed `f7g7` to `f7g8`, and P053 changed `a2a3` to `e3e2`, in
  both repeats. Their relative chess quality has not been independently assessed.

The repeated differences show changes worth retaining for investigation, but
the screen does not trace each divergence to a particular terminal leaf.
Changed branching and finite-time iteration cutoffs both affect these results.
All FENs, iterations, scores, moves and PVs are retained in `timing.json` and
the UCI transcripts.

## Decision

**Retain the correction in the isolated search candidate.** It fixes the
demonstrated terminal fault, preserves neural outputs and passes the measured
cost gate. Playing-strength evidence remains outstanding, including the
combined J47/J48 bundle. The depth outliers belong in that assessment.

This does not establish the cause of any previously supplied drawn game, nor
does it certify state-of-the-art search. No additional hardware job was started
while reviewing these results.

The completed source-only hash recheck passed and did not launch hardware.
Frozen input lock:
`6aa2eeaec082dfab70d604887aecdb885c9492fa8b21b5de48313166da4e4627`.

Raw evidence: `completion.json`, `contracts.json`, `legality-comparison.json`,
`raw-equality.json`, `terminal-child.json`, `candidate-lifecycle.json`,
`timing-summary.json`, `timing.json`, and the retained native/self-test/UCI logs.
