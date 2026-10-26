# J77 — separate Stockfish evaluation from search

Authorized by the user's 26 October request: “first of all do that. then tell
me”, referring specifically to Stockfish static evaluation versus search with
tables disabled. This is the diagnostic authorization, not permission for
training, a match, or promoting speculative search settings.

Expected 2–3 minutes, maximum six minutes cumulative, two physical CPU cores,
2 GiB RAM. CPU only. No builds, games, training, downloads, or installed-package
changes. Charger, shared hardware lock, STOP and resource guards apply.

Reuse all twenty J76 roots with their complete saved histories. Pin the existing
Stockfish 19 executable and its network, plus the original Wide192 executable
and model. Use one thread, 32 MiB hash, MultiPV 1, 10 ms move overhead, clean
search state per measurement. Stockfish SyzygyPath is empty and ProbeLimit is
zero. Require zero tbhits in every Stockfish search output.

For each root record Stockfish's no-search NNUE trace and final static score,
then independent cold 100 ms, 1 s and 5 s searches. Record Wide192's native raw
logits/score and a fresh one-second search, alternating engine order. Replay all
reported PVs independently; preserve score bounds and distinguish a displayed
PV move from the final bestmove. Normalize scores to the original side to move.
Stockfish trace scores are rounded displayed values, not unscaled tensors.
Cross-engine centipawn magnitudes are not calibrated to the same probability.

Use the existing exact J76 table outcomes only as separate ground truth; no
engine consults them. Larger positions remain unsolved unless established by
other evidence. Inspect selected final PV leaves with static evaluation to
illustrate whether search reached a different assessment, without treating one
PV as proof of every defence. Vary the halfmove clock on copies of two boards
for a small static-input sensitivity check; these synthetic copies intentionally
discard game history and are labelled separately.

Save identities, UCI transcripts, all measurements and a verification receipt.
Freeze failed attempts; source/report repairs must fit the same cumulative cap.
One completion notification; no heartbeat or repeated chat polling.
