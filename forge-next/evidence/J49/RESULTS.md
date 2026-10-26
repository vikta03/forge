# J49 — corrected Specialist search versus original

Corrected engine: **51 wins / 110 draws / 39 losses**, 106/200 points (**53.0%**).

Opening-pair bootstrap 95% score interval: **49.0%–57.0%** (10,000 resamples of 100 pairs). The paired interval includes 50%; this screen does not resolve a strength difference.
Conservative bounded-score 95% interval: 39.4%–66.6%; this avoids treating a degenerate bootstrap as certainty.

Both engines used the exact same Specialist weights, quiet policy enabled, one search thread, 64 MiB hash and 10 ms move overhead. Eight concurrent games ran on eight physical cores. Each side received 8 seconds for the game plus 0.08 seconds after each move. One hundred frozen human-game openings were played with reversed colours on the same core. First colour order alternated between pairs. There was no evaluation adjudication or early stop based on score.

The corrected binary combines J47 and J48. This experiment cannot attribute a difference to one individual correction. Weights and search defaults are unchanged. Full receipts identify exact binary and model hashes.

- Corrected as White: {'wins': 35, 'draws': 51, 'losses': 14}.
- Corrected as Black: {'wins': 16, 'draws': 59, 'losses': 25}.
- Terminations: {'current_threefold': 76, 'insufficient_material': 24, 'checkmate': 90, 'current_fifty_moves': 9, 'stalemate': 1}.
- Clock expiries: 0; any such games remain in the primary score and are listed in summary.json.
- Median game wall time: 23.22 seconds.
- Maximum played plies after the opening: 482.

All 200 games passed independent log/PGN replay, legal-move, clock, result and colour-pair checks. Results are a short-time-control screen on one laptop at concurrency eight, not a precise Elo measurement or a long-time-control claim. The interval assumes sampled opening pairs are reasonably independent; shared opening families and unknown training overlap limit generalisation. No engine was promoted or published.

See games.pgn, games/, match.json, pairs.json, audit.json and summary.json for the evidence. A successful supervisor completion.json additionally confirms resource limits and unchanged frozen input hashes.
