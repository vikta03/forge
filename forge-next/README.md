# FORGE · the capacity experiments

**A family of trained CPU chess engines, with the experiments that shaped them.**

By **Remmy Victor · [@vikta03](https://github.com/vikta03)**.

The current daily driver is **Wide192**. It learns a compact description of a
board, updates that description as pieces move, and lets one small specialist
read it. Alpha-beta search uses the result to explore continuations.

**[Build Wide192](BUILD.md)** · **[Download the models and source](https://github.com/vikta03/forge/releases/tag/research-2026-10-26)** · **[Architecture](ARCHITECTURES.md)** · **[Results](EVIDENCE.md)** · **[Visual tour](visuals/README.md)**

![The four-head Specialists model opened up in the anatomy explorer](visuals/explorer/overview.png)
*The interactive explorer shows the earlier trained Specialists model, with exact
weights and arithmetic checked on 128 saved native positions. Wide192 broadens
the shared memory; this screenshot is not a Wide192 activation trace.*

## What is here

| Engine | The experiment | Status |
|---|---|---|
| **Wide192** | 192 channels per board perspective; four 32-unit readers | Current daily driver |
| Specialists | 128 channels; four readers; skip the reserved zero inputs | Historical reference |
| Wide160 | Use the empty input space for more learned placement channels | Historical comparison |
| Wide160 FullKey | Verify the full transposition-table key within the same Hash budget | Separate correctness/cost comparison |
| Deep32 | Add a second 32-unit layer behind the 128-channel representation | Lost its short-clock screen |
| Reader64 | Wide192 memory, two larger 64-unit readers | Lost its 400-game screen against Wide192 |
| Wide192 + Syzygy | Connect exact endgame answers through Fathom | Separate scoped experiment; no table files included |
| Repetition guard | Recognise a legal drawing cycle before selective pruning | J78 research candidate; option off by default |

The latest two investigations are especially useful:

- **[What Stockfish sees before searching](evidence/J77/RESULTS.md):** with
  tablebases disabled, its network already recognises one of FORGE's troublesome
  drawn endings. Other positions need search. The missing ingredient is not
  simply a missing halfmove-clock input.
- **[The repetition guard](evidence/J78/RESULTS.md):** passed its specified
  correctness checks and cost about 0.27% median NPS in a small screen, but did
  **not** repair that fortress. Depth regressions remain visible in the report.

## Reproducibility, without pretending the evidence says more

This snapshot contains exact native source for each packaged variant, model and
training mathematics, export tools, regression fixtures, match PGNs, diagnostic
receipts, and illustrations. The historical model files are release downloads
identified by SHA256 in [models.json](models.json).

No engine was rebuilt, retrained or match-tested just to publish this snapshot.
New path/launcher/training-entry adapters receive static publication checks;
the engine experiments have their own dated receipts. The historical local
binaries are identified, but **not distributed as portable Linux binaries**.

Training used **213,899,602 retained training positions** and **2,106,816 validation
positions**. Those are records, not independent games or newly generated labels.
The training corpus and optimizer states are not bundled. See [Training](TRAINING.md).

This research release does not replace [netB v1](../docs/evaluation.md) or
[the FORGE 2 source edition](../forge2/README.md). Historical `promoted: false`
fields are preserved; the user's later adoption of Wide192 is a preference,
not proof that it beat Wide160.

[Source identities](SOURCE_PROVENANCE.json) · [Publication checks](PUBLICATION_CHECKS.json) · [Licences](LICENSE.md)
