# Advance repetition guard — isolated candidate, specified checks passed

Base: exact `engines/forge-capacity-wide192/src`, with adopted J47/J48
corrections, exact Wide192 checkpoint and existing defaults. The research
`RepetitionGuard` option defaults **off**. No daily-driver files were changed.

The reversible-move Zobrist-difference algorithm and alternating-history
filter follow Stockfish, including Marcel van Kervinck's repetition-detection
work. Reference: Stockfish commit
`edb0d9db6731067ec50ce619ff372b463bc4dd5d`, `src/position.cpp`,
`Position::upcoming_repetition` and the move-delta initialization.
Stockfish copyright 2004–2026 the Stockfish developers; GPLv3-or-later.
Keep this attribution and the existing GPL / Fathom notices with derivatives.

FORGE adaptation uses a fixed open-addressed table rather than cuckoo hashing,
normalized repetition keys (including the existing pinned-EP correction),
and both pseudo-legality and king-safety validation. There are 3,668 geometric
reversible moves in an 8,192-slot table; with 64-bit keys and 16-bit moves the
arrays occupy 80 KiB. No model features, weights, or HCE scores are added.

The new search use is deliberately narrower than Stockfish's: only a non-PV
zero-width window whose beta is at most zero can return the legal draw bound.
Excluded-move searches bypass it; PV and root nodes retain their current
behaviour. It writes no TT entry itself and does not alter the existing rule
for twofold cycles inside search versus threefold repetition before the root.

J78's specified correctness checks passed on 26 October 2026. Its median cost
was small, with meaningful individual regressions. No strength test or automatic
promotion followed. See the parent directory's RESULTS.md and frozen run.

This is a correctness-motivated search candidate, not a demonstrated cause of
the historical failures, a fortress solver, or a proven strength improvement.
