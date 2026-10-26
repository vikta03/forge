# J59 — trained capacity candidates versus zero-skip Specialists

100 games per candidate; 8 seconds per side per game plus 0.08 seconds per move; eight games concurrent in total. Same 50 human-game openings, both colours.

| Candidate | Wins | Draws | Losses | Score | 95% opening-pair bootstrap |
|---|---:|---:|---:|---:|---:|
| wide160 | 35 | 31 | 34 | 50.5% | 42.5%–58.5% |
| deep32 | 24 | 35 | 41 | 41.5% | 35.5%–47.0% |

All engines use one thread, 64 MiB hash, quiet policy enabled with weight 1024/minimum depth 4, 10 ms move overhead and unchanged default search/pruning settings. All have the adopted J47/J48 search. The baseline is the verified J54 zero-skip Specialist binary. Model hashes and exact binaries are frozen in source-lock.json.

Both colour games and both candidates for an opening run on the same physical core. Candidate order and first colour alternate by opening. Eight workers are pinned to eight distinct physical cores; hybrid core speeds and laptop contention are part of these conditions. Hash/history are cleared between games; complete opening and game histories are sent on each move. No evaluation adjudication, resignation rule, tablebases or result-dependent early stopping.

- **wide160:** The interval includes 50%; no strength difference resolved. As White: {'wins': 21, 'draws': 18, 'losses': 11}; as Black: {'wins': 14, 'draws': 13, 'losses': 23}. Clock expiries: 0. Terminations: {'checkmate': 69, 'current_threefold': 23, 'insufficient_material': 4, 'current_fifty_moves': 4}.
- **deep32:** This sample favours the baseline. As White: {'wins': 18, 'draws': 20, 'losses': 12}; as Black: {'wins': 6, 'draws': 15, 'losses': 29}. Clock expiries: 0. Terminations: {'current_threefold': 24, 'checkmate': 65, 'insufficient_material': 5, 'current_fifty_moves': 6}.

Every game passed independent PGN/move-log replay, legal-move, result, clock, name, colour and core-pair checks. Flags are retained in primary results. A 600-ply game is censored and stops the run; it is never silently scored a draw. Unexpected engine/runner failures stop the run with receipts retained.

This is a short-time-control screen, not a precise Elo rating or long-time-control verdict. Fifty opening pairs per candidate are the uncertainty units; the approximate bootstrap intervals are per-comparison, not simultaneous familywise claims. Known engineering openings and unknown training overlap limit generalisation. No engine was promoted. See summary.json, pairs.json, audit.json, games.pgn and per-move receipts. Supervisor completion.json also checks resource limits and frozen input hashes.
