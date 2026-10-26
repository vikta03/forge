# Accuracy and scope of the FORGE anatomy explorer

## The direct answer to “is this 1:1?”

**The stored model is complete and exact. The forward arithmetic matches the saved native verification set. The spatial layout is an illustration.** This is not a physical copy of an engine, a complete search simulator or an exhaustive proof that every possible chess position is equivalent.

The first explorer showed representative links and sampled weights. This second version embeds every byte of the deployed model and provides exact indexed access to every tensor. It does not silently relabel the earlier schematic as exact.

## Provenance

- Model: `specialists.fg2`, 4,271,904 bytes, including its 32-byte header.
- Model SHA256: `9430224f025568d8c20504e02775ac33c74a07fccdefd4b765c0a02ce5349512`.
- Reference source: `forge_next_probe.cpp` from the J45 pass-12 engine package.
- Source SHA256: `8aaf905625bf5f215adaaf3d80f309a45477d1fe60d5ef883f86db200682f0ba`.
- Native evidence: existing J46 `quality-specialists.uci`, 128 inspections.
- Native log SHA256: `158adf688640abd2e579c93805a0045b5bcf2c6b121b561c461d6e0524298d7d`.
- The data preparer validates the file header, payload size and CRC32 and verifies the model SHA256 before extracting anything.
- The two games come from the supplied UCI logs. A chess rules library reconstructs positions and legal quiet moves, including the engine’s castling encoding. The longest complete move sequence in each log supplies the game.

The build receipt identifies the current HTML and checked source hashes. The older browser receipt applies only to the pre-repair HTML; fresh browser acceptance of the interaction repair remains outstanding (see VERIFICATION.md). No new native engine search, match, training or strength test was needed to make the explorer.

## Tensor accounting

| Stored component | Shape | Signed-int8 values | Executed by this model? |
|---|---|---:|---|
| Shared placement | 32 × 768 × 128 | 3,145,728 | Yes |
| Four hidden matrices | 4 × 328 × 32 | 41,984 | One selected head |
| Four value matrices | 4 × 32 × 2 | 256 | One selected head |
| Four query matrices | 4 × 32 × 16 | 2,048 | Selected head when policy is requested |
| From / to move tables | 2 × 768 × 16 | 24,576 | Policy |
| Special-move table | 8 × 16 | 128 | Policy |
| Contact relation table | 32,256 × 32 | 1,032,192 | No |
| Local relation table | 768 × 32 | 24,576 | No |
| Blocker relation table | 12 × 32 | 384 | No |
| **Total stored** | | **4,271,872** | |

The placement/head/move path contains 3,214,720 declared parameters, hence the rounded 3.21M label. This includes weights connected to reserved-zero inputs and embedding rows that need not be used by a particular position. It is not a count of multiply-add operations. The full binary retains 1,057,152 inactive relation parameters for format compatibility.

## Exact computational path implemented

1. Encode each piece relative to each colour: six own identities and six opponent identities. Black’s square coordinate is vertically flipped with XOR 56. Piece row is `relative_type * 64 + canonical_square`.
2. Each king chooses a bucket: `rank * 4 + min(file, 7 - file)` in that colour’s canonical coordinates. This groups mirrored king files; it does not apply a horizontal flip to the whole board.
3. For each colour, sum the 128-component placement vector of **every piece**. Two independent sums result. The browser rebuilds them directly instead of emulating incremental make/unmake updates.
4. Put side-to-move channels first, opponent channels second. Each is `clamp(trunc(sum / 2) + 32, 0, 127)`.
5. Append 64 reserved zero inputs and eight metadata values: halfmove clock (clamped to 0…255 then integer-divided by 2), material phase, own and opponent castling rights ×16, canonical en-passant square +1 or zero, piece count, check flag, constant32. Legal en-passant availability is normalized by the data preparation rules library.
6. Select head `clamp(trunc((piece_count - 1) / 8), 0, 3)`. The normal ranges are H0:2–8, H1:9–16, H2:17–24, H3:25–32. There is no interpolation.
7. For hidden unit j: `s[j] = sum_i input[i] * Hidden[i,j]`; then `hidden[j] = clamp(trunc(s[j] / 64), 0, 127)`.
8. Value outputs are `a,c = hidden * Value / 256`, with a the conditional-win logit and c the decisiveness logit. `W = sigmoid(c)*sigmoid(a)`, `D = 1-sigmoid(c)`, `L = sigmoid(c)*(1-sigmoid(a))`.
9. Let `E = W + D/2`. The scalar is mathematically `round(187*logit(E))`, clamped to ±12,000 centipawns. The implementation uses the source’s stable log-domain expression and half-away-from-zero rounding. The floating-point WDL display is a derived probability display; the acceptance checks compare the native integer outputs, not bitwise identity of floating-point exponentials.
10. The selected hidden state can produce a 16-dimensional signed query: `clamp(trunc(hidden * Query / 64), -127, 127)`. A move’s raw score is the dot product with the sum of the corresponding from/to/special embeddings.

The viewer deliberately computes all four heads so that their same-board outputs can be compared. The engine selects one head per evaluation. Counterfactual heads are clearly labelled and may be outside their trained piece-count range.

## What the native comparisons establish

The browser and Node evaluator compare all 128 frozen inspections with the same model:

- 4,096 hidden activations: 32 per position.
- 256 raw value logits: two per position.
- 128 scalar scores.
- 3,938 raw quiet-policy logits, including exact move-set agreement.

That is 4,480 value-path integers plus 3,938 policy integers, all equal. The fixtures are stored separately from the browser computation. These are comparisons to existing C++ output, not tests that merely repeat the JavaScript formulas.

The 317 game position records are evaluated live in the viewer. They are not all separately checked against native engine output. Passing 128 fixtures is strong bounded evidence, not proof over all legal chess states. The page is a research explanation tool, not a replacement UCI engine.

## What each visual encoding means

| View | Exact | Illustration or qualification |
|---|---|---|
| Chessboard | Saved position, pieces and squares | Piece meshes and board scale |
| 32 memory panels | Full 768×128 texture per bucket | Screen pixels may combine many texels at a distance |
| Weight universe | Every one of 98,304 weights in one bucket; 8,192 per isolated piece layer | Spatial layout is square/channel/piece indexing, not a semantic embedding plot |
| Weight colour | Sign and magnitude from stored int8 | The 3D view applies a power of 0.72 to normalized magnitude and saturates at the chosen range; no linear-brightness claim. Ledger heatmap is linear before screen colour rendering |
| Accumulator / input dots | 256 placement channels and all 328 inputs | Geometry, size and brightness mapping |
| Specialist nodes | All 32 hidden units, two value outputs and 16 query outputs per head | Inactive heads are diagnostic counterfactuals |
| Incoming neuron links | Exact products for the selected unit; all 328 terms accessible | Scene defaults to the strongest 12 absolute products; All terms draws all 328. Link colour follows signed product, scaled within the displayed set. This is arithmetic, not a causal attribution |
| Hidden-to-output links | Every hidden-to-value/query weight, 576 edges per head | Layout and dimming |
| WDL cylinders | Fill height proportional to the engine-selected head’s WDL | Counterfactual head values appear in their own inspectors; they do not replace these columns. Height and spacing are arbitrary |
| Stage roads | Correct aggregate data flow | Each road is not an individual learned connection |
| Guided walkthrough | One real piece/weight/channel/input path, exact hidden/output arithmetic | Manual Next/Previous or optional ten-second autoplay; timing is explanatory, not measured runtime |
| Policy ranking | Computed raw logits and all 16 dot-product terms for selected legal quiet moves | Board arrows identify the inspected move, not a recommendation. Not actual search order; TT/capture/refutation stages, history and candidate filtering matter |

The boundary microscope compares old and new heads on **the same post-capture board**, converted to White’s perspective. It isolates the numerical routing gap for that board. It does not prove a discontinuity caused a bad move or establish which head is more accurate. Looking only at successive raw scores would also mix in the capture and a change of side to move.

## Deliberately outside scope

Alpha-beta search; real search candidate lists and history scores; transposition/cache mechanics; AVX2 instruction execution and costs; native incremental update performance; wall-clock speed measurements; learned semantic names for neurons; causal chess explanations; calibrated win probabilities against an opponent; playing-strength conclusions. None of these is inferred from attractive geometry or from a matching forward pass.
