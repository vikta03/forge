# J70 — New capacity model versus original Wide160

400 games; 8 seconds per side per game plus 0.08 seconds per move; eight games concurrent in total. Same 200 human-game openings, both colours.

| Candidate | Wins | Draws | Losses | Score | 95% opening-pair bootstrap |
|---|---:|---:|---:|---:|---:|
| candidate | 113 | 171 | 116 | 49.6% | 45.9%–53.4% |

All engines use one thread, 64 MiB hash, quiet policy enabled with weight 1024/minimum depth 4, 10 ms move overhead and unchanged default search/pruning settings. All have the adopted J47/J48 search. The baseline is the original Wide160 daily-driver binary. The candidate is the verified J69 scratch-trained capacity architecture; both retain the same corrected search. Binaries and models are frozen in configuration.json and delivery receipts.

Both colour games for an opening run on the same physical core. First colour alternates by opening. Eight workers are pinned to eight distinct physical cores; hybrid core speeds and laptop contention are part of these conditions. Hash/history are cleared between games; complete opening and game histories are sent on each move. No evaluation adjudication, resignation rule, tablebases or result-dependent early stopping.

- **candidate:** The interval includes 50%; no strength difference resolved. As White: {'wins': 67, 'draws': 85, 'losses': 48}; as Black: {'wins': 46, 'draws': 86, 'losses': 68}. Clock expiries: 0. Terminations: {'current_threefold': 130, 'checkmate': 229, 'insufficient_material': 23, 'current_fifty_moves': 17, 'stalemate': 1}.

Every game passed independent PGN/move-log replay, legal-move, result, clock, name, colour and core-pair checks. Flags are retained in primary results. A 600-ply game is censored and stops the run; it is never silently scored a draw. Unexpected engine/runner failures stop the run with receipts retained.

This is a short-time-control screen, not a precise Elo rating or long-time-control verdict. Two hundred opening pairs per candidate are the uncertainty units; the approximate bootstrap intervals are per-comparison, not simultaneous familywise claims. Known engineering openings and unknown training overlap limit generalisation. No engine was promoted. See summary.json, pairs.json, audit.json, games.pgn and per-move receipts. Supervisor completion.json also checks resource limits and frozen input hashes.
