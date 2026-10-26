# J62 Wide160 FullKey: correctness and NPS

Same Hash64 budget, one pinned search core; unchanged model and quiet policy. Two counterbalanced one-second searches per arm on 48 independent-game positions. Model loading and cache clearing excluded. No games or strength claim.

| Engine | Median GUI NPS | Actual nodes/wall second | Median depth | Startpos GUI NPS (3 s) | Startpos depth |
|---|---:|---:|---:|---:|---:|
| control | 1,752,023 | 1,738,477 | 20.0 | 1,554,180 | 24.0 |
| candidate | 1,729,454 | 1,711,027 | 20.0 | 1,560,121 | 25.0 |

Paired median throughput change: **-1.66%**, 95% game-bootstrap interval [-1.98588162444715, -1.1635210921573658]. Faster on 5, slower on 43 usable positions. Median depth change +0.0.

Usable positions: 48/48. Excluded symmetrically for early termination/depth ceiling: []. All raw observations retained.

| Warm history | Original replay NPS | FullKey replay NPS | Change | Endpoint depths: original → FullKey |
|---|---:|---:|---:|---|
| 008 | 1,779,489 | 1,760,173 | -1.09% | [31, 29] → [27, 36] |
| 009 | 1,872,978 | 1,833,773 | -2.09% | [37, 43] → [37, 43] |
| 013 | 1,786,417 | 1,786,889 | +0.03% | [33, 35] → [34, 33] |
| 018 | 1,942,508 | 1,960,670 | +0.93% | [32, 35] → [26, 34] |

Each warm history contains 64 successive 250k-node positions and two 2-second endpoints. Four engineering histories are a small, previously examined sample. They preserve game prefixes and retained TT/history; cold timings clear state between positions.

GUI NPS is the final emitted iteration, which can precede unfinished work. Wall NPS counts all nodes through bestmove. Changed cache capacity/identity also changes trees; these are actual search-throughput comparisons, not isolated lookup-cost measurements. Key companions and clock tags fit the stated Hash budget, costing about 34% of the original entry capacity.

Regressions (paired per-position mean NPS, sorted worst first):

- P223 (endgame): -4.27%.
- P306 (opening): -3.08%.
- P328 (endgame): -3.03%.
- P180 (forcing): -3.01%.
- P293 (quiet): -2.79%.
- P039 (endgame): -2.74%.
- P288 (endgame): -2.58%.
- P244 (forcing): -2.46%.

Original package unchanged. No rating or improved-strength claim. Receipts: gates.json, parity.json, raw-eval.json, startpos.json, timings.json, warm.json, summary.json and the supervisor completion.json.
