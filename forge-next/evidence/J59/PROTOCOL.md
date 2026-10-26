# J59 — 100 games per new capacity model versus zero-skip Specialists

User specifically authorized: “100 matches tc 8+0.08, conc 8. both the new kids
vs specialists with zero skipp”. Interpreted as 100 games per candidate, 200
total, 8 concurrent games total. 8 seconds per side for the entire game, with
0.08 seconds added after each move. Expected 10–15 minutes, hard 25-minute cap,
eight physical CPU cores, 4 GiB RAM. CPU only, no builds, training, GPU or tuning.

Exact binaries: verified J58 Wide160/Deep32 and verified J54 zero-skip Specialist.
Selected trained weights are unchanged. All carry the adopted J47/J48 corrected
search. Binary/model hashes and input source inventory frozen before launch.

50 fixed-seed selected, unique 12-ply openings from 50 independent source games
in the existing J49 opening cohort. No engine evaluation filtering. Each new
candidate plays both colours against the same baseline from every opening.
One worker handles all four games for an opening on the same pinned physical
core. First candidate and first colour alternate by opening. Eight worker
threads, at most eight active engines/searches, with 24 persistent engine
processes (the inactive players remain idle).

One engine thread each; 64 MiB hash; QuietPolicy on, weight 1024, minimum depth4;
10ms move overhead, MultiPV1, no pondering, no tablebases. All search/pruning
settings retain the defaults. Clear TT/history with ucinewgame before every
game and replay the full move history each turn. Initialization and protocol
readiness are outside the game clock; measured response time is charged.

Play to natural terminal outcomes, current threefold repetition or current
fifty-move claims. No evaluation-based draw/resign adjudication. Time forfeits
are counted and listed. More than two expiries stop the run for inspection.
600 played plies censors the game and stops the run; it is never scored a draw.
Unexpected illegal moves, crashes or protocol failures stop the run and retain
partial logs. No score-based stopping or automatic retry/extension.

Persist PGNs, per-move legal moves/timings/clocks/GUI statistics, exact options
and per-game receipts. At completion independently replay PGNs and logs, verify
clocks, outcomes, names, colours, core allocation, all 200 games and 50 pairs per
candidate. Report separate W/D/L, points, score, colour splits, failures and
95% opening-pair bootstrap intervals (10,000 fixed-seed resamples). These are
per-comparison screening intervals, not precise Elo claims, long-time-control
results, simultaneous familywise intervals or evidence of generalisation to
unseen opening families. Known cohort; training overlap unknown. No promotion.

Durable systemd supervision, charger/STOP/shared-lock guards, cgroup CPU/memory/
wall limits, automatic reports and one desktop completion/failure notification.
Use the lightweight progress.py snapshot manually; no conversational polling.

Run: `bash research/forge_capacity_match_20261024/start_j59.sh --approve-j59`.
