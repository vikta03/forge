# Attribution

- **Boring**: the earlier chess engine by Remmy Victor, which supplies FORGE's
  alpha-beta search, move generation and rules. Ordinary FORGE 2 evaluation
  uses the learned placement model; retained handcrafted evaluation is bypassed.
  The `boring` namespace and historical technical comments are retained.
- **Fathom**: retained Syzygy probing source under the MIT licence;
  disabled in this standalone playing configuration;
  copyright Ronald de Man (2013–2018), basil00 (2015), Jon Dart (2016–2020).
  The authoritative full notice is [src/syzygy/LICENSE](src/syzygy/LICENSE).
  No third-party tablebase files are distributed.
- **PyTorch, NumPy, python-chess**: external training/verification dependencies,
  installed separately under their respective licences. No framework libraries
  are bundled in the playing package.
- **VEGAM / Lc0 raw format and move vocabulary**: the retained decoder reads
  teacher records using the frozen vocabulary. Source provenance and original
  hashes are in `training/reference/codec/PROVENANCE.json`; GPL notices remain.
  External teacher datasets are not included or relicensed.
- No GitHub Actions binary packaging is run for this source-only edition.
