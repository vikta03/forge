# Training source and external inputs

By Remmy Victor (@vikta03). The full explanation is in
[the engine guide](../ENGINE_GUIDE.md#15-training-examples-and-their-boundaries).

These are the core algorithms used by the completed full-placement run J38:
compressed-game reading, compact encoding, data filtering, pooled forward and
ragged policy loss, GPU optimization, validation, checkpointing and export checks.
`reference/` contains the independent board/codec/model implementations used for
comparison. No raw corpus, optimizer state, GPU libraries or trained relational
model is included.

## Required inputs for the exact run

| Input | Contract |
| --- | --- |
| SQLite manifest | `meta(key,value)`, `archives(id,path,bytes,mtime_ns)`, `games(id,archive,offset,bytes,sha256,positions,split)`; read-only; snapshot must match config |
| Compressed teacher games | Indexed gzip members containing supported Lc0 raw v5/v6 records; offsets/lengths, inflated SHA256 and frame counts must match |
| Pilot checkpoint | `kind=placement`, model state, correct snapshot; SHA256 `c8f403bb64ffcda5bb60957cb8cf4eab77236ddc95ec3fe5ea111c7497178dee` |
| Pilot input-key cache | Original committed train shards and receipts with `keys.npy`; used to exclude pilot exposure from expanded validation |
| Engineering fixture directory | For historical reader gates: `data/receipt.json` with sampled game IDs and `data/train.jsonl` rows; the source sampler is in `reference/data.py` |
| Compatible native engine and runtime model | For exported-model parity checks; set `FORGE_TEST_ENGINE` to a locally built binary |
| Execution environment | Python 3.14 (stdlib `compression.zstd`), NumPy, PyTorch with CUDA, python-chess, C++20 compiler, sufficient disk and an external resource/time supervisor |

Recorded environment versions were Python 3.14.8, PyTorch 2.13.0, NumPy 2.5.1,
python-chess 1.11.2. These are observed versions, not a claim that an installation
resolver has been tested here on every platform. Select a PyTorch CUDA build
compatible with your system. No dependency installation was performed for this
source edition.

The historical snapshot is
`8af5f4d59ea972cc721ac1b8343e6ab68e75034cb0f8c1ce13d584a0f29e412c`.
The surviving runtime `placement.fg2` does not contain the pilot's optimizer,
training cursor or input-key set. It cannot replace the missing external inputs
for an exact warm-start replay. Training from random initialization or another
corpus is a new experiment, requiring its own validation.

## Code organization and publication adaptations

`common.py` resolves `reference/` locally instead of a neighboring private research
directory. `FORGE_TRAIN_CONFIG` selects an explicit JSON config;
`FORGE_TRAIN_FIXTURES` and `FORGE_TEST_ENGINE` select external gate inputs.
`config.example.json` retains actual numerical settings but uses path placeholders.

The historical native client now selects the published runtime-model location and
standalone node telemetry label. An obsolete endgame-probe option is omitted from
the export check because the standalone engine disables specialist endgames.
`reference/verify_worker.py` contains only the pure fixture/constants/scalar portion
of the historical job script. The original machine-specific supervisor, desktop
notification, package delivery and directory-copy operations are not shipped as
if they were portable training requirements.

**These publication adaptations received static checks, not a new runtime training
validation.** Mathematical/encoding source preserved unchanged is identified by
hash in [SOURCE_PROVENANCE.json](../SOURCE_PROVENANCE.json). Historical successful
training gates belong to the original J38 run.

## Execution sequence for a prepared environment

This describes how to connect the included modules; it is not an automatic job
launcher. Arrange CPU/GPU/RAM/disk/time limits outside Python first. The guard
`FORGE_TRAINING_ALLOWED=1` is an explicit opt-in, **not** a resource limiter.

1. Copy `config.example.json` to a separate `config.json`; resolve and verify all
   external paths and snapshot identities. Retain train/validation/test separation.
2. Create a fresh run directory. Compile `pack.cpp` as the encoder shared library:

   ```bash
   g++ -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Werror \
       -Wno-misleading-indentation -fPIC -shared \
       pack.cpp -o /path/to/run/placement-pack.so
   ```

3. Run the gates in `checks.py`: `verify_reader(cfg, lib, run)`,
   `verify_training_math(cfg, fixtures, run)`; then `full_checks.full_reader` and
   `full_checks.training_resume`. Configure CUDA with `network.configure(cfg)`
   before training math. Supply the historical gate inputs above rather than
   silently skipping missing fixtures. The native export client needs
   `DebugStats=true` to collect final node telemetry; see its constructor.
4. Start `stream.build_cache(cfg, run, lib)` in a supervised producer process.
   The equivalent script entry is `python stream.py RUN_DIRECTORY LIBRARY_PATH`
   with `FORGE_TRAIN_CONFIG` selecting the configuration.
5. In a separate supervised process, call `network.configure(cfg)` and
   `full_train.train(cfg, run)`. It waits for validation-ready and committed
   training shards. Keep both processes in the same job/resource lifetime.
6. At completion or orderly stop, create `RUN_DIRECTORY/STOP`; join the producer
   at a shard boundary. Inspect its receipt and both exit statuses. Preserve
   failures. Remove the STOP marker only for an intentional compatible resume.
7. Export `models/best.pt` with `../tools/export_model.py`. Run independent native
   parity and legal-search gates in an appropriately authorized hardware job.

The full trainer chooses the best **full-validation** WDL loss. The quick monitor
sample does not select the model. The reserved test partition is never selected
by this producer. Cached training examples may repeat across shards; do not relabel
the retained count as globally unique positions.

The original supervisor supplied the actual time/resource guarantees. Numerical
limits retained in the config are used by the appropriate modules, but not every
operational field is enforced by `full_train.py` alone. In particular a wall-time
field in JSON does not terminate a Python process by itself.
