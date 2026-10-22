# Experimental evidence

These are records of completed work, not fresh tests of a new binary.

| Files | Experiment and scope |
| --- | --- |
| J38-delivery.json; J38-validation-history.jsonl | Full-data placement training and export/delivery checks; includes monitor and full-validation entries |
| J40-verification.json; J40-timing.json | Policy integration correctness, unchanged static evaluation, disabled-path parity, UCI/relocation and small timing screen |
| J42-summary.json; J42-verification.json | Frozen 400-position study, paired/game-cluster statistics and integrity checks |
| J42-positions.json; J42-openings.json | Frozen positions with history and separate paired match openings |
| J42-games.pgn; J42-match-audit.json | 100 games, policy on 23W/55D/22L, independent legal-move/clock audit |

J42 used 8 seconds per side per game + 0.08 seconds/move, concurrency eight.
No demonstrated strength gain supports preferring policy off. See the complete
[interpretation and limitations](../ENGINE_GUIDE.md#20-tests-and-experimental-evidence).

## Recovery amendment

The original runner completed all 100 games and 345 positions, then incorrectly
rejected P280's legal early mate/depth-ceiling finish at depth 245. Recovery kept
all completed receipts byte-identical and added only the 55 missing positions.
No sample replacement or second match occurred. Any timed reading reaching the
ceiling causes that position's strict-time NPS/depth comparisons to be excluded
in **both** arms, while its common-depth result remains. This symmetric handling
was documented after observing the boundary case. Failed original output remains
in the local archive; the public records identify the amended final analysis.

## Publication transformations

Absolute local paths in JSON receipts are replaced by `[local]/basename`; JSON
formatting may differ. Original input hashes and published file hashes appear in
`../SOURCE_PROVENANCE.json`. Numerical results are unchanged. Validation history,
position/opening records and played move scores are factual records. The source
human-game corpus itself is not bundled; the frozen samples retain its hash.
No third-party annotations or artwork are relicensed as original writing.
