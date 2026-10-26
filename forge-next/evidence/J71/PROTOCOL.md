# J71 / J72 — Wide192 with two 64-unit readers

The user asked to set up this second model after Wide192 and explicitly chose
**400 games against the first Wide192**. Their existing authorization covers
correctness/cost tests through full training, a ready engine and the match.
The current J69/J70 job and the original daily driver remain untouched.

**Current admission revision, 25 October:** after reviewing the completed CPU
screen with the user, continue this experiment with NPS as a reported trade-off.
The measured depth cost is modest. Retain all correctness, depth, time and memory
limits and the 400-game comparison. The original numeric NPS vetoes below remain
documented as the original plan; the explicit review at the end supersedes them
for this one continuation. The failed receipts are not relabelled as passes.

## Architecture and comparison

- First model: 24,576 × 192 placement table, two summed viewpoints + 8 state
  values = 392 inputs; four 392 → 32 readers, with value and policy-query outputs.
- New model: the same placement architecture and inputs; **two 392 → 64 readers**,
  with the same 2-logit WDL output and 16-dimensional quiet-policy query.
- Fixed hard routing: head 0 for **2–16 pieces**, head 1 for **17–32**, counting
  both kings. One reader executes. No learned routing or inference ensemble.
- At 16 and 17 pieces, the other head gets 0.25 relative WDL supervision weight,
  normalized with the primary head's weight 1. Policy trains the selected head.
  This retains the earlier boundary-overlap mechanism at the new sole boundary.
- Total trainable parameters stay **4,795,776**: 4,718,592 placement, 52,480 reader
  weights, and 24,704 shared move-table weights. Active reader capacity doubles;
  fewer stored specialists see broader game phases. This is not a universal
  increase in representational capacity: specialization is traded for width.

Both use scratch seed 20261025, the exact same compressed corpus, sample order,
optimizer and 12-pass schedule. Initial placement values/common reader columns
match their shared seed construction; learned placement weights can diverge.
The comparison tests the reader design (width, head count and routing together),
not width alone. One training seed and a short match cannot establish universal
superiority. There is **no new Wide160 or Wide192 control training**.

## Queue and provenance

An inotify watcher waits for successful completion of the **entire** J69/J70
pipeline, including its first 400 games. It subscribes before checking status,
so a completion cannot fall between inspection and subscription. No periodic
polling or chat wakeups. The wait expires after 24 hours; failures or explicit
STOP halt the queue with a notification. It survives chat/account changes while
this Linux user session remains running; it is not a reboot-resume service.

Before starting, authenticate the first Wide192's 12-pass receipt, selected
checkpoint export, verified package, full-validation sufficient statistics,
source, 400-game audit and checksums. The new match opponent must be that exact
Wide192/4×32 binary and model; no fallback to Wide160. Check matching recipe fields
and source equality outside evaluator/identity changes. Pin the resolved files
in the new run, retain source snapshots and recheck all hashes at completion.
A shared hardware lock prevents overlapping workloads. New work also stops on
charger loss, competing GPU use, STOP, low disk or resource limits.

## J71 CPU cost and correctness gate

Expected 4–7 minutes, hard 10 minutes, two physical CPU cores, 4 GiB RAM, no GPU.
Build a Wide192/4×32 control, a real Wide192/2×64 engine, and a cost-only 2×64
proxy. Pair adjacent trained 32-unit heads into the 64 lanes of each proxy head.
The proxy computes every lane, then zeros the irrelevant 32 lanes according to
the original four-band rule. This reproduces the first model's value and policy
while paying for 64-unit computation. **The real build never masks lanes.**
The proxy's extra masking and compiler differences limit cost precision; it is
not a mathematically guaranteed bound on actual trained-model speed.

Verify rule/Chess960 tests, scalar/AVX2 and lazy incremental state, independent
NumPy hidden/value/policy and routing, both distinct sentinel heads including
lane 63, every piece count 2–32 and mirrored boards, invalid/missing/wrong-layout
models, readiness, stop/quit and the adopted stalemate regression. Original,
rebuilt and proxy must have identical fixed-32,768-node search trajectories on
48 independent-game positions. The actual unmasked candidate has a different
function: test its oracle agreement, not identical search output.

Two 600-ms searches per arm per position, rotating/reversing execution order,
complete history, fresh state, single thread. Use the frozen J68 48-position set.
Bootstrap paired positions/source games, retain each regression and stratum.
Require at least 44 usable positions. The proxy must satisfy against **both**
original and rebuilt controls:

- lower paired NPS 95% bound ≥ −12%;
- median paired depth change ≥ −1 ply;
- at most 10% of positions lose more than 2 mean plies;
- every stratum's median NPS change ≥ −18%.

The rebuilt control's median NPS must be within 3% of the original. These are
predeclared admission limits, not claims that speed will meet them. A failure
retains results and stops before full training; no silent threshold relaxation.
The 12% allowance deliberately permits more cost than J68's 5% frontier while
still rejecting substantial depth loss for this user-selected reader design.

## J71 learning

213,899,602 training rows; 2,106,816 validation rows. Twelve complete passes =
2,566,795,224 presentations. Stream eight authenticated compressed shards per
prefetched group; never expand the corpus to disk. No new labels or training
corpus, tablebases, hand-written chess scores or targeted resampling.

AdamW, effective batch 4096, LR 0.01 to 0.001, 2M-presentation warmup, one high-LR
pass followed by cosine decay; weight decay 1e-6, gradient clip 1, quiet-policy
weight 0.25. No early-stop reduction in passes. Select the checkpoint with lowest
full-validation WDL cross-entropy, just as for the first Wide192.

Before the full run, independent integer/GPU agreement before/after discarded
updates, all-head and extra-lane gradients, exact boundary supervision,
deterministic initialization, actual-trainer checkpoint/resume and validation
aggregation checks. Discard calibration updates; real training starts fresh.
Rate calibration must project training plus a 10% reserve within the four-hour
training cap (minus shutdown margin). If it does not, stop and report.

Expected about 2–3 hours of training, provisional until calibration. Gate cap
10 minutes; full training/export/report phase cap **4 hours**. Four physical CPU
cores, 6 GiB host RAM, 3 GiB GPU process memory, 2.5 GiB allocator. GPU preflight
runs on the host outside the sandbox. Checkpoints every 120 seconds and at
validation; retain failures and explicit resumable state, with no automatic retry.

Reuse the first Wide192's authenticated validation statistics instead of
re-evaluating it. Report paired game-grouped error intervals, quiet policy,
endgame slices, head use and 16/17 boundary disagreement. Validation selects
checkpoints, so these intervals are exploratory, not an untouched strength test.

## J72 engine delivery and direct match

Delivery: maximum 15 minutes, two physical CPU cores, 4 GiB RAM, no GPU. Reuse
only the verified **unmasked** 2×64 executable. Export the selected model; repeat
native parity, rules/Chess960, incremental selftest, legal policy-on/off searches,
stop/quit and stalemate checks, bad-model rejection, NetPath override and
relocation to a path containing spaces. Paired 48-position NPS/depth and startpos
readings compare to the first Wide192. Trained-model lower paired NPS 95% bound
must be ≥ −15%, median depth change ≥ −2; model-dependent tree changes make this
separate from the function-controlled cost test. Failures stop for investigation.

Install only a new package, with source, licence, hashes and receipts:
`workspace/engines/forge-wide192-reader64/forge-reader64`.
That is the planned destination, not a claim that an engine already exists.

Then **400 games against the first Wide192/4×32**, 8 seconds per side per game
+ 0.08 seconds per move, concurrency 8; the same 200 frozen colour-paired human
openings as J70. No engine filtering or results-based selection. Known engineering
openings; training overlap unknown. One thread and 64 MiB Hash each, quiet policy
on, weight 1024/minimum depth 4, 10-ms overhead, unchanged J47/J48 search defaults.
Eight distinct physical CPU cores, paired colours on the same core, first colour
alternating. Fresh hash/history per game, full move history per position.

Expected 20–30 minutes, hard 30 minutes, eight cores, 4 GiB RAM, no GPU. No
resignation/evaluation adjudication/tablebases. Natural/current claimable draws;
600-ply games are censored and halt review, never automatically scored draws.
Retain flags; more than 8 flags stops for infrastructure review. Independently
replay every PGN, move log, clock/result and opening pair. Report W/D/L, colours,
opening-pair bootstrap intervals and bounded-score interval. No precise Elo or
long-time-control claim; no automatic daily-driver replacement.

Total active hard cap **5h15m**, including any explicitly resumed attempts;
phase caps also cumulative. Completion/failure notification once; small receipt
reader in `progress.py`. The original package and frozen first run stay intact.

## Import failure and reviewed recovery — 25 October 2026

The dependency watcher successfully dispatched J71 after the first Wide192's
training, engine delivery and 400 games completed. J71 then stopped after
37.467352 seconds, in its CPU cost phase, before any GPU work or training.
The helper named `queue.py` shadowed Python's standard library `queue`, causing
`engine_client.UCI` to fail at `queue.Queue()`. This was a runner packaging error.

Rename that helper to `dependency_watch.py`; leave the engine, model design,
training recipe, tests and admission thresholds unchanged. A lightweight fake
UCI process now checks both clients' real imports, startup, readiness, stop and
quit before hardware work, and again from the actual frozen worker snapshot.
It also rejects local filenames that shadow standard-library modules.

The original failed run `runs/20261025T061317Z` remains intact. Recovery creates
a new run with a revised source lock and an explicit link to the original
failure. Only the watcher rename, supervisor, import check and this protocol may
change. The old frozen sources and unchanged dependency are authenticated first.
Its 37.467352 seconds of active wall time, including 37.466522 seconds of the
cost phase, are carried into the new run's cumulative limits. No training is
repeated; it never started. A recovery cannot be dispatched twice automatically.

## Reviewed NPS trade-off — 25 October 2026

The repaired CPU run `20261025T094221Z` completed its tests, then stopped only at
the preset NPS limit. All 48 trajectories matched at fixed node count. The true
unmasked candidate also passed its independent integer/routing, rules, incremental
and UCI checks. Proxy NPS fell 15.32% versus the first Wide192 (95% interval
-15.60% to -14.73%); the rebuilt control was within 0.08% of the original.

At equal 600-ms budgets, 25 positions retained mean depth and 23 lost 0.5–1 ply;
overall mean loss was 0.458 ply, median zero. Best moves differed in 4/96 paired
searches. Thus unchanged median depth does not establish unchanged gameplay or
strength. It does satisfy the user's requirement to avoid a substantial depth
collapse while testing wider active readers. The user challenged the NPS veto,
and the existing authority to choose implementation details and finish this
specific experiment supports continuing to training and the requested match.

Reuse the authenticated CPU tests and unmasked binary; do not rebuild or repeat
timings. A new run records explicit admission and the original failed summary,
and carries both prior attempts (262.797396 seconds) against existing limits.
Only the admission/controller/delivery reporting and this protocol change; model,
training, search, data, seeds, twelve passes, opponent and resources stay fixed.
NPS remains measured and reported at delivery, including its original veto result.
It no longer blocks this experiment by itself. Keep the existing delivery median
depth floor of -2 plies and every correctness/resource gate. The 400-game match
is the playing-strength screen, with no automatic daily-driver promotion.
