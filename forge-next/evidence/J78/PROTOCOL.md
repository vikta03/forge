# J78 — one isolated advance-repetition search mechanism

**Approved 26 October 2026.** User: “usage had run out, resume. also i approve job”.
This is the specific J78 build/measurement allowance requested in this thread.

Expected **5–8 minutes**, maximum **10 minutes cumulative** across attempts,
two physical CPU cores, **4 GiB RAM**. Charger required. CPU only; no GPU,
training, engine games, downloads or automatic replacement of the daily driver.

## Question

Can a legal repetition be recognised before a zero-width search prunes or
omits the drawing move, without changing existing rules or imposing substantial
runtime cost? This is distinct from recognising a fortress without repetitions.
J77 proved that Stockfish already recognises the clearest fortress statically;
this job is not presented as a remedy for that evaluator gap.

## Frozen candidate

Exact current Wide192 source copied into `candidate/src`, retaining J47/J48.
The exact existing Wide192 checkpoint is used read-only. The original binary,
model and source are hashed and never modified. The only runtime change is
`Position::repetition_move` plus the experimental `RepetitionGuard` option,
default off. The delta table takes 80 KiB. Existing evaluation, model features,
search settings and correction-history defaults remain unchanged.

The method uses normalized repetition keys, a reversible-move delta lookup,
and actual legality verification. It does not cross a pawn move, capture or
null-move boundary. Across the root a third occurrence is required; strictly
inside the tree the existing cycle convention is retained. The search uses
this only at non-PV nodes with beta <= 0, excluding excluded-move searches.
The legal draw is a sufficient lower bound there. No direct TT write, no new
draw contempt, no automatic draw score for PV/root nodes.

Algorithm attribution and pinned Stockfish reference are in
`candidate/REPETITION_PROVENANCE.md`.

## Required gates

1. Clean AVX2/Haswell production build and separate UBSan-trap contract build.
2. All retained J47/J48 contracts, plus seeded main-search/quiescence witnesses,
   no-TT-write, stop priority, excluded move, positive beta and root-boundary
   checks. J48 terminal witnesses also run with the new option enabled.
3. Existing rules, perft/Chess960 and accumulator self-tests. Reuse the 3,454
   independent J48 legality fixtures, without changing their expected results.
4. At every prefix of frozen actual histories and constructed boundary cases,
   compare the new method with all legal children evaluated by the original
   repetition-state update. Independently compare those moves to python-chess
   board identities and occurrence counts at search plies 0, 1, 3, 4, 8, 64.
   Include available/not-yet-claimable cycles, null boundaries, castling-rights
   loss, pinned/legal en passant, promotion and check-evasion cycles.
   Require zero false positives and exact agreement on the specified domain.
5. With the option off, require exact raw evaluation and fixed-node trajectory
   parity with the original binary on 18 frozen positions. UCI readiness during
   search, explicit stop, legal PVs and clean quit must pass.
6. A 24-position CPU cost screen, one position per independent game, six each
   opening/forcing/quiet/ending. Cold on/off 400 ms searches in ABBA/BAAB order.
   Report game-bootstrap interval, depth changes and worst regressions. A median
   NPS loss above 5% requires investigation; it is not silently accepted or
   converted into a claim of better search efficiency. Changed trees make NPS
   a cost observation, not a controlled instruction-level benchmark.
7. Twenty J77 historical roots, complete saved histories, cold one-second
   on/off searches with original weights and no tablebases. Record repetition
   cutoff counters, complete trajectories, PV legality and root scores.

No strength claim follows from these checks. Missing confidence is reported,
not repaired by deleting difficult fixtures or relaxing gates. Failed attempts
and completed measurements remain in `runs/`. Any repair must fit the original
cumulative allowance. Completion/failure produces one desktop notification;
there is no conversational heartbeat or repeated status polling.

## Execution after approval

```bash
bash workspace/research/forge_repetition_guard_20261026/start_j78.sh --approve-j78
```

The supervisor acquires the existing hardware lock, verifies charger and source
hashes, rejects known concurrent heavy jobs, selects two physical cores, freezes
the source and scripts, and enforces process-group resource limits. Results are
in the directory named by `latest-run.txt`. A successful job still leaves the
candidate isolated for review; the daily-driver launcher is not replaced.
