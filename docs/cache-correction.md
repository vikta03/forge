# A score belongs to a board and a clock

FORGE's network consumes the number of halfmoves since the last capture or pawn
move. Its base evaluator also uses that clock. The same piece arrangement at
clock 3 and clock 53 can therefore have different static scores.

The inherited table normally identifies a position by its board key, and adds
the exact clock to the search key only from `Rule50TTThreshold` (default 80).
Previously an entry below that threshold could supply a static evaluation
computed at a different clock. That is a mismatch between evaluator inputs and
cache identity. It does not prove any particular historical loss was caused by it.

The release keeps the existing search-bound key policy. Each cached **static**
evaluation has its own one-byte clock tag. Main search and quiescence recompute
the static evaluation when the tag differs. Values from other clocks cannot be
silently reused as if their evaluator inputs matched.

An accepted table payload write changes both evaluation and tag. A rejected
shallow write changes neither. Refreshing only the static evaluation preserves
the bound, depth, move and key. A missing evaluation has an invalid tag. Clear
and resize reset tags. Clocks 0–254 are represented exactly; larger values use
the invalid marker and always recompute.

Three tags accompany each unchanged 32-byte cluster. A requested Hash budget
therefore funds 35 bytes per cluster, rounded down to preserve alignment.
It holds about 8.6% fewer clusters than the old allocation at the same setting.
Allocator bookkeeping is outside that byte budget, as with ordinary allocations.

This ports the evaluation-tag mechanism from the earlier `src_ttclock`
experiment and adds explicit memory accounting and ownership regression checks.
It does not replace the search-bound key policy, alter weights, retune pruning,
or promise stronger play.

The earlier clock-tag experiment, on a different experimental configuration,
finished 40 wins / 116 draws / 44 losses in 200 games; its reported interval
included zero. It is inconclusive strength evidence and is not a result for
this release. Historical netB matches used the original uncorrected binary.

Verification covers distinct clocks, same-clock reuse, accepted and rejected
replacement, tag indexing, absent evaluations, clear, shrink/resize, Hash
accounting, and instrumented execution of both main search and quiescence.
The production build excludes the audit counters.
