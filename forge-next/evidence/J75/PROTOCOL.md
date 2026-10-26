# J75 — does FORGE leave useful policy guidance unused?

Prepared 25 October 2026 after the user requested the policy investigation.
Source preparation is permitted; the hardware phase awaits specific approval.

Expected 3–6 minutes, maximum 12 minutes, two physical CPU cores and 4 GiB RAM.
CPU only: one isolated diagnostic build, short searches and saved-data review.
No GPU, training, games, downloads, tablebases, new corpus or changes to installed
engines. This job does not introduce a search treatment or promote a setting.

## What this distinguishes

1. Does the existing policy favour a reference defensive move?
2. Does natural search reach that board, examine that move, reduce it, prune it,
   or end the node before reaching it?
3. When a move is examined in a separate, restricted root search, does the
   evaluator/search still disagree with the saved reference?

A reference line is a finite analysis, not a proof of a unique defence or draw.
Winning controls remain labelled as controls. A zero evaluation is not itself
success, and a move difference is not evidence of better chess.

## Frozen subjects and positions

Primary model: exact mature daily-driver Wide160. Secondary models: both J74
scratch pilots, to explain their previous readings. All three use the same
adopted J47/J48 search and their unchanged model files. Keep ages separate.

Twelve roots from the already inspected J74 panel: the four historical cases,
opposite bishops, pawnless rook/bishop versus rook from both sides, rook-only,
rook/bishop versus rook with pawns, one simple liquidation draw, and two
decisive corpus controls. Exact IDs, full histories and source hashes are in
positions.json/source-lock.json; no outcome-based sampling after this run.

For each root retain the initial board and the defensive-side boards after
one and three plies of its longest saved J66 exact-score reference PV, where
nonterminal. At most 36 contexts. These are reference candidates, not new labels.
Matching a trace uses board key plus halfmove clock; full search paths are
recorded to distinguish actual transpositions, different repetition histories,
excluded-move tests and hypothetical null-move paths. Counts on different
paths must not be silently merged into proof about the actual game history.

## Observation-only native source

Build a copy of the unchanged Wide160 source with diagnostic events only.
No extra neural inference, history updates, legality decisions or pruning
changes are inserted. Observe node entry/return, main static score, quiet-policy
gate and score blend, selected move/stage, actual initial child depth, retries,
pruning reasons and result windows. Quiescence entry, stand-pat and return are
observed; this is not an exhaustive recording of every quiescence move.

The tracer records only the three target boards in a natural search. Cap each
search at 2,048 matching node visits / 20,000 events. Explicitly mark truncation;
missing events in a truncated trace never establish that a defence was absent.
No timing/NPS claims may be made from the diagnostic binary.

## Execution

1. Check input/model/source hashes; one clean fixed-AVX2 build. Existing rule
   and incremental neural self-tests, startup/readiness and stop/quit.
2. Verify raw integer outputs unchanged on the existing 13 rule/encoding
   fixtures. Original versus diagnostic searches, including trace enabled,
   must agree in every deterministic UCI iteration (nodes, depths, score,
   bound, PV and final move). Six independent fixtures at 16,384 nodes, then
   each of the 12 roots with each model at 262,144 nodes. Alternate execution
   order. Reject any disagreement; retain the failed receipts.
3. Read raw value and all legal quiet-policy logits at every retained context
   for each model. Rank only among the eligible moves; mark captures and
   check-evasion boards where this policy integration is not used.
4. At every context run one unrestricted original-engine search at 131,072
   nodes. Then restrict separate searches to the saved reference move and the
   raw top-policy move (deduplicated, at most two). These experiments diagnose
   what the moves lead to; they are not ordinary gameplay or a causal policy
   intervention. Read each selected child's raw value from its perspective.
5. Replay every PV independently, reject illegal moves/unexpected tablebase
   hits, verify forced-root constraints and raw-logit scalar conversion.
   Recheck installed package hashes. Preserve raw UCI logs, all events,
   score bounds, capped traces and the source used.

Fresh search state, complete game histories, one thread, Hash64 MiB,
QuietPolicy=true, weight1024, minimum depth4, no pondering. Diagnostic workload
has a fixed node allowance; wall deadlines are safety stops, not timing data.
No reference engine is run: reuse its saved legal candidate lines and scores.

## Reporting and decision

Report each case separately: policy rank, actual examination/pruning, reduced
and full-depth attempts, outcomes, and disagreement after forced inspection.
Do not treat repeated visits as independent statistical samples. Explain cases
where the reference path was never visited or where history differs.

If policy repeatedly favours a useful defence that is reduced or discarded,
that motivates a separate generic policy-guided-reduction comparison. If the
defence is already examined and still misvalued, increasing policy authority
has not been justified by this observation. Neither outcome proves an engine
bug. No automatic additional jobs, retraining, matches or promotion.

## Operation and limits

Launch only after approval:

```
bash workspace/research/forge_policy_defence_20261025/start_j75.sh --approve-j75
```

One durable systemd service, shared hardware lock, charger and competing-workload
preflight, STOP/resource guards and one desktop completion/failure notice.
No chat polling, heartbeat or automatic restart. A run failure retains its source,
logs and measurements; any recovery must count previous time in the same cap.
Read status without running work using progress.py in this directory.
