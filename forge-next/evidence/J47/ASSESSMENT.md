# J47: verified search corrections with a small CPU cost

The isolated candidate passed every required gate. The corrected search is
approximately 0.74% slower in this short cost screen, with no median change in
completed depth. The targeted tests reproduce faults in the original and show
that the candidate corrects them. Playing strength has not been measured.

## What actually passed

| Check | Original/rebuilt baseline | Corrected candidate |
|---|---:|---:|
| Direct search/rules/time contracts | 9/29 pass | 29/29 pass |
| Same direct contracts under UndefinedBehaviorSanitizer | Not required | 29/29 pass, no trap |
| Independent rules states | 12/184 mismatches | 0/184 mismatches |
| Existing native regressions, including Chess960 | Pass | Pass |
| Native perft fixtures | 28/28 pass | 28/28 pass |
| Incremental evaluator self-test, Specialist model | Not rerun | 4,632 checks pass |
| Incremental evaluator self-test, placement model | Not rerun | 4,632 checks pass |
| Raw network comparisons with the original binary | Exact agreement | Exact agreement, 74/74 |
| Six ponder/infinite lifecycle cases | 2/6 pass | 6/6 pass |

The 29 contracts include positive and negative controls and several cases for
one mechanism. Twenty failing assertions do **not** mean twenty independent
bugs. The separate observation about quiet TT moves entering qsearch is not
counted as a passing correction; it remains an ablation candidate.

The 184 rules states come from 36 histories, including mirrored and FEN-root
cases. The baseline mismatches are draw recognition in pinned-en-passant
repetitions and elementary dead-material positions. Legal-move sets and raw
position keys/FENs agreed between the two C++ builds. Negative controls prevent
claiming draws merely because a position has few pieces.

The 74 raw-output comparisons comprise 37 FENs for each model. Hidden values,
value logits, reported static score and quiet-policy logits matched the original
binary exactly. This establishes unchanged neural outputs on these fixtures;
search results are allowed to differ. Full PV legality, MultiPV, both policy
settings, and stop/quit checks also completed in the worker.

## Concrete behavioral changes

- A pinned pawn's unusable en-passant right no longer hides a threefold
  repetition. A genuinely legal en-passant right still distinguishes positions.
- Basic dead-material draws are recognized before a nonzero neural stand-pat
  value can masquerade as an advantage. This uses chess rules, not tablebases.
- En-passant captures survive the demonstrated erroneous delta-pruning case.
- A recursive search replacing a transposition-table slot no longer changes
  the parent's already-probed decision data. A double extension assigned to
  one child no longer consumes its sibling's path budget.
- Injected stops no longer turn the unfinished null/ProbCut child into a result
  or cache bound. Stopped qsearch leaves the seeded cache entry untouched.
- Losing-mate windows bypass the demonstrated reverse-futility shortcut, and
  extreme extended depths stay representable in the TT depth field.
- `go movetime 300` used about **291 ms** instead of **256 ms** in the lifecycle
  check; the retained 10 ms overhead explains the intended approximately
  290 ms search allowance. Ponder/infinite modes now wait for the appropriate
  command instead of emitting `bestmove` prematurely in the reproduced cases.

The TT replacement, stop injection, extension and depth-ceiling witnesses are
deliberate control-flow tests. They establish the mechanism, not how frequently
it occurs in ordinary games. The mate-window fixture is a pruning safeguard
test, not proof that every forced mate is now found.

## Speed and depth

Specialist model, policy enabled, Threads 1, Hash 32 MiB, one pinned CPU for
both arms, equal warm-up, cleared search state, and external 0.5-second stops.
There are 24 positions from 24 independent games: six opening, six forcing,
six quiet and six endgame positions. Two rounds per position in ABBA/BAAB
order give 48 searches per engine, 96 total. No search hit the depth ceiling.

| Measurement | Baseline | Candidate |
|---|---:|---:|
| Median wall NPS across all 48 searches | 1,835,867 | 1,827,573 |
| Median last displayed UCI NPS | 1,855,519 | 1,845,906 |
| Median completed depth | 19 | 19 |
| Completed depth range | 16–24 | 16–24 |
| Actual measured duration range | 500.32–501.41 ms | 500.36–500.75 ms |

The primary paired statistic is **-0.7443% NPS**, with a position/game bootstrap
95% interval of **[-0.9356%, -0.3872%]**. This compares each position with itself
before taking the median; it is not the ratio of the two overall NPS medians.
The interval reflects resampling these positions, not uncertainty across other
laptops, thermal conditions, days or time controls.

No position had an average slowdown greater than 10%. The worst average was
-1.86%; the best was +1.75%. Average completed depth was unchanged in 21/24
positions, down 0.5 in one, down 1 in one and up 1 in one. Two of 48 same-repeat
move comparisons differed (P164 repeat 0 and P248 repeat 1). Equal-time stopping
and changed search trees can both contribute; these choices have not been
independently adjudicated as better or worse.

Wall NPS uses actual nodes at stop and externally measured time. Last displayed
NPS comes from the most recent completed-iteration UCI line, which can precede
the stop substantially. These measurements must not be confused with the much
higher move-generation-only perft rate in the native test logs.

## Run integrity and recovery

The first attempt stopped at the missing shared UBSan runtime after 51.677 s.
It is retained in `../20261023T145456Z/`. The recovery reused the four successful
builds with hashes and used compiler trap-mode sanitization. A sentinel exited
normally for safe input and trapped on deliberate signed overflow, confirming
instrumentation was active. No runtime download or system repair was needed.

The resumed job completed in 74.964 s: **126.640 s cumulative**, within the
approved ten-minute limit, on the same two-core allowance. No GPU, games or
training ran. The final source-only preflight passed without launching hardware
work. Frozen input lock:
`eca1f164307ed88e1a6023397e9f60d1f5a1a22ee9561def34e9c96273c4a224`.

The approved protocol, audit and amendment remain frozen with their original
preparation/approval wording. This assessment and `JOB_STACK.md` record the
completed status. Originals and published releases remain unchanged; the
candidate has not been promoted.

## Decision and remaining work

The correctness and cost gates are cleared. Keep the corrections together as
the verified candidate for the next strength screen. Neither the small NPS
cost nor these passing tests establish an Elo gain or state-of-the-art search.
The audit's remaining search-quality hypotheses still require separate,
controlled experiments. No further hardware job was started by this review.

## Evidence files

- `completion.json`, `resources.json`, `worker.json`: completion and limits.
- `contracts.json` and the three contract logs: direct before/after witnesses.
- `rules-differential.json`, `rules-expected.json`: independent rules evidence.
- `raw-equality.json`: exact model-output comparisons.
- `lifecycle.json` and `protocol-*.uci`: UCI behavior.
- `timing.json`, `timing-summary.json`, `timing-*.uci`: individual timings,
  searches and paired summary.
- `baseline-native-tests.log`, `candidate-native-tests.log`, `selftest-*.log`:
  native and incremental-evaluation regressions.
- `ubsan-sentinel.json`, `resume.json`, `builds.json`: sanitizer recovery and
  build provenance.
