# J74 — matched scratch comparison of two value-training objectives

25 October 2026. Authorized by the user's request:
“okay pilot comparison of the 2 training objectives go.”

## Question

At the same modest training budget, does teaching the expected score consumed
by alpha-beta make Wide160 a better scalar evaluator than teaching full WDL?
This is a learning-objective pilot, not another architecture, another corpus,
or a claim that a short pilot must beat the fully trained daily driver.

## Fixed mechanism and exact losses

Both arms use the original Wide160 architecture and quantized forward algebra:
160-channel placement tables, two perspectives plus eight state inputs, four
piece-count-selected 32-unit readers, the same two WDL logits and quiet policy.
The model implementation is copied byte-for-byte from the adopted Wide160
training source. No pretrained weights initialize either arm.

For teacher labels `(W*, D*, L*)`, define `t = W* + D*/2`.
For the student's predicted WDL, define `p = W + D/2`.

```
L_WDL   = -(W* log W + D* log D + L* log L)
L_score = -(t log p + (1-t) log (1-p))

Control value loss   = L_WDL
Candidate value loss = 0.8 L_score + 0.2 L_WDL

Total loss = value loss + 0.25 quiet-policy cross entropy
```

Both arms retain the existing normalized 0.25 adjacent-head overlap at piece
count boundaries. The auxiliary WDL term preserves some pressure to distinguish
a likely draw from equally likely decisive outcomes. All targets are existing
teacher labels; there are no hand-written endgame scores.

Compute `log p` as `logaddexp(log W, log D - log 2)` and `log (1-p)` similarly
from L/D. This avoids infinite scalar labels and probability underflow. Runtime
conversion remains the original `187 * log(p/(1-p))`, including its existing
integer rounding and clamps. There is no scalar-only output head or raw-CP
regression in this experiment.

The 80/20 mixture is fixed in advance, not tuned on these results. Its gradient
scale can differ from full WDL CE even though coefficients sum to one. Keep
the identical optimizer/policy weight and report clipping counts; do not claim
this is the best possible weighting or that a negative result rejects every
scalar-focused objective.

## Matching, budget and data

- One matched random seed, 2026102574; starting floating weights must match
  exactly, with independent optimizer storage.
- 67,108,864 presentations **per arm**, batch 4,096: 16,384 updates. Both models
  receive the exact same GPU batch tensors, in identical order. Alternate which
  model updates first to avoid a persistent execution-order advantage.
- Ordinary data comes from a seeded traversal of the existing 213,899,602-row
  training cache, shuffled in four-shard groups. Each partial group tail below
  one batch is skipped identically. This is roughly 31.4% of one corpus pass,
  not a full-data run, repeated small training pool, or targeted resampling.
- Same AdamW, weight decay 1e-6, clip norm 1; warm up for 512 updates to 0.01,
  cosine decay to 0.001 at the fixed final dose. This is a shortened scratch
  schedule, not continuation under a changed optimizer.
- Monitor at 16,777,216; 33,554,432; 67,108,864 presentations. Final comparison
  always uses the last equal dose; no result-based checkpoint selection.
- Reuse J67's existing ordinary and quiet-ending selection/confirmation panels.
  Check source-game separation from every training group. Panel files and cache
  receipts are pinned; compressed array hashes are verified on first read.
  These panels have been used in prior research, so they are **not pristine
  never-inspected tests**. No new mining, labeling or teacher searches occur.
- Expand only bounded shards in RAM. No raw archive expansion, new datasets,
  downloads, cloud compute, tablebases, builds or games.

Expected **12–18 minutes**; hard maximum **25 minutes**, four physical CPU cores,
six GiB aggregate host RAM, three GiB GPU process memory. A 256-update calibration
must project completion inside the remaining cap, with 20% timing margin and
210 seconds for checks/reporting. Otherwise stop and retain partial work.
Output cap two GiB; eight GiB initially free, one GiB reserve. Charger and an
uncontended GPU required. GPU checks run outside the sandbox. Use the shared
hardware lock and a durable user service. No automatic restart or cap extension.

## Correctness and outputs

Before training: check the loss independently in probability space, including
exact teacher wins/draws/losses and extreme logits; show that the auxiliary term
distinguishes equal-score distributions. Control loss must equal the historical
formula exactly. Independently executed CUDA optimizer updates/checkpoint restart
allow at most 1e-5 parameter roundoff, explicitly recorded. Check actual integer
forward values against an independent NumPy implementation, before and after
learning. All preliminary update tests are discarded before scratch training.

Save initial weights, periodic joint optimizer checkpoints, all three matched
doses, batch-order fingerprint, head exposure, clipping counts, row predictions,
teacher labels/references, summaries and resource receipts. Failure preserves
everything written. A failed attempt cannot silently start again.

Export both pilots into isolated UCI packages using the unchanged Wide160 binary.
Verify native integer value/query/policy against 13 rule/encoding fixtures,
incremental selftest, readiness, stop/quit and relocation with spaces. Then run
24 existing full-history roots at one second per model, one physical core,
alternating order, clearing hash, policy on, 64 MiB hash, no pondering. Check all
PV moves and best moves. Report scores, moves, NPS and depths as diagnostics;
these immature-model short searches do not establish strength or a draw cure.

## Assessment, fixed before results

Primary: **equal-source-game mean expected-score MSE** on ordinary confirmation.
Also report expected-score MAE/CE, WDL KL/Brier, policy CE, errors of at least
15 percentage points, piece-count heads, confident teacher draws and decisive
positions. Use paired game-cluster bootstrap intervals (2,000 resamples),
with the same games paired between arms. Intervals cover panel sampling,
not seed variability or whether teacher labels are chess truth.

Call the candidate promising for a larger experiment only if ordinary expected
MSE improves by at least 2% and its paired 95% interval excludes zero, ordinary
MAE does not rise, focused/all-draw/decisive MAE each worsens by at most 0.5
percentage points, and ordinary policy CE worsens by at most 0.02. WDL KL is
reported even if worse; optimizing scalar agreement can trade off WDL detail.
Report every guard, confidence interval and regression, including negative results.

Nothing automatically trains further, plays a match or replaces the daily driver.
This experiment can compare two recipes' learning at this dose; it cannot prove
better long-run convergence, better sibling ordering, or an Elo improvement.

## Operator commands

```
bash workspace/research/forge_objectives_20261025/start_j74.sh --approve-j74
python3 workspace/research/forge_objectives_20261025/progress.py
```

Create `STOP` in this directory or the run directory, or use the workspace
`STOP_HARDWARE`, to stop. One native completion/failure notification; no heartbeat
or conversational polling. The user will report when the hardware is quiet.
