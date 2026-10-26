# J54 — zero-block skip with unchanged input packing

Prepared 24 October 2026; **awaiting specific hardware approval**.

Expected **5–7 minutes**, maximum **10 minutes**, two physical CPU cores and
4 GiB aggregate host RAM. CPU only: no GPU, games, training, downloads or
automatic promotion. Charger required; avoid other substantial CPU work during
timing. The estimate uses J52's saved ~1.2-second control searches, 288 new
searches and two builds. The supervisor stops at 600 seconds; service limits
allow only teardown grace, not continued computation. Preserve a partial run
on failure; no automatic restart or allowance reset.

## Question and exact change

J52's specialized packing loop lost vectorization and the candidate slowed
search by 9.07%. Its two block products were actually removed. This follow-up
changes only the dense block traversal and dispatch, retaining the adopted
packing/range-check/scalar-fallback source verbatim. It is a revised
implementation of an already attempted optimization, not a new architecture.

Placement-only inputs 256–319 are guaranteed zero. The nine visited AVX2
blocks are 0–7 and 10; metadata stays at 320–327, padding at 328–351. No weights
are removed or rewritten. Only Mode 1 selects the specialized kernel. Other
modes retain all eleven blocks. Static assertions bind the mapping to current
dimensions; audit mode verifies the zero-input invariant. New self-test cases
compare the skipped kernel with the scalar reference using nonzero weights
in the omitted columns, live metadata, signed extremes and scalar fallback.

Unchanged packing source is a design constraint, **not a claim about compiler
output**. Preserve generated assembly and inspect packing vectorization after
the build. Never call this a speed improvement merely because two blocks vanish.

## Sources and arms

All new source starts from the adopted combined J47/J48 baseline:
`workspace/engines/forge-specialists-corrected`.

1. **Shipped:** original binary, read only.
2. **Control:** pristine copied source rebuilt with its original build script.
3. **Candidate:** the same source with only `src/forge_next_probe.cpp` changed.

All use the unchanged Specialist model and existing search/policy defaults.
Compiler flags remain `-O3 -flto=1 -march=haswell -mtune=generic`, with the same
seed and path remapping. Record compiler identity, source manifests, binaries,
model hash and assembly. Retain the baseline provenance, licensing, rule tests
and search fixtures; no old search tree is substituted.

## Gates

1. Validate frozen source/fixture/model/binary identities before and after.
   Build the two isolated binaries sequentially. Save every build failure.
2. Run loaded-model incremental self-tests for all three arms: mirrors,
   special moves including Chess960, undo, lazy synchronization and clock
   invalidation. Candidate also exercises both dense-kernel variants against
   the independent scalar loop. No full 845M-node move-generation rerun is
   needed for this evaluator-only change.
3. Require identical hidden activations, raw value logits, scalar evaluations
   and every legal quiet-policy logit on the retained 61 boards.
4. Compare short exact search trajectories with quiet policy on and off.
5. Warm and measure eight head fixtures, 100,000 calls per observation, two
   reversed orders. This includes packing/mixer/value conversion; it excludes
   board updates and is not gameplay NPS.
6. Use the same frozen 48 full-history positions from 48 independent games:
   12 openings, 12 forcing middlegames, 12 quiet middlegames and 12 endings.
   Two million requested nodes, one thread, 128 MiB hash, QuietPolicy enabled.
   Three arms and two reversed orders: **288 timed searches / 576M requested
   nodes**, plus bounded small correctness/warmup searches. Alternate which
   arm starts each position. One engine searches at a time on the same core.
7. Require identical actual nodes and full reported iteration trajectories,
   scores, bounds, PVs and best moves. Replay PVs for legality. TT/history are
   cleared outside each timer. Include the J48 stalemate-child witness and
   stop/readiness/quit as success gates.
8. Average repeats within each game, then report paired NPS/time changes,
   game-bootstrap 95% intervals, per-stratum figures and worst positions.
   Compare candidate/control, candidate/shipped and control/shipped. Preserve
   all raw observations and report uncertainty. This is a known engineering
   fixture set, not an untouched strength test; no Elo claim.

Any build, exactness, legality, lifecycle, identity or resource failure stops
the job. A slowdown is a completed negative result, not a concealed failure.
If gain is absent or uncertain, do not promote. Even a gain only supports
evaluation throughput with identical sampled behavior; it does not prove a
playing-strength increase. The current GUI engine is never overwritten.

## Launch and completion

After specific approval only:

```bash
bash workspace/research/forge_zero_skip_20261024/start_j54.sh --approve-j54
```

Without that flag the script performs source-only checks and starts no engine.
The job uses the existing shared hardware lock, charger/STOP/RAM/wall guards,
durable user service and one desktop completion/failure notification. A user
can stop it by creating `STOP` in the run directory. There is no chat polling.

Preparation checks do not establish native correctness or speed. Those remain
pending until this job actually runs.
