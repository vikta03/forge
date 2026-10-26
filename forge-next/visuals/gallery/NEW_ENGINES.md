# Wide160, Deep32 and the FullKey cache

Added 24 October 2026 to the existing static gallery. These drawings describe
the deployed native models, not a proposed future design.

| Component | Specialists reference | Wide160 / J56 | Deep32 / J57 |
|---|---:|---:|---:|
| Shared placement table | 24,576 × 128 | 24,576 × 160 | 24,576 × 128 |
| King contexts | 32 | 32 | 32 |
| Piece-square rows per context | 768 | 768 | 768 |
| Own / opposing perspective | 128 + 128 | 160 + 160 | 128 + 128 |
| Reserved zero inputs | 64 | 0 | 64 |
| State values | 8 | 8 | 8 |
| Head inputs | 328 | 328 | 328 |
| First hidden stage, per head | 328 × 32 | 328 × 32 | 328 × 32 |
| Second hidden stage, per head | absent | absent | 32 × 32 |
| Value weights, per head | 32 × 2 | 32 × 2 | 32 × 2 |
| Query weights, per head | 32 × 16 | 32 × 16 | 32 × 16 |
| Stored heads / executed heads | 4 / 1 | 4 / 1 | 4 / 1 |
| Shared move tables | (768 + 768 + 8) × 16 | same | same |
| Counted coefficient slots | 3,214,720 | 4,001,152 | 3,218,816 |

The Specialist reference is J45 pass 12. Its count excludes dormant relation
tables. Counts include unused piece-code rows and, where present, hidden
weights connected to reserved zeros. They are declared stored coefficients,
not a claim that every slot is used by every position. The FGCAP01 native
Wide160 and Deep32 files contain no dormant relation tensor.

## What is exact in the physical models

- Each rack group contains **32 towers**, one per king context. The labelled
  768 rows are piece-type/colour/square identities; shelves are schematic.
- Wide160's upper rack band represents its additional 32 channels. Those
  channels are learned alongside the original 128, not a separately computed
  branch. Its two trays each contain 160 channel glyphs.
- Each head's hidden tray has 32 glyphs. Deep32 visibly has two such trays.
  A/C has two outputs and the query has 16. Grids on storage blocks simplify
  arrays; they do not claim to show millions of individual weights.
- Grey heads are unselected in an illustrative routing example. These are
  **not measured activations**. No named chess concepts are assigned to units.
- The selector is `clamp((piece_count - 1) // 8, 0, 3)`. Kings count. Legal
  two-king positions fall in H0, which spans 2–8 pieces; then 9–16, 17–24,
  and 25–32. This is mechanical routing, not a definition of chess phases.
- Wires represent dataflow, not physical processor wiring. The two perspective
  sums share one learned table. The large table is not copied for each head.
- Architecture plates are enlarged independently. Only the separate capacity
  cubes share volume proportional to the stated coefficient count. The
  comparison cutaway also gives input-strip lengths a common scale.

## Follow an evaluation

For each viewpoint, sum the active king-conditioned piece-square rows. Repair
those sums incrementally when a move changes features; refresh when necessary
for king-context changes. Convert each channel with
`clip(trunc(sum / 2) + 32, 0, 127)`.

Concatenate own and opposing views, reserved zeros when applicable, and eight
state values. The state supplies halfmove clock, phase, own and opposing
castling rights, en-passant square, piece count, check flag and a constant,
using the source's integer scaling. It does not supply Boring's ordinary
positional score.

The selected reader computes `clip(trunc(x @ W / 64), 0, 127)`. Deep32 applies
the same form again using its additional 32×32 matrix. The two output logits
and the quiet-policy query both read the **last** hidden stage.

Value logits divided by 256 give advantage `A` and decisiveness `C`:

```
q = sigmoid(A)
c = sigmoid(C)
W = c q
D = 1 - c
L = c (1 - q)
E = W + D/2
cp = round(187 log(E/(1-E)))
```

The C++ implementation uses stable log-domain algebra for the same conversion
and clamps its final scalar to ±12,000. Predictions are not forced outcomes.

The query is `clip(trunc(h @ Wquery / 64), -127, 127)`. It is dotted with the
sum of shared from/to/special move embeddings. The engine's existing quiet
ordering integration then combines the score with its search ordering rules;
this is not VEGAM's MCTS prior. Training uses a scaled version of policy
scores in its probability loss. No inference is run by this gallery.

## Training and evidence

J56 and J57 each completed twelve scratch-trained passes over 213,899,602
examples. Both used the same overlap supervision at exactly 8/9, 16/17 and
24/25 pieces: adjacent value-head loss weight 0.25 against the same board's
teacher WDL, active head weight 1, normalized by 1.25. Policy still trains
only the routed query. Inference still selects one reader.

Thus comparison with the older Specialists checkpoint changes both architecture
and boundary supervision. The two new architectures share this recipe, but
one run each does not establish seed robustness. Validation also selected the
checkpoint; it is not an untouched strength test. The gallery cites the
existing validation figures and J59 match screen, without rerunning either.

Historical evidence:

- Training protocol (archived local reference)
- Training results (archived local reference)
- Native exports (archived local reference)
- [J59 match results](../../evidence/J59/RESULTS.md)

## FullKey is a search-cache change

The additional cache cutaway shows a packed 32-byte cluster: three 10-byte
entries and two padding bytes. The original stores three clock bytes beside
it. FullKey stores three seven-byte companions: six bytes of high key bits
plus one clock each. Therefore total accounted storage changes from
**32 + 3 = 35** to **32 + 21 = 53 bytes** per cluster.

The existing low 16-bit tag remains a quick filter. The high 48 bits must
also match before reuse; final storage rechecks ownership because recursion
can replace a retained slot. Clock validity governs cached static evaluation.
The prior search-bound key, ageing and replacement policies remain unchanged.
A full 64-bit hash collision remains possible; this is not comparison of
every board bit or a solution to every history-dependent search question.

J62 passed in 353.28 seconds. At Hash64, the table holds 3,798,612 entries
instead of 5,752,188. Paired median wall NPS declined 1.66% in the 48-game-
position screen, with median depth unchanged. GUI median NPS was 1,729,454
versus 1,752,023. These are different summaries: the paired percentage is not
the ratio of the two aggregate GUI medians. No match was run.

[J62 results and conditions](../../evidence/J62/RESULTS.md)
and delivery audit (archived local reference).

## Reproducibility

`capacity_content.py` contains the exact tensor dimensions used for inventory
arithmetic. `capacity_draw.py` builds the two explanatory cutaways. The new
full models share the gallery's drawing grammar in `guide_draw.py`.
`new-engine-receipts.json` records the native model hashes, byte sizes and
the hashes of the code/evidence read for these additions. Both native model
hashes were checked against J58's export receipt.

Render with `/usr/bin/python3 build.py --frozen` and check with
`/usr/bin/python3 verify.py`, from this directory. Pycairo and the documented
system fonts are required for regeneration; viewing the exported SVG/PNG/HTML
requires no engine, Python, JavaScript or external asset download.

Original writing and illustrations: Remmy Victor, CC BY 4.0. Generator code:
GPL-3.0-or-later. Existing third-party attributions remain unchanged.
