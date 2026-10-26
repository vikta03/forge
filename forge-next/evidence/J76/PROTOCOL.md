# J76 — local Syzygy tables with Wide192

Authorized by the user's request to plug in their tables and test the problem
positions, followed by “resume”. Wide192 is the current daily driver.

One isolated CPU build and bounded diagnostic searches. Expected 3–6 minutes;
maximum 10 minutes cumulative, two physical CPU cores, 4 GiB RAM. No GPU,
training, games, downloads or changes to the original installed engine/model.
Keep failed attempts and do not restart or expand the allowance silently.

Use the exact `engines/forge-capacity-wide192` model and source. Its existing
Fathom library is disconnected from FORGE search. Add a separate Syzygy module,
not the disabled legacy endgame evaluator. New UCI `SyzygyPath` enables it;
empty path disables it. Root DTZ uses the real fifty-move clock and conservative
distance-to-zero ordering. Root legal moves/history and searchmoves remain
authoritative. Skip the rare DTZ rounding boundary instead of claiming certainty.
Interior WDL probes are restricted to halfmove clock zero, before cache/pruning
in both main search and quiescence. Missing tables mean normal search, not draw.
Tablebase wins/losses use non-mate sentinel scores; they are not neural pawn
evaluations or distance-to-mate claims. An optional root-probe switch exists to
test interior search propagation independently.

Use `external-input/tables` read-only.
Inventory file names/sizes. Verify outcomes independently with python-chess's
Syzygy reader, including every legal move on covered historical roots. Preserve
full game history, clocks, actual scores, PVs and probe counters.

Gates: existing rule/perft and incremental fixtures; unchanged fixed-node
answers with tables off; raw model equality; legal root moves; 50-move boundary,
en passant, promotion, absent material table, castling, path with spaces,
invalid path, stop/ponder/infinite and option-clear tests. Check covered roots
against the independent oracle, plus larger historical/corpus positions and
first covered leaves reached by search. No general strength conclusion.

Output: original/candidate identities, exact table answers, off/on comparisons,
root/interior hit counts, first covered leaf and legal PV replay, explicit
coverage limitations and a separate ready-to-use research launcher if checks pass.
Safety: charger, shared hardware lock, CPU affinity, systemd RAM/CPU/wall caps,
STOP file, one completion/failure notification. No heartbeat or chat polling.
