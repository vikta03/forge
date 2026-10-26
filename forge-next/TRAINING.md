# Training and export

The mathematical model, encoder, losses, optimizer, batching, checkpointing,
integer export and validation implementations used in the runs are included.
Local job supervisors, system services, private corpus paths and optimizer
checkpoints are intentionally excluded.

| Directory | Used for |
|---|---|
| `training/specialists` | J45 four-head 128-channel Specialists |
| `training/capacity-v1` | J56/J57 Wide160 and Deep32 |
| `training/capacity-v2` | J69 Wide192, with code for the screened capacity candidates |
| `training/reader64` | J71 Wide192 with two 64-unit readers |
| `training/reference` | Shared original model/codec definitions |

Only `common.py` path/authorization glue is adapted in these trainer copies.
Numerical model, loss, schedule and forward/export code are unchanged. Source
hashes and individual exceptions are recorded in `SOURCE_PROVENANCE.json`.
Merely having a candidate in a constructor does not mean it received full
training or beat another model.

## Required external data

The retained corpus contains 213,899,602 training records and 2,106,816 validation
records. Labels come from Lc0 game/training records: board/state, teacher WDL
and move policy. Original compressed games, the selection manifest and retained
cache are **not** included. You must obtain suitable records under their own
terms; these scripts are not a downloadable 216-million-position dataset.

The encoding path is `training/pack.cpp` plus each family's `encode.py` and
`training/reference/codec`. It turns original records into two sets of indexed
piece features, eight state inputs, WDL and sparse move probabilities. The
existing [FORGE 2 data preparation](../forge2/README.md) and
[`forge2/training/stream.py`](../forge2/training/stream.py) describe the producer
and manifest flow. `forge-next` consumes the completed `npy-zstd-1` cache:

- `train-00000`, … and `valid-00000`, … shard directories with `receipt.json`;
- Zstandard-compressed NumPy arrays, with the field names and types specified
  by `encode.save_rows` / `encode.load_rows`;
- `producer.json` with complete status and `splits.train.rows` / `splits.valid.rows`;
- `validation-ready.json`, created only after validation is complete.

Game identity and train/validation separation must survive extraction. Counts
and corpus selection determine what a reproduced training result means. A
different corpus is a new experiment even when the network dimensions match.

## An explicit training entry point

The recorded environment used Python 3.14.8, PyTorch 2.13.0, NumPy 2.5.1 and
python-chess 1.11.2. Python's `compression.zstd` is required by the cache reader;
training additionally needs a compatible CUDA PyTorch installation. Runtime
engine inference needs none of these dependencies. Arrange wall-time, RAM and
GPU limits for your own hardware before starting.

```bash
# From forge-next; supply your complete, compatible cache.
FORGE_TRAINING_ALLOWED=1 python3 tools/train_cached.py capacity-v2 \
  --config training/capacity-v2/config.example.json \
  --cache /path/to/encoded-cache --output /path/to/wide192-run

# Export your own trusted checkpoint after training.
python3 tools/export_checkpoint.py capacity-v2 \
  /path/to/wide192-run/models/best.pt /path/to/my-wide192.fgc
```

`reader64` selects its matching family/config; `capacity-v1` supports `wide160`
and `deep32` through the architecture field. The entry checks corpus counts and
refuses a changed resume configuration/cache path. The original trainer also
checks full-pass coverage and checkpoint identity. This new portable entry was
statically checked, not used to rerun the historical training for publication.
The entry preserves a shard-receipt manifest; capacity-family loaders use it to
authenticate compressed arrays on first consumption and reject changes during
training. The earlier Specialists loader retains its historical behaviour.

The configuration files preserve the recorded schedule. They do not grant a
portable ETA or imply the full dataset is available. A clean machine cannot
reproduce the exact reported metrics without the exact external data selection.

## Downloaded model versus a training checkpoint

Release `.fgc` / `.fg2` files are compact runtime weights with format, dimensions
and CRC. They are sufficient to build and play the published engine. They are
not optimizer checkpoints. `models.json` gives SHA256 identities. The export
tools preserve integer rounding, truncation and layout; changes to them can
change evaluation even if the floating-point training loss is unchanged.
