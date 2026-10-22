# Regression fixtures and historical checks

The native source includes rule and Chess960 tests (`src/tests.cpp`), 28 perft
positions (`src/bench.cpp`), evaluation-clock cache regressions and the incremental
neural self-test (`src/forge_next_probe.cpp`). After building and placing the model,
`./forge2-policy test` and `./forge2-policy selftest` run those suites. They consume
CPU and are not automatically executed by this source release.

`engine_client.py` preserves the J40 UCI harness and its standard/special-position
fixtures. `startpos-recorded.json` is the first saved J40 candidate inspection,
used by the guide. It includes exact hidden activations, value dots and quiet
policy scores. Native castling diagnostics use king-to-rook notation.

Additional independent encoder/gradient/export/resume checks are in `training/`.
Some require external historical data/checkpoints. Their publication path
adaptations have not been runtime-verified afresh. See the training notes.

The evidence directory records earlier completed checks. This publication checks
source identity and packaging; it does not claim a new clean-build pass.
