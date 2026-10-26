# J72 — Wide192 two 64-unit heads versus Wide192 four 32-unit heads

400 games; 8 seconds per side per game plus 0.08 seconds per move; eight games concurrent in total. Same 200 human-game openings, both colours.

| Candidate | Wins | Draws | Losses | Score | 95% opening-pair bootstrap |
|---|---:|---:|---:|---:|---:|
| candidate | 92 | 158 | 150 | 42.8% | 39.2%–46.2% |

All engines use one thread, 64 MiB hash, quiet policy enabled with weight 1024/minimum depth 4, 10 ms move overhead and unchanged default search/pruning settings. All have the adopted J47/J48 search. The baseline is the first verified J69 Wide192/4×32 engine. The candidate is the J71 Wide192/2×64 engine. Both use the same scratch seed, corpus and twelve-pass schedule; the reader width, count and routing differ. Both retain the same corrected search. Binaries and models are frozen in configuration.json and delivery receipts.

Both colour games for an opening run on the same physical core. First colour alternates by opening. Eight workers are pinned to eight distinct physical cores; hybrid core speeds and laptop contention are part of these conditions. Hash/history are cleared between games; complete opening and game histories are sent on each move. No evaluation adjudication, resignation rule, tablebases or result-dependent early stopping.

- **candidate:** This sample favours the baseline. As White: {'wins': 51, 'draws': 84, 'losses': 65}; as Black: {'wins': 41, 'draws': 74, 'losses': 85}. Clock expiries: 0. Terminations: {'checkmate': 242, 'current_threefold': 110, 'insufficient_material': 30, 'stalemate': 3, 'current_fifty_moves': 15}.

Every game passed independent PGN/move-log replay, legal-move, result, clock, name, colour and core-pair checks. Flags are retained in primary results. A 600-ply game is censored and stops the run; it is never silently scored a draw. Unexpected engine/runner failures stop the run with receipts retained.

This is a short-time-control screen, not a precise Elo rating or long-time-control verdict. Two hundred opening pairs per candidate are the uncertainty units; the approximate bootstrap intervals are per-comparison, not simultaneous familywise claims. Known engineering openings and unknown training overlap limit generalisation. No engine was promoted. See summary.json, pairs.json, audit.json, games.pgn and per-move receipts. Supervisor completion.json also checks resource limits and frozen input hashes.
