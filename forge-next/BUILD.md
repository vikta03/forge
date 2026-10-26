# Build a selected engine

Target: **Linux x86-64, Haswell-compatible CPU**: AVX2, FMA and BMI1/2.
Use a C++20 compiler (GCC 12 or newer). The fixed CPU target avoids accidentally
requiring the development laptop's newest instructions. Runtime evaluation
needs no Python, GPU, PyTorch or external model library.

From a checkout of this repository:

```bash
cd forge-next
./build.sh wide192
python3 tools/fetch_model.py wide192
./forge
```

In a UCI GUI, select the absolute path to `forge-next/forge`. The launcher
resolves its binary and weights relative to itself, including paths with
spaces. It checks that the files exist; the native loader checks model format,
dimensions and CRC. The download helper checks SHA256 before installing.

All variants are listed in [models.json](models.json). For example:

```bash
./build.sh reader64
python3 tools/fetch_model.py reader64
FORGE_VARIANT=reader64 ./forge
```

For a GUI that cannot set environment variables, use a two-line shell wrapper
that exports `FORGE_VARIANT=reader64` and executes the absolute `forge` launcher
path. Alternatively invoke `bin/reader64 --model /absolute/path/forge-reader64.fgc`.
`FORGE_MODEL` overrides the launcher's model path. Preserve the variant/model
pair: equal file sizes do not imply compatible architecture or identical weights.

The new top-level build/launcher adapters were syntax/path checked for this
publication; they have not received a new compilation or relocation search.
Native source files match the historical packages byte for byte. Refer to
`SOURCE_PROVENANCE.json` for hashes and the individual experiment receipts
for actual engine verification. Locally built executables inherit the host's
glibc/libstdc++ requirements; there is no new universal binary package.

## Settings and experiments

The daily-driver Wide192 uses the existing J47/J48 search and defaults. Its
quiet policy is on; value, pruning and time-management defaults are preserved.
It uses the historical compact TT identity policy. **Wide160 FullKey is an
explicit different comparison variant**, not a hidden change to Wide192.

`repetition-guard` shares Wide192's model. After building/fetching that variant,
send `setoption name RepetitionGuard value true` to enable the research mechanism.
It defaults off and has no demonstrated match-strength gain.

`wide192-syzygy` is the separate tablebase integration. Set UCI `SyzygyPath` to
your own table directory. Table files are not distributed. A successful probe
only answers covered positions; the presence of a table directory does not
guarantee every larger fortress is resolved. See [J76](evidence/J76/RESULTS.md).

Regression fixture source is in [tests](tests/README.md). Do not interpret a
synthetic contract passing as a strength result.
