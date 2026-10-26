# Chess, given form — expanded field guide

Open **[chess-in-form.html](chess-in-form.html)**. The three pages, diagrams,
fonts and layout work offline without JavaScript or an engine process.

- **The models:** twelve physical-looking models with 72 connected
  callouts, dimensions, narrated computation paths and source/counting notes.
- **[The handcrafted workshop](handcrafted.html):** eight illustrated Boring
  mechanisms; MyChess2's complete feature-family breakdown; netB's HCE-to-learned
  handoff; comparison of hand-designed work in the other evaluators.
- **[Field notes](notes.html):** fifteen observations, comparisons, analogies
  and a glossary. Interpretation is distinguished from measured evidence.
- **Seven deep plates:** the HCE workshop, incremental row updates, one VEGAM
  transformer block, one Stockfish 19 branch, an archived measured netB eval, Wide160/Deep32 capacity placement, and the FullKey search cache.
- `plates/collection.png`: twelve-model overview poster.
- `plates/capacity.svg`: the common-volume capacity comparison.
- `plates/*-anatomy.svg`: full-size annotated models. Every SVG has a PNG.

Models: Boring, published 106-input MyChess2, FORGE netB, FORGE 2 placement,
FORGE Specialists, earlier Vega NNUE, VEGAM, Berserk, Stockfish 19 and Lc0 BT4.

The models are explanatory software geometry, individually enlarged for
readability. Only the capacity cubes have volume proportional to the declared
coefficient inventory. Colours are not measured activations. The example board
fragments are teaching motifs, not full legal positions or engine experiments.
The netB numeric plate explicitly uses a previously saved measured evaluation.

## Source and regeneration

`SOURCES.md` and `model-inventory.json` freeze dimensions and counting scopes.
Optional local source links require the original workspace; external source
links require internet. All illustrated teaching content is bundled.

With Python 3, PyCairo, Montserrat and Adwaita fonts installed:

```bash
python3 build.py --frozen
python3 verify.py
```

`--frozen` draws from the bundled inventory, so no original engines or model
files are needed. Omitting it rechecks the original local MyChess2/Vega model
headers and VEGAM configuration. No training, model inference, search, browser
launch, benchmark or match is performed.

- `build.py`: basic geometry and capacity accounting.
- `guide_draw.py`: richer models, labels, original chess symbols and cutaways.
- `guide_content.py`: source-based explanation and commentary.
- `guide_pages.py`: three static pages and their shared CSS.
- `verify.py`: local links/fragments, HTML, SVG and arithmetic checks.
- `build-receipt.json` / `verification.json`: generation and inspection records.

Original illustration and writing by **Remmy Victor**, CC BY 4.0. Generator:
GPL-3.0-or-later. Other engine authorship, copyright and source licences remain
with the respective projects.

## 24 October additions

- [Wide160](chess-in-form.html#wide160): 160-channel shared racks, two wider
  views, the filled 328-input connector and all four small readers.
- [Deep32](chess-in-form.html#deep32): 128-channel memory, the reserved-zero
  block, and a visible second clipped layer in every reader.
- [Wide versus deep](chess-in-form.html#forge-capacity-inside): matched diagrams
  of where the extra coefficients and computation live.
- [FullKey](chess-in-form.html#fullkey-inside): a separate search-cache model,
  with exact 35/53-byte storage accounting and J62 measurements. It is not a
  new trained network.

See [NEW_ENGINES.md](NEW_ENGINES.md) for dimensions, source receipts and caveats.
The existing detailed interactive Specialist explorer is preserved.
