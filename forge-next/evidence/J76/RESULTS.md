# J76: what local tables change for Wide192

**They correct the covered endings. They help some larger positions, but do not
resolve every fortress from far away.** Original Wide192 and its weights remain
unchanged. This is a separate research build, not a demonstrated strength upgrade.

## The decisive result: 80.h5 gives away the win

In the historical h-pawn game, the five-piece position before 80.h5 is an exact
tablebase win. The pawn push 80.h5 changes it to an exact draw; 80...Be4 reaches
the saved 81st-move position. Wide192 still reports +5.23 there without tables.
With tables it immediately reports a draw and selects a move that preserves it.
This conclusion comes from the actual table files, independently read by Fathom
and python-chess, including all legal moves and the recorded halfmove clocks.

Before h5, 12 legal moves preserve the win and five draw. Rd7+ (the unassisted
engine's choice in this test) still wins. This experiment does **not** show that
current Wide192 would replay the historical h5 mistake at this time budget.

## Search can reach an answer from outside the tables

The eight-piece corpus position `corpus-1494057-118` changes from +1.03 to 0.00.
Its displayed continuation reaches a five-piece tablebase draw after exchanges;
the endpoint was independently verified. The six-piece historical position
`rook-bishop-64w` has no matching six-piece table in this folder, but search
reaches a covered winning ending and chooses Ke6. Its displayed line ends at
a five-piece position that is an exact loss for the opponent.

Those leaf answers are exact. The conclusions at the larger roots still come
from selective alpha-beta search, not an exhaustive mathematical proof of every
defence. A high tablebase-hit count alone does not settle a larger position.

The thirteen-piece opposite-bishop positions remain outside direct coverage.
Search does use covered leaves in some branches, yet still reports a positive
advantage at the root. For example, `current-54b` changes from +1.09 to +0.84;
it is **not solved** by this experiment. Some other larger positions barely
change and one eleven-piece position's positive score increases.

## Complete comparison

Twenty saved roots with full histories. One second requested per engine/root,
alternating off/on execution order; 10 ms move overhead, one search thread,
32 MiB hash, clean search state each time. Tables-on covered roots return early.
Scores below are the last displayed UCI score, from the side to move; they are
not independently calibrated pawn advantages. The final bestmove is listed
separately because an interrupted iteration can change it after a displayed PV.

| Position | Pieces | Tables off | Tables on | Final move off → on |
|---|---:|---:|---|---|
| fortress-80w | 5 | +5.47 | **WIN** (direct table) | Rd7+ → Rd1 |
| fortress-81w | 5 | +5.23 | **DRAW** (direct table) | Rd7+ → Kg5 |
| rook-bishop-65w | 5 | +1.52 | **WIN** (direct table) | Kf6 → Rh7+ |
| catalan-80w | 14 | +2.56 | +2.56 | Be5 → Be5 |
| corpus-1332191-109 | 11 | +3.28 | +3.47 | f5 → f5 |
| corpus-364233-225 | 5 | +0.84 | **DRAW** (direct table) | Kd7 → Kb7 |
| corpus-1176670-264 | 5 | -0.42 | **DRAW** (direct table) | Rf8 → Ka2 |
| corpus-1630699-67 | 12 | +1.24 | +1.13 | Kd4 → Kd4 |
| corpus-1494057-118 | 8 | +1.03 | +0.00 | Ra5+ → Ra5+ |
| corpus-2036369-274 | 5 | +0.00 | **DRAW** (direct table) | Bxg1 → Bxg1 |
| corpus-761265-138 | 5 | mate 5 | **WIN** (direct table) | Rxa1 → Rxa1 |
| corpus-1421554-75 | 17 | -2.11 | -2.08 | Re7 → Re7 |
| current-24b | 23 | +0.55 | +0.55 | Qxb4 → Qxb4 |
| current-32b | 14 | +0.89 | +0.91 | Rxb3+ → Rxb3+ |
| current-44b | 13 | +0.95 | +1.09 | Ke6 → Ke6 |
| current-46b | 13 | +1.06 | +1.06 | Bf3 → Bf3 |
| current-54b | 13 | +1.09 | +0.84 | Kd5 → Kd5 |
| current-79b | 13 | +1.10 | +1.15 | Bf1 → Kc4 |
| current-92b | 13 | +1.22 | +0.97 | Kf3 → Kf1 |
| rook-bishop-64w | 6 | +5.68 | searched tablebase win marker | Rb6 → Ke6 |

## Scope and interpretation

- Folder: 146 WDL files and 146 DTZ files, about 1.39 GB. Material-class counts
  are 5 three-piece, 30 four-piece, 110 five-piece and one six-piece class:
  **KQPvKBP**. Both kings count as pieces. This is not general six-piece coverage.
- Seven of the twenty roots are directly covered: three wins, four draws.
  The other thirteen require ordinary search to reach any covered leaf.
- The neural evaluation itself is unchanged. The search consults exact answers
  in addition to its existing evaluator. It has not learned to recognise these
  fortresses in unprobed positions.
- Direct covered roots use DTZ (distance to a capture/pawn move, or mate), the
  actual fifty-move clock, and legal game-history checks. They prefer prompt
  progress toward a reset. DTZ is not distance to mate. A documented one-ply
  boundary ambiguity falls back to normal search rather than claiming certainty.
- Interior probes use WDL at halfmove clock **zero**, including quiescence.
  This deliberately avoids treating zero-clock WDL as exact near the fifty-move
  limit. Missing material tables mean no answer, not a draw.
- An ablation disabling direct root lookup still overvalues the five-piece
  fortress despite many interior hits: ordinary search only reaches certain
  covered zero-clock branches. Correct root lookup is therefore important.
- Covered roots return without conventional search; depth 1 and nodes/NPS 0
  are expected. Table-assisted nominal depth elsewhere is not comparable to
  unassisted depth. No speed or strength benchmark was performed.
- Known tablebase wins/losses use a ±31753 cp marker (adjusted by search ply),
  below the mate-score range. **This is not a 317-pawn neural evaluation.** A real
  detected mate retains a mate score. The UCI log identifies direct root answers.
- This research build selects one outcome-preserving move immediately in a
  directly covered ending; it does not offer multiple root PVs there or rank
  practical swindling chances against imperfect opponents. There was no match.

## Verification and retained failures

The successful run passed the existing rule suite and all 28 perft fixtures
(including Chess960), 4,632 incremental evaluator checks, raw-evaluation parity
on 17 contexts, and exact tables-off fixed-node equality on 12 searches.
All seven directly covered root outcomes and selected moves agree with the
independent reader, which also enumerated every legal move at those roots. 11,820 displayed PV plies from the 20 comparisons replay
legally. First interior table hits and the important final PV endpoints were
independently verified.

Additional checks cover the fifty-move boundary, en passant, promotion, missing
material tables, castling fallback, restricted searchmoves, mate precedence,
stop, ponderhit, disabling/reloading tables, invalid paths and relocation to a
directory containing spaces. The launcher automatically finds the same model.

Three earlier attempts remain intact: missing Wide192 build dimensions (9.16s),
a checker that did not handle partially missing child tables (61.07s), and a
table-loader lifetime error exposed by the invalid-path test (57.46s). The loader
now uses Fathom's own reinitialization API; final teardown is not followed by
reinitialization. The integration's capture-mate precedence was also corrected
and has explicit regressions. These were new integration/checker issues, not
evidence of corruption in the original daily driver. The final run took 57.48s;
cumulative hardware time was 185.17s, under the ten-minute allowance, with about
373 MB peak aggregate RSS. No GPU, training or games were run.

Evidence: `comparisons.json`, `interior-only.json`, `boundary-checks.json`,
`checks.json`, `audit.json`, `PV_ENDPOINTS.json`, `REVIEW.json`, raw `logs/`,
and the immutable run's source/model snapshots.
