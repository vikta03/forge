# Training and export: what is reproducible

The release includes the exact architecture and runtime model, its surviving
weights-only training checkpoint, the training/export code and experimental
records. Runtime inference and checkpoint export can be reproduced from those
assets. **The original full training corpus cannot currently be downloaded
from this repository.**

The historical configuration points to unavailable FEN/target storage; the
large training bin cache and wide training sidecar are also absent from that
configuration's surviving location. A validation cache and several other
artifacts survived privately. None is misrepresented here as the complete corpus.

## Required external inputs

For each `train` and `val` split, obtain:

1. One valid standard-chess FEN per line, preserving its halfmove clock and side
   to move. No blank lines; row ordering must remain fixed.
2. Exactly one aligned **W/D/L probability triple**, little-endian float16,
   six bytes per FEN, from the **side-to-move perspective**. Values must be
   finite, within [0,1] and sum to one within float16 rounding tolerance.
3. To reproduce the historical initialization, the original common warm-start
   checkpoint widened to netB. Its surviving sanitized copy is supplied as
   `netB-initial.pt`. Starting from released best weights is fine-tuning and
   is not a replay of the 24-epoch experiment.

The historical labels' complete teacher-generation settings, sampling history,
data rights and full corpus are not reconstructed by these files. Obtain and
document those separately before claiming the historical training run has been
reproduced. Alternative licensed inputs train a new experiment.

The offline move-ranking metrics additionally need the held-out child-position
corpus, deeper-search labels and group membership used in that experiment.
It is not bundled. The historical metric logs are evidence of that run, not
a substitute for those inputs.

## Prepare a compatible dataset

Use Python 3.12+ and the pinned packages in `requirements-training.txt`.
Install the CPU PyTorch wheel for export or a matching CUDA distribution for
training. Playing does not need these dependencies.

```bash
python3 -m venv .venv
.venv/bin/pip install torch==2.13.0 --index-url https://download.pytorch.org/whl/cpu
.venv/bin/pip install -r requirements-training.txt
./tools/build_extractor.sh
.venv/bin/python tools/prepare_training.py \
  --fens /your/data/train.fens --targets /your/data/train.wdl16.bin \
  --output-prefix data/train
.venv/bin/python tools/prepare_training.py \
  --fens /your/data/val.fens --targets /your/data/val.wdl16.bin \
  --output-prefix data/val
```

This validates FENs and labels, extracts the base trace, side-to-move placement
planes and 26 extra quantities from the frozen C++ evaluator, then creates the
bin cache, phase/pawn router cells and wide bins. In-check and specialized
endgame positions are excluded by the same row mask. Output files preserve
the ordering needed by the labels and sidecars. Existing output prefixes are
refused rather than overwritten.

No historical datasets are fetched automatically. Disk and compute requirements
scale with the supplied corpus; the original run used 52,708,925 retained
training rows and 541,629 validation rows.

## Recorded netB configuration

Eight routes; 57 signed + 44 context features; 129 bins; factor width 8;
16 placement channels; 32 king buckets; product mixer 40→16; 24 epochs;
seed 20260814; batch 32,768; learning rate 0.003; weight decay 0.000001;
smoothness 0.0001; shuffle blocks 524,288; split router EM at epochs 6 and 12,
floor 0.04 and stride 8. MultiPV, sibling and disagreement auxiliary loss
weights were zero. See the [sanitized recorded configuration](../evidence/training/netB-config.json).

```bash
.venv/bin/python tools/train_netb.py \
  --data data --initial models/netB-initial.pt --out runs/netB --device cuda
```

This command is a recipe, not a claim the missing historical inputs are present.
The release verification performs no training.

## Export the surviving checkpoint

Download `netB-best.pt` from Releases. It contains tensor weights, route maps
and architecture metadata, with no optimizer state or private paths.

```bash
.venv/bin/python -m experiments.wide_trace.export_wide_runtime \
  netB models/netB-best.pt models/reexport.forge
sha256sum models/reexport.forge
```

The release gate checks the exported runtime model against:

```text
49cdece06bf5006835808ce4c2fde904765b2da26487f430c3f845a749323a6b
```

The importer checks format, shape, bin-layout digest, payload CRC and finite
parameters. Architecture compatibility is stricter than having a `.forge`
file extension.
