# Attribution

- **Boring**: the earlier chess engine by Remmy Victor, which supplies FORGE's
  alpha-beta search, move generation, rules and base evaluation features.
  The `boring` namespace and historical technical comments are retained.
- **Fathom**: optional Syzygy probing under the MIT licence;
  copyright Ronald de Man (2013–2018), basil00 (2015), Jon Dart (2016–2020).
  The authoritative full notice is [src/syzygy/LICENSE](src/syzygy/LICENSE).
  No third-party tablebase files are distributed.
- **PyTorch, NumPy, python-chess**: external training/verification dependencies,
  installed separately under their respective licences. No framework libraries
  are bundled in the playing package.
- **GitHub Actions**: release packaging uses pinned upstream actions; their source
  and licence remain with their respective projects.
