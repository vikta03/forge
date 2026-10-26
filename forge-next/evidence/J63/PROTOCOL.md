# J63 — Wide160 FullKey versus original Wide160

Authorized directly by the user: “no bad. gimme 100 game match results. the usual”.
The established usual is 100 games, 8 seconds per side per entire game plus
0.08 seconds per move, concurrency eight. Expected 6–10 minutes; hard maximum
20 minutes, eight physical CPU cores, 4 GiB aggregate RSS. CPU only. No builds,
GPU, training, downloads, tuning, promotion or automatic retry.

Use the exact original Wide160 daily driver and verified J62 FullKey package.
Their model bytes match. At equal Hash64, the FullKey candidate has about 34%
fewer entries; this practical storage trade-off is part of the comparison.
Original engines and frozen earlier research stay intact.

Reuse J59's frozen 50 unique 12-ply openings from 50 independent games in the
J49 cohort. No filtering by engine evaluation or previous match result. Each
opening is played twice with colours reversed on the same physical core; first
colour alternates by opening. Eight pinned workers, two persistent engines
per worker, at most eight simultaneous searches and sixteen resident engines.

Same existing runner and settings: Threads1, Hash64, QuietPolicy on, weight1024,
minimum depth4, MultiPV1, Move Overhead10ms, pondering off, no tablebases.
Search/pruning/evaluator defaults unchanged. ucinewgame clears both engines
before every game; send the full opening and played move history each turn.
Startup/readiness is outside the clocks; actual bestmove response time is charged.

No evaluation draw/resign adjudication. Play to natural outcomes or current
threefold/fifty-move claims. Count and list time expiries; more than two stop
the run for inspection. A 600-played-ply ceiling censors and stops rather than
being scored a draw. Illegal moves, crashes or protocol failures stop and retain
partial evidence. No score-based stopping.

Save PGNs, full UCI transcripts, exact options, legal move/clock/GUI statistics
and per-game receipts. Automatically audit all games independently from PGNs
and move logs, including clocks, legal outcomes, names, both colours and core
assignment. Report FullKey W/D/L, points, score, colour splits and failures;
95% opening-pair bootstrap uses 50 pairs and 10,000 fixed-seed resamples.
Known engineering openings and unknown training overlap limit generalisation.
This is a short-time screen, not a precise Elo or long-time verdict.

Freeze code/config/binary/model/evidence hashes before launch. Use the existing
hardware lock, charger and STOP guards, systemd CPU/memory/wall caps and a
single completion/failure desktop notification. Automatic reporting requires
no conversation polling. A manual progress.py snapshot is optional.

Run: `bash research/forge_fullkey_match_20261024/start_j63.sh --approve-j63`.
