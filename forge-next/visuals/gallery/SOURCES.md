# Chess, given form

Original illustrations and writing: **Remmy Victor**. Prepared 23 October 2026.

Open [the static gallery](chess-in-form.html). Also open the [handcrafted workshop](handcrafted.html) and [field notes](notes.html).
The [collection poster](plates/collection.png)
and [capacity plate](plates/capacity.png) are also ordinary images. Vector copies
are in `plates/`. There are no live controls, model calls, external resources,
animations or JavaScript in the gallery.

## How to read the models

The architecture plates are three-dimensional **schematics**, enlarged
individually for readability. They show the main ordinary evaluation paths.
They are not replicas of silicon, an exhaustive engine execution graph, a
neuron per learned weight, or a model of playing strength.

- Towers represent indexed learned tables; dots represent channels or units.
- Stacked square surfaces represent transformer blocks. The 8 VEGAM blocks
  and 15 BT4 blocks are explicitly drawn; their 12/32 attention-head tabs are
  also explicitly drawn. A grid on a surface is an architectural glyph.
- Thin connections bundle data flow. They do not enumerate every matrix entry.
- Four FORGE specialist heads are stored, but normal evaluation selects one.
  Likewise, Vega and Stockfish store eight output branches and select a branch.
- Boring's paired score rails, pawn cache, attack maps, ordinary-score modules
  and endgame bypass are drawn explicitly. Eight workshop close-ups explain
  the source mechanisms with illustrative chess motifs; they are not neural layers.
- Colour identifies components and families, not measured neural activity.

The separate **capacity cubes** all use the same camera and scale. Their
volume is proportional to the stated parameter inventory. For a count `P`,
each cube edge is `38 × cube_root(P / 1,000,000)` drawing units. Neither a log
scale nor a minimum visible size is applied. MyChess2 really is that small.
The repeated Specialists and VEGAM cubes in the second row use the same size
as in the first row.

These counts require explicit scopes: export folding, reserved storage and
auxiliary outputs make a single universal "active parameter" count misleading.
They do **not** measure per-position operations, RAM, file size, useful
independent features, or Elo. A sparse NNUE can contain more stored coefficients
than a transformer while touching far fewer of them in each position.

## Local sources

### Boring

Source: `external-input/engines chess/boring/src/eval.cpp` and its README.
No learned neural parameters. Material, piece-square values, pawn structure,
piece terms, mobility, king safety, threats and space feed a phase blend and
scaling. The workshop also describes the separate recognized-endgame bypass.
Pawn and king-shelter cache validity, non-linear king danger, passer path
analysis, phase taper, endgame scaling and ordinary score orientation were
checked in eval.cpp, eval_terms.h, pawns.cpp / .h and endgame.cpp.

### MyChess2

Selected the **published 106-input baseline**, not an unnamed later variant.
Source: `external-input/mychess2`.
The `nets/baseline.nnf` header was read without running the engine:
`106 → 128 → 32 → 3`, ReLU hidden layers, FP32 weights and biases.
Count: `(106+1)128 + (128+1)32 + (32+1)3 = 17,923`.
File: 71,728 bytes including its header. The active worktree also contains
130-feature and other experiments; those are not mixed into this plate.

### FORGE netB

Sources: `releases/forge/README.md`,
`releases/forge/evidence/training/netB-config.json`, and
`releases/forge/experiments/forge/section_lattice_model.py`.
**620,609** is the saved training-definition count. The export folds shared
and routed direct tables together and uses mixed numeric types, so that number
is not a deployed byte count. Runtime file: **730,614 bytes**.
The plate groups the 57 signed and 44 context inputs into their learned
tables, shows the 16-wide placement branch and the eight routed 40→16 mixers.
Advantage and decisiveness have independently selected routes; eight routes
do not mean eight full model executions for every position.

### FORGE 2 placement

Sources: `releases/forge/forge2/tools/model.py`,
`releases/forge/forge2/models/placement.fg2.json`, and `ENGINE_GUIDE.md`.
Declared placement/head/policy count: **3,181,504**. The full file retains
**1,057,152** unused relationship entries, excluded from the cube. The count
still includes weights connected to the 64 reserved-zero head inputs and
unused policy piece codes. It is not a count of independent effective weights.

### FORGE Specialists

Sources: `research/forge_specialist_explorer_20261023/v2/ACCURACY.md` and its
`reference/forge_next_probe.cpp`. Reference is the J45 pass-12 model already
used in the detailed explorer. Declared placement/head/policy count:
**3,214,720**, with the same dormant relation storage excluded.
The extra three heads add only **33,216 entries**, about **1.04%** over the
placement model's declared path. This is capacity, not a measured speed cost.
Each head has 328→32 hidden weights, 32→2 value weights and 32→16 policy-query
weights. Selection is by total occupied-square count: 2–8, 9–16, 17–24, 25–32.

### Vega NNUE

Sources: `external-input/nnue.h`,
`nnue.cpp`, and `engine/experiments/overhaul/nets/nnue_contract.nnue`.
The model header confirms 24,576 input rows, 256 accumulator channels,
32 hidden units, eight branches and a 32-wide global-state projection.

The **6,637,624** deployed coefficient slots comprise:

```text
24,576 × 256 placement + 256 bias
16 × 32 global projection + 32 bias
24,576 × 8 learned PSQT values
8 × ((544 × 32 + 32) + (32 × 32 + 32) + (32 × 3 + 3))
```

Training-only factorization and auxiliary heads are excluded after export
folding. This is earlier **Vega NNUE**, not the VEGAM transformer.

### VEGAM

Sources: `releases/vegam/nextchess/model.py`,
`releases/vegam/provenance/training-model.json`, and
`releases/vegam/docs/ARCHITECTURE.md`.
Configuration: 120 input channels, width 384, eight blocks, 12 attention heads,
FFN 576, adaptive relation rank 16. The complete **9,800,012** count was
independently recomputed from layer shapes without importing PyTorch.
It includes the outcome/duration heads that default TF3-Q search disables.
The structural plate highlights position WDL and the joint policy/action-WDL
decoder; the outcome and duration outputs are shown in grey as default-disabled search
adjustments. A separate cutaway opens one exact pre-normalized VEGAM block.

## External primary sources

These are architecture snapshots, not assertions about today's strongest
available weight file. No new external engine binaries or large models were
downloaded, and no benchmark or match was run.

### Stockfish 19 · SFNNv16

[Official release](https://stockfishchess.org/blog/2026/stockfish-19/).
Pinned release-tag sources:
[architecture](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/nnue/nnue_architecture.h),
[feature transformer](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/nnue/nnue_feature_transformer.h),
[piece-square](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/nnue/features/half_ka_v2_hm.h),
[threats](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/nnue/features/full_threats.h),
[pawn pairs](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/nnue/features/pp_3wide.h).

This version has 22,528 piece-square, 59,808 threat and 4,560 pawn-pair feature
slots, giving 86,896 rows. The inferred logical coefficient inventory is
**89,957,768**:

```text
86,896 × (1,024 accumulator + 8 PSQT) + 1,024 bias
+ 8 × ((1,024 × 32 + 32) + (64 × 32 + 32) + (128 × 1 + 1))
```

Two 1,024 accumulators produce a 1,024-wide combined product representation.
Each of eight branches has 32 and 32 affine outputs, dual activations, a
concatenated output and a skip contribution. SIMD padding is excluded. The
plate is **not** the older dual-big/small-network architecture; the secondary
network was retired in Stockfish 19.

### Lc0 · BT4

[Lc0 team's architecture report and tables](https://lczero.org/blog/2024/02/transformer-progress/).
The reported BT4 has **191.3M parameters**, rounded: 15 transformer layers,
width 1,024, FFN 1,536 and 32 attention heads. Smolgen supplies position-dependent
attention biases. Learned embedding, value, policy and moves-left paths are
represented in aggregate. This is a well-documented reference network, not an
identification of the user's installed Lc0 weights. The site's 2026 note links
its successor conference publication; dimensions here are explicitly those
in the BT4 report. Rounded count means the cube is correspondingly approximate.

### Berserk · checked main-source snapshot, 23 October 2026

[Dimension definitions](https://raw.githubusercontent.com/jhonnold/berserk/main/src/types.h) and
[inference arrays](https://raw.githubusercontent.com/jhonnold/berserk/main/src/nn/evaluate.c).
The checked source defines 16 king buckets, 12,288 feature rows, width 1,024,
and `2,048 → 16 → 32 → 1` after the two accumulators. These definitions differ
from the older 512-wide architecture still described in its README.

Calculated inference inventory: **12,617,297**, including biases, excluding
alignment padding and non-neural lookup tables:

```text
12,288 × 1,024 + 1,024
+ 2,048 × 16 + 16
+ 16 × 32 + 32
+ 32 × 1 + 1
```

This is labelled a source snapshot rather than a numbered release. The links
to `main` can change later; the observed dimensions and arithmetic are frozen
in `model-inventory.json`.

## Generation and checks

`python3 build.py` uses Cairo to draw static vector geometry and PNGs.
`guide_draw.py` contains the expanded models and cutaways; `guide_content.py`
contains the source-based explanation; `guide_pages.py` builds the three HTML
pages and shared stylesheet. `python3 build.py --frozen` uses the shipped
inventory and does not require the original local model headers/config.
Only a normal build rechecks those headers and the VEGAM count formula. It does
not initialize a network, train, search, launch a browser or use WebGL. Python
bindings for Cairo and local fonts are required. `build-receipt.json` records
the exported artwork hashes, dimensions, text-bound checks and volume checks.
PNG renders are visually inspected for cropping, overlap and legibility.

Original artwork/writing: **CC BY 4.0**. Generator: **GPL-3.0-or-later**.
Engine authorship, source copyright and licences remain with their respective
projects; this gallery does not reattribute any engine source to its illustrator.

## Expanded edition source checks

The richer anatomy uses the same selected checkpoints and coefficient counts.
The additional source tracing distinguished three kinds of hand-designed work:
explicit positional scoring, semantic feature extraction, and representation /
search wrappers. Those are not interchangeable.

- MyChess2's complete frozen feature families were checked in `FEATURES.md`
  and its actual output conversion in `src/eval.cpp`. The 106-input vector is
  not a direct Boring score.
- netB's live HCE call chain was checked in `releases/forge/src/eval.cpp` and
  `src/forge_factor_net.cpp`. Route A selects the hidden mixer; the A and C
  output projections use their respective routes. The quantitative cutaway
  uses the saved `docs/evaluation.md` and `evidence/worked-evaluation.json`.
- FORGE 2's normal call path ends at `ForgeNext::evaluate`, rather than running
  the retained HCE trace code. Specialists' selector, metadata and output
  arithmetic were checked in the explorer's frozen reference source.
- Stockfish 19's non-neural wrapper was checked in the pinned
  [evaluate.cpp](https://raw.githubusercontent.com/official-stockfish/Stockfish/sf_19/src/evaluate.cpp).
  It applies complexity/optimism, material and rule-50 adjustments around
  learned PSQT and positional outputs. The branch cutaway follows the pinned
  `nnue_architecture.h` cited above.
- Berserk's wrapper was checked in
  [eval.c](https://raw.githubusercontent.com/jhonnold/berserk/main/src/eval.c):
  material-draw handling, accumulator update/refresh, phase scaling and contempt.
- BT4 dimensions remain the documented reference, not an invented detailed
  reconstruction of unverified output-head dimensions. Output blocks are
  explicitly aggregate modules. The team now asks readers to cite its
  [ICLR 2026 Chessformer paper](https://openreview.net/forum?id=2ltBRzEHyd)
  for the research; this gallery's historical dimensions come from its linked
  architecture table.

Full pages and illustrations are portable and work offline. Optional original
workspace source links naturally require that workspace; external primary-source
links require internet. The source files themselves are not copied into this
gallery or relicensed by it.

## Wide160, Deep32 and FullKey — 24 October extension

The two scratch-trained architectures are J56 Wide160 and J57 Deep32.
[NEW_ENGINES.md](NEW_ENGINES.md) gives the tensor ledger, exact integer dataflow,
training conditions, drawing conventions and limits.

- Training definition: `research/forge_capacity_overnight_20261024/network.py`.
- Native reader: `engines/forge-wide160/src/forge_next_probe.cpp`, with compile-time
  placement width and second-stage selection. Deep32 uses P128/deep1; Wide160
  uses P160/deep0.
- Native export receipt: `research/forge_capacity_nps_20261024/runs/20261024T065717Z/export.json`.
- Historical match conditions/results: `research/forge_capacity_match_20261024/runs/20261024T071726Z/RESULTS.md`.
- FullKey: `engines/forge-wide160-fullkey/src/tt.h`, `tt.cpp`, and the J62
  run at `research/forge_wide_fullkey_20261024/runs/20261024T152654Z`.

The new native model hashes and sizes were independently checked against the
export receipt. [new-engine-receipts.json](new-engine-receipts.json) stores the
source/evidence hashes used for this addition. No model was retrained, no
weights altered, and no inference was needed to create these diagrams.
External engine snapshots retain their 23 October provenance; adding these
models is not a fresh claim about the latest versions of external projects.
