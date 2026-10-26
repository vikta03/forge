# J71 Wide192 with two 64-unit heads

Twelve passes from scratch. Compare with the first Wide192/four 32-unit heads, using the same seed, corpus and schedule. Placement architecture is fixed; learned placement weights can change with training. This tests reader design overall, not width alone. Validation selects checkpoints, so these are exploratory intervals, not untouched-test or strength estimates.

| Model | WDL CE | Teacher KL | Expected-score MAE | Quiet-policy CE |
|---|---:|---:|---:|---:|
| Wide192 / 4x32 | 0.6400604 | 0.1711210 | 0.0866783 | 2.9114408 |
| Wide192 / 2x64 | 0.6372639 | 0.1683245 | 0.0866233 | 2.9061187 |

Candidate minus first Wide192: {
  "games": 20944,
  "positions": 2106816,
  "ce_difference": -0.002796447835817366,
  "ce_difference_95": [
    -0.003365265274690123,
    -0.0022070203910460858
  ],
  "mae_difference_points": -0.005503739807271031,
  "mae_difference_points_95": [
    -0.022500789508753262,
    0.011388092010162233
  ],
  "note": "Position-weighted paired source-game bootstrap; validation selected checkpoints, not an untouched test or Elo."
}

J72: 400 games directly against the first Wide192/4x32, 8+0.08 seconds per side, concurrency 8. All original packages stay intact; no automatic daily-driver promotion.
