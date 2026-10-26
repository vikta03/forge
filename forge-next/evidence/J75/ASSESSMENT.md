# J75 assessment — 26 October 2026

J75 completed successfully. Total hardware time was 76.372 seconds including
the 17.877-second failed startup. The recovery reused the passed build and rule
checks. No engine, model or search setting was promoted or changed.

## Answer to the policy question

The current integration gives policy limited authority, and we observed real
examples of that limitation. However, this diagnostic does not establish that
stronger policy authority fixes the conversion problems. In the clearest
historical fortress case, examining the reference defence separately still
leaves a very large disagreement with the saved reference evaluation. Policy
also supplies a tactically losing top-ranked quiet move in that position.

### The historical fortress, after 81.Rd1

Context `wide160-fortress-81w@1`, Black to move:

`8/5k2/7K/7P/4b3/8/8/3R4 b - - 2 81`

The saved reference continuation is ...Bc2. Wide160 ranks it fourth among
19 legal non-captures. In natural search from the previous White root, its exact
history prefix appears in 61 main-search visits. The move is picked once, at
remaining depth 2, then discarded by quiet futility. Most other visits return
through existing node-level shortcuts, including 37 null-move cutoffs. These
are narrow-window bound decisions, not 61 independent full evaluations or proof
that those shortcuts are erroneous.

When this Black position is searched as its own unrestricted root, FORGE
actually selects ...Bc2, but evaluates it at -7.43. Restricting a fresh search
to ...Bc2 returns -6.75 after a completed depth-21 iteration. Thus the shallow
discard is real, but making the move available does not by itself recover the
draw-like reference assessment. This does not isolate neural evaluation from
all deeper selective-search effects.

The policy's top quiet move is ...Bg6 (only 9.23% of the diagnostic softmax,
not a confident near-certain recommendation). White can answer hxg6+, taking
the bishop. The separate forced search reports mate against Black and its saved
PV legally ends in checkmate. Ordinary search chooses ...Bc2 instead. Increasing
policy authority indiscriminately is therefore not supported by this case.

The parent `fortress-81w@0` has saved finite reference score +0.10. Wide160
returns +6.72 in the unrestricted 131,072-node context search and also in the
262,144-node natural search. The preceding `fortress-80w` is a winning control
(saved reference +6.48), not another position we should drive toward zero.

### A concrete example of policy being outweighed

In `wide160-corpus-364233-225@3`, a pawnless rook-and-bishop versus rook ending,
the reference move Re4 is also the raw policy's first choice. It is examined
28 times in the natural trace; it is not simply invisible. One late visit gives
it a +266 ordering bonus against a -24,877 pre-policy ordering score. These
are ordering units, not centipawns. Its scheduled 13-ply child search is reduced
to 11 plies and returns below the current alpha, so that attempt is not retried
at full depth. A subsequent visit reduces it to 12 plies, also failing low.

But unrestricted fresh-root search scores this context -0.75; forcing Re4
scores -0.69. The observation supports limited policy influence, not a large
missed correction demonstrated by forcing that move.

## Coverage and checks

- 12 selected roots, 35 related contexts per model; mature Wide160 is primary.
  The two short-trained J74 pilots are explanatory comparisons, not equally
  trained strength competitors. Contexts are not independent game samples.
- All 36 original/instrumented natural search pairs have identical deterministic
  iterations, final actual-node counts and chosen moves. Six startup fixtures
  also passed. None of the traces reached its recording cap.
- Wide160 visits 31/35 contexts through the exact recorded history prefix and
  schedules the reference move in 28/35. Direct move pruning is seen in two
  contexts. Missing moves can also result from unvisited branches or valid
  bounds; these counts alone do not diagnose a bug.
- Search correctly finds a zero score in the liquidation control despite its
  raw -5.97 evaluation. This is not a universal inability to back up draws.
- Saved-data review replays 67,595 PV plies across 369 saved searches, checks
  192 forced-root constraints, and rechecks the original binary and all three
  model hashes. See REVIEW.json. No new engine work was used for that review.

## Decision

Do not increase policy weight or change pruning by default on this evidence.
No policy-protection treatment, match or strength claim was part of J75.
A narrowly controlled policy-guided-reduction experiment remains a hypothesis;
it would need to improve defence recognition without rescuing bad policy moves
or losing too much search work. The larger unresolved issue here is recognition
of a defensible outcome after a move is already being searched.

Raw receipts: `natural/`, `contexts/`, `checks.json`, `audit.json`, `summary.json`.
All reported cp scores use the side to move at the stated context. Forced-root
and natural searches allocate work differently; their scores are not a causal
measurement of an altered policy integration. Reference analyses are finite,
not mathematical proofs or tablebase results.
