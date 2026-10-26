# J74 — matched WDL versus expected-score objective pilot

no admission to full training on this pilot.

Both Wide160 models started from identical random parameters and saw the same 67,108,864 positions in the same batch order. The comparison uses the fixed final dose, not a winning checkpoint selected afterward.

| Panel / game-mean metric | WDL control | Score-focused | Candidate minus control, 95% interval |
|---|---:|---:|---:|
| Ordinary: expected-score MAE (percentage points) | 11.513589 | 11.591400 | +0.077811 [+0.053009, +0.102972] |
| Ordinary: expected-score MSE | 0.029392 | 0.029148 | -0.000244 [-0.000355, -0.000139] |
| Ordinary: WDL KL | 0.248202 | 0.255177 | +0.006975 [+0.005988, +0.007967] |
| Quiet endings: expected-score MAE (percentage points) | 11.261801 | 11.316066 | +0.054265 [-0.003314, +0.115370] |
| Quiet endings: expected-score MSE | 0.029290 | 0.029118 | -0.000172 [-0.000421, +0.000097] |
| Quiet endings: WDL KL | 0.319598 | 0.325942 | +0.006345 [+0.003786, +0.009067] |

Negative error differences favour the score-focused candidate. Source games, not individual positions, are the bootstrap units.

## Search and exports

Short diagnostic search summary: `{"wdl": {"median_nps": 1401755.0, "median_depth": 24.0}, "score": {"median_nps": 1372723.0, "median_depth": 23.5}}`. Same inference operations and search, different learned weights and trees. These 24 positions are not a strength test.

- wdl: `workspace/research/forge_objectives_20261025/runs/20261025T152814Z/engines/wdl/forge` (research pilot only)
- score: `workspace/research/forge_objectives_20261025/runs/20261025T152814Z/engines/score/forge` (research pilot only)

## Limits

- One random seed and a short scratch schedule; not full-data convergence.
- Panels are game-disjoint from training but were used in earlier diagnostics.
- Intervals quantify panel game variation, not random-seed uncertainty.
- No new labels, sibling rankings, matches, or demonstrated playing-strength gain.
- Loss coefficients are fixed recipe choices; a failure does not reject every scalar-focused objective.

Wide160 daily driver is unchanged. No automatic full training, games or promotion.
