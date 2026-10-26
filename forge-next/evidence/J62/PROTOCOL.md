# J62 — production full-key Wide160 and NPS

Authorized by the user's 24 October instruction: “okay do it and get nps.
w160 is my daily driver now”. Expected 4–7 minutes, maximum 10 minutes,
two physical CPU cores, 4 GiB aggregate RAM; no GPU, training, downloads or
games. Preserve original packages and frozen runs. No automatic retries.

Copy the exact Wide160 source/model. Keep its 32-byte three-entry clusters,
16-bit fast filter and existing age/replacement policy. Replace each one-byte
clock tag with seven bytes: six remaining key bytes and the clock byte.
Verify all key bits at probe and recheck ownership at final store, including
stale writer pointers. Refused same-key writes retain clock/eval ownership.
No diagnostic per-node arrays or counters are added. Storage is fully charged
to Hash: 53 rather than 35 bytes per three slots, approximately 34% fewer
slots at equal memory. Do not call this a strength improvement.

Build the candidate and unmodified Wide160 control using the same compiler,
fixed Haswell target and flags. Gates: direct TT contracts including false
short-tag hits, stale writers, no-move replacement, refused writes, raw clock
refresh, zero keys, clear/resize, replacement age and exact memory accounting;
repeat contracts under trapping UBSan with a sanitizer sentinel. Existing
rules/Chess960/perft, incremental evaluator, UCI readiness/stop/quit, retained
stalemate regression, missing/corrupt model errors and relocated launch from
a directory containing spaces. Raw evaluator outputs must match originals.
Rebuilt control must reproduce the original at fixed nodes.

Timing: same Hash64, one engine searching at a time, same physical CPU core,
unchanged quiet-policy defaults. Three 3-second start-position repetitions;
48 frozen positions from independent games, two 1-second searches per arm
with alternating order. Additionally replay J60's four recorded histories,
64 successive 250k-node positions each, then two 2-second warm endpoints,
retaining state. Rotate arm order by history. Record wall NPS, actual nodes,
GUI NPS, depth, legal PV/move, occupancy and elapsed time. Report paired median
ratios, game-grouped bootstrap interval and regressions. These compare actual
search throughput; changed trees prevent treating them as pure instruction cost.

Retain every failure and stop. A slowdown over 10% must be called out, not
hidden by changing Hash or dropping the verification. On success, assemble a
separate relocatable `engines/forge-wide160-fullkey` package with exact source,
weights, licence/provenance, results and checksums. No old path is overwritten
and no GUI setting is changed. A desktop notification reports job completion;
the assistant waits for that event without status polling.
