# Installation and protocol

The release package runs on Linux x86-64 with a Haswell-compatible AVX2 CPU
(including FMA and BMI1/2), glibc ≥ 2.35 and a GCC 12-compatible libstdc++.
These are the Ubuntu 22.04 build targets; see the published dependency receipt
for symbols required by the actual binary. There is no GPU requirement.

Download `forge-netB-1.0.0-linux-x86_64-avx2.tar.gz` and `SHA256SUMS` from
[v1.0.0](https://github.com/vikta03/forge/releases/tag/v1.0.0).
Verify the archive, extract it, and select its `forge` launcher in your UCI GUI.
Keep the extracted directories together. Spaces in the path are supported.

For a source build, install GCC 12 and run `CXX=g++-12 ./build.sh` from the
repository. `python3 tools/fetch_model.py` downloads the exact runtime model
and verifies its SHA256. The model is a Release asset, not a Git blob.

```text
uci
isready
position startpos
go movetime 1000
```

Wait for `bestmove` before sending `quit`. `stop` interrupts a search.
The engine is single-search-threaded; the Hash option controls transposition
storage. It defaults to 64 MiB, including the new evaluation-clock tags.

To load another compatible model, use UCI `setoption name NetPath value PATH`.
Paths may contain spaces. `FORGE_NET` selects an alternate startup model:

```bash
FORGE_NET="/path with spaces/another.forge" ./forge
```

An invalid model clears readiness and cannot silently continue with the old
weights. The public launcher fails before UCI startup if its model is missing
or incompatible. The underlying executable also accepts `--net PATH`.
Direct `test` and `perftsuite` modes intentionally permit no model to check
the inherited rules/evaluator fixtures.

`forgeeval` prints the neural evaluation, or identifies a specialized exact
endgame path. `forgeinspect` additionally prints raw concepts and placement
indices for the independent replay. These are diagnostic commands, not UCI options.

The optional Syzygy interface is inherited and defaults to an empty path.
No tablebase files are provided. Specialized built-in endgame handling,
including KPK, is retained.
