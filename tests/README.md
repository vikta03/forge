# Regression fixtures

`positions.fen` is the first 64 positions, in stored order, from the historical
`results/next_strength/foreign/bank.epd` (300 positions). These are numerical
and protocol fixtures, not a strength suite. No results informed the selection.

The C++ `test` command retains the inherited rule, SEE, make/unmake, perft and
Chess960 cases. Added clock-cache checks cover payload ownership and Hash
accounting. Audit builds additionally expose counters from actual main search
and quiescence; production builds contain no audit counters.
