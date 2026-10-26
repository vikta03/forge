# J63 — Wide160 FullKey versus original Wide160

100 games; 8 seconds per side per game plus 0.08 seconds per move; eight games concurrent in total. Same 50 human-game openings, both colours.

| Candidate | Wins | Draws | Losses | Score | 95% opening-pair bootstrap |
|---|---:|---:|---:|---:|---:|
| fullkey | 20 | 61 | 19 | 50.5% | 45.0%–56.0% |

All engines use one thread, 64 MiB hash, quiet policy enabled with weight 1024/minimum depth 4, 10 ms move overhead and unchanged default search/pruning settings. All have the adopted J47/J48 search. The baseline is the original Wide160 daily-driver binary; FullKey is the verified J62 package. Weights are identical. Full-key companions are included within Hash64, leaving about 34% fewer entries in the candidate. Model hashes and exact binaries are frozen in source-lock.json.

Both colour games for an opening run on the same physical core. First colour alternates by opening. Eight workers are pinned to eight distinct physical cores; hybrid core speeds and laptop contention are part of these conditions. Hash/history are cleared between games; complete opening and game histories are sent on each move. No evaluation adjudication, resignation rule, tablebases or result-dependent early stopping.

- **fullkey:** The interval includes 50%; no strength difference resolved. As White: {'wins': 11, 'draws': 32, 'losses': 7}; as Black: {'wins': 9, 'draws': 29, 'losses': 12}. Clock expiries: 0. Terminations: {'current_threefold': 47, 'checkmate': 39, 'current_fifty_moves': 5, 'insufficient_material': 8, 'stalemate': 1}.

Every game passed independent PGN/move-log replay, legal-move, result, clock, name, colour and core-pair checks. Flags are retained in primary results. A 600-ply game is censored and stops the run; it is never silently scored a draw. Unexpected engine/runner failures stop the run with receipts retained.

This is a short-time-control screen, not a precise Elo rating or long-time-control verdict. Fifty opening pairs per candidate are the uncertainty units; the approximate bootstrap intervals are per-comparison, not simultaneous familywise claims. Known engineering openings and unknown training overlap limit generalisation. No engine was promoted. See summary.json, pairs.json, audit.json, games.pgn and per-move receipts. Supervisor completion.json also checks resource limits and frozen input hashes.
