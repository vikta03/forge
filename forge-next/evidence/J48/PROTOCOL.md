# J48 — exact terminal recognition before evaluation/pruning

**Prepared, not approved or run.** Expected **5–8 minutes**, maximum **10 minutes**,
two physical CPU cores, 4 GiB aggregate host RAM. CPU only; no GPU, training,
games, downloads, publishing or daily-driver replacement. Charger required.
The outer service has a 30-second cleanup allowance.

## Question and candidate

J47's 29 contracts and 24-position cost screen passed; that bundle is this job's
baseline. A remaining source-confirmed path allows a stalemate to receive a
neural stand-pat value, TT value or main-search forward-pruning return before
any legal move is established. This is distinct from J47's dead-material rules.
The prepared witnesses have not yet been executed in C++.

Only three files change from J47: `movegen.h`, `movegen.cpp`, `search.cpp`.
`has_legal_move()` first proves existence using an unpinned pawn push, or a king
move accepted by the existing legality checker. If these proofs fail, it falls
back to normal move generation and legality, including pins, en passant,
promotions and Chess960. Failing a fast proof does not mean stalemate.

At each non-check interior main-search or qsearch node, no legal move means a
draw before TT reuse, evaluation, stand-pat or pruning. Known rules draws also
take priority over the stack-limit evaluation fallback. In-check handling and
root move generation remain the existing paths. No quiet-TT, recapture, SEE,
pruning-constant, policy, network, training or evaluation-algebra change is
included. The helper assumes legal game positions, as the engine does.

This is deliberately broader than the conditional last-piece stalemate check
in the saved Stockfish 19 source. Broader coverage may cost more; it is not
automatically a strength improvement. All source, receipts and costs remain
available even if rejected. Do not weaken the check silently to pass a speed gate.

## Gates, in order

1. Verify frozen inputs and original-engine/model hashes. Reuse the exact,
   successfully tested J47 candidate binary as baseline, with its source
   verified byte-for-byte. Build the new production candidate with unchanged
   flags. Compile both direct-search harnesses plus the new sanitizer harness
   using trap mode and a safe/overflow sentinel; no shared UBSan runtime needed.
2. Run all 29 J47 contracts plus ten terminal contracts. The candidate must pass
   39/39, including under UBSan. All 29 existing cases must pass on baseline.
   Required seeded stalemate/TT/forward-pruning witnesses must fail on baseline.
   New cases cover positive and negative cached evaluations, cached bounds,
   main search, qsearch, stack fallback, mate and a legal king-escape control.
   Seeded values establish control flow, not natural-game frequency.
3. Compare `has_legal_move()` and the complete legal-move set against python-chess
   on **3,454 frozen legal states**, including all 960 initial layouts, special
   move cases and saved real-game prefixes. Check that the helper leaves the
   position unchanged. Run the same oracle through baseline, candidate and
   sanitized candidate. No engine-generated games are involved.
4. Candidate existing native rules/Chess960/cache/perft regressions and both
   model incremental self-tests. Require exact raw network-output agreement
   with J47 and the original daily driver on 61 FENs per model, 122 comparisons.
5. Legal searches with policy both on/off, legal reported PVs, MultiPV, stop/quit,
   and six ponder/infinite lifecycle cases. A real depth-one root search limited
   to `Qf7` must report a draw for its stalemated child: start FEN
   `7k/4Q3/6K1/8/8/8/8/8 w - - 0 1`, `searchmoves e7f7`.
6. **Cost screen only:** 48 positions from 48 independent games, twelve in each
   opening/forcing/quiet/endgame stratum, excluding all J47 games. Selection is
   the first eligible games in the pre-existing J46 file order, before candidate
   execution. Specialist model, policy on, Threads 1, Hash 32 MiB, one identical
   pinned CPU for both arms. External stop budgets **0.25 s and 1.0 s**, each
   twice per engine, ABBA/BAAB and alternating budget order. Equal warm-up is
   discarded; TT/history cleared before each search. 384 timed searches,
   approximately 240 seconds of timed search work.

Report wall and GUI NPS, completed/selected depth, choice/PV changes, every
position's regression and a game-bootstrap 95% interval at each budget. The
bootstrap resamples games (one position per game); do not pool the two budgets
as independent positions. Symmetrically exclude any whole pair hitting the
depth ceiling. **A median NPS slowdown over 5% at either budget requires review.**
Retain failed results and stop; no automatic tuning, restart or promotion.
Passing these gates is not an Elo claim or proof of state-of-the-art search.

## Running and notification

Preparation only (no compiler/search):

```sh
PYTHONDONTWRITEBYTECODE=1 python3 research/forge_terminal_search_20261023/run_j48.py
```

After specific user approval:

```sh
bash research/forge_terminal_search_20261023/start_j48.sh --approve-j48
```

The systemd user service survives chat/account changes. The supervisor enforces
charger connection, shared hardware-job lock, CPU affinity, RAM and wall limits,
workspace `STOP_HARDWARE` and run-local `STOP`. It writes durable receipts and
sends one completion/failure notification. No conversational polling.

Match evidence for the broader J47 correction bundle and later search heuristics
remains separate work requiring its own protocol and hardware approval.
