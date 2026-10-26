# J49 — 200-game corrected FORGE search screen

Authorized by the user's request on 2026-10-23: **“200 games, tc 8+0.08”**.
Concurrency eight carries forward the user's established match preference and was stated before preparation.

Compare the combined J47/J48 Specialist search corrections with the unmodified
Specialist daily driver. Both use the same full-trained Specialist network,
quiet policy enabled, identical search settings, one thread and 64 MiB hash.
The existing tested binaries are used: no rebuild and no promotion. config.json
contains exact paths; run_j49.py verifies the known three binary/model hashes.

Each side receives **eight seconds for the entire game, plus 0.08 seconds after
each move**. Wall clocks measure go-to-bestmove including transport; startup,
opening setup and isready exchanges are outside the clocks. Move Overhead is
10 ms for both engines. Late moves lose on time, except insufficient opposing
mating material; there is no extra grace in the scored clocks. A protocol hang
over a further second stops the job for investigation rather than fabricating
a result. More than two clock expiries also stops for review, retaining games.

## Frozen openings and execution

prepare_openings.py selects 100 distinct 12-ply human-game prefixes from 100
source games, excluding all games used by J47/J48. Selection is deterministic
and fixed before searches, with no engine evaluation filter. All openings have
at least two legal moves and are not terminal or claimable draws. Training
overlap is unknown. Source hashes, selection seed and chosen prefixes are saved.

Each opening gets both colours on the same physical CPU core, with first-colour
order alternating by pair. Eight workers use eight physical cores, each with
two resident engines and only one searching at a time. No pondering or GPU use.
No evaluation adjudication, resign threshold, tablebase adjudication or early
stop based on the score. Natural terminal results and current threefold/fifty
move claims end games. A 600-played-ply guard stops the job without assigning a
draw. Every move, clock, UCI transcript, PGN and game result is retained.

## Bounds and completion

Expected **10–15 minutes**, hard cap **25 minutes**, eight physical CPU cores,
4 GiB aggregate RAM, charger required. No training, games beyond these 200,
downloads, new builds, or modifications to daily drivers. The shared hardware
lock excludes other managed jobs. Global STOP_HARDWARE, unplugging the charger,
resource limits, an explicit run/STOP file or a technical failure terminates the
process group. Failed results are preserved; no automatic retry.

After games finish, independently replay logs and PGNs, validate legality,
clock arithmetic, game results and pair assignment. Report corrected-engine
W/D/L, colour splits, termination reasons and score confidence intervals using
10,000 bootstraps of **opening pairs**, not individual games. Include a
conservative bounded-score interval for degenerate samples. This is a short
time-control screen, not a precise Elo or general strength claim. It cannot
identify which individual correction causes any observed change.

Source inventory and hashes are checked before and after execution. A durable
systemd user service sends one completion/failure desktop notification. There
is one startup snapshot and **no chat polling**.

Command: `bash research/forge_search_match_20261023/start_j49.sh --approve-j49`
