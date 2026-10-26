# J47 — FORGE search corrections: verification and CPU cost

**Prepared, not authorized or executed.** Specific approval is required by the workspace `AGENTS.md` hardware rule.

Expected **3–6 minutes**, maximum **10 minutes** of worker wall time, at most **two physical CPU cores** and **4 GiB aggregate host RAM**. CPU only. **No GPU, training, games, downloads or strength sweep.** The service has a 30-second outer cleanup allowance. Charger must be connected. Existing original engines and releases are read-only inputs.

## Frozen inputs

- `baseline/`: exact Specialist package source; original hashes in `provenance.json`.
- `candidate/`: the five-file correction bundle described in `AUDIT.md`.
- Original Specialist binary and model, plus the fully trained placement model, specified in `config.json`.
- `positions.json`: first six eligible positions in each of the four J46 strata, preserving its order and excluding repeat games. 24 positions from 24 different games. No selection based on candidate output.
- Current test/client scripts and official reference sources. `source-lock.json` locks all prepared inputs; the job rechecks original hashes at completion.

## Ordered gates

1. **Clean builds.** Build baseline and candidate with identical production flags. Two simultaneous compiler drivers are allowed inside the two-core budget. Build a small direct-search harness for each, and an additional candidate harness with UndefinedBehaviorSanitizer. No sanitizer in production timing binaries.
2. **Existing native regressions.** Both builds run the existing `test` suite, including FEN, make/unmake, Chess960, SEE, cache tags and perft. Candidate runs the existing incremental-evaluator self-test with both Specialist and placement models.
3. **Direct search contracts.** Test pinned-EP repetitions and legal-EP controls; dead-material rules and negative controls; null make/unmake; normal versus explicit-limit time allocation; stopped qsearch; EP delta pruning with both SEE modes; dead draw before stand-pat; losing-mate RFP safeguard; depth encoding; recursive TT-slot replacement; per-path double-extension accounting; stopped null and ProbCut return handling. Candidate and UBSan candidate must pass every contract. Critical witnesses must fail on baseline as predicted. A missing reproduction is investigated, not silently counted as a fix.
4. **Independent rules oracle.** `fixtures.py` generates mirrored, FEN-root and replay cases. The current source-only preparation contains 36 histories / 184 states. Compare every state's draw predicate, full legal-move set and geometric-EP FEN with python-chess. Also compare raw keys/FEN/legal sets directly between old and new C++, establishing that the trained input representation remains unchanged. Additional states added before freezing are recorded by the job.
5. **Unchanged neural outputs.** 37 FENs per model (six standard, seven special, 24 frozen sample): original binary, rebuilt baseline and candidate must agree exactly on hidden activations, value logits, scalar and policy logits. Two models = 74 comparisons. No claim of unchanged search results: corrections are expected to change some trees.
6. **Search/protocol checks.** Both policy settings, legal short searches and full reported-PV legality; MultiPV; stop and quit while searching; fixed duration; ponder with node/depth limits; infinite with depth ceiling; terminal ponder/infinite roots. Queue deadlines detect premature bestmove output. Candidate must pass. Old protocol failures are retained as evidence.
7. **Small cost screen.** Specialist model; default search settings and policy enabled; Hash32 MiB; Threads1. Identical external 0.5-second stops, two rounds per position, ABBA or BAAB alternating between positions. One pinned CPU for both arms; no concurrent engine searches. Equal warm-up discarded; TT/history cleared before each measured search. DebugStats enabled equally to obtain actual nodes at stop. Report wall NPS, last GUI NPS, depth, seldepth, PV/choice and per-position regressions. Exclude a whole pair symmetrically if either hits the depth ceiling. This is a cost screen, not a statistical strength test.

The bootstrap resamples independent games (one position per game), 3,000 deterministic replicates. Report the paired median NPS change and its 95% bootstrap interval, depth changes and counts of >10% per-position slowdowns. **A median slowdown greater than 10% stops at a review gate.** Do not erase the correction to hide its cost. Changes to tree shape mean NPS is not a pure evaluator microbenchmark.

## Harness instrumentation and evidence limits

The direct harness includes a run-local copy of the actual search translation unit. Five small observation/stop-injection sites are inserted by `instrument()`; production builds are untouched. The TT witness deliberately replaces a slot, the extension fixture seeds exact child bounds, and EP tests seed chosen static values. The stop fixtures inject a stop at specific recursive return boundaries. These are isolated control-flow proofs, not ordinary-play frequency estimates. All source and generated instrumented copies are retained.

No code changes, retries, automatic parameter tuning, matches, original-engine replacement or publishing occur inside the job. On any gate failure, stop and retain logs/results. Inspect and document any repair before asking for or using a remaining-budget continuation. The runner refuses a second attempt by default.

## Execution and completion

Prepare/freeze (source-only):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 research/forge_search_audit_20261023/run_j47.py --freeze
```

After specific approval:

```sh
bash research/forge_search_audit_20261023/start_j47.sh --approve-j47
```

A user systemd service owns the job, independent of the chat/account session. It checks AC power, workspace `STOP_HARDWARE`, run-local `STOP`, the shared hardware-job lock, elapsed time and aggregate process-group RAM. It sends one desktop completion/failure notification. No chat polling. `latest-run.txt`, `worker.json`, `completion.json`, `resources.json` and `RESULTS.md` provide durable state and evidence. A fatal OS/service kill is also visible in the systemd service log if it prevents a receipt being written.

Before approval, only source reads/edits, hash/syntax checks and small independent chess-fixture preparation are permitted. No C++ compiler or chess engine has been run for J47 during preparation.

## Documented recovery after approval

The first approved run stopped at the absent shared UBSan library. See
`AMENDMENT.md`: use compiler trap mode with an instrumentation sentinel, reuse
the four successful builds, and charge the failed run against the original
ten-minute cap. Notification delivery was confirmed by the user. No engine,
fixture, model, timing-sample or rejection-criteria changes accompany the repair.
