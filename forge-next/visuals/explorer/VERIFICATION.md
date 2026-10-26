# Verification record — interaction repair, 23 October 2026

## Current build

`ux-audit/repair-check.json` records **21 passing non-browser checks**, with hashes
of the actual JavaScript, template and stylesheet. `build-receipt.json` identifies
the generated HTML and copies the checked source hashes. The build refuses a
stale receipt.

The checks cover:

- Unchanged embedded model SHA256 and all 128 saved native comparisons:
  **4,480 value-path integers and 3,938 policy logits** match.
- Both orbit axes, fixed radius, pointer-anchored zoom at several scales, wheel
  delta units, screen-space pan and projection-only resize.
- Exact head and coordinate preservation for all value/query picks.
- All unfolded routing endpoints, floor visibility, per-view camera restoration,
  focus tracking, flight exit and safe move-arrow removal on view switches.
- Ray intersections with low/high channel coordinates in test geometry.
- Piece identities throughout both games, including the bishop’s capture.
- Channel, hidden, value/query and move-term sums against the model outputs.
- Controller transitions: fixed head/colour, explicit follow mode, Back,
  per-view selection, manual walkthrough and its cancellation on a position change.
- Generated inspector strings across several game phases, and constant element
  references against the template and generated markup.

The controller harness substitutes inert value holders and does not render a
DOM. Geometry checks instantiate Three.js mathematical/geometry objects without
a WebGL renderer. These checks therefore do not prove visual layout, hit-target
legibility, timing in a real browser, or comfortable mouse/touch use.

**Fresh interactive browser acceptance was not run.** The browser tool explicitly
rejected the local-file URL. No alternate browser, local server, CDP, or other
route was used to circumvent that restriction. No new screenshots are claimed.

No native-engine job, games, benchmarks or training ran for this repair.

## Historical receipts

`browser-check.json`, `interaction-check.json`, the PNG screenshots and the old
browser verification scripts refer to the earlier HTML, not this repair. They
are retained as historical evidence. Their passing flags are not current UX
approval. In particular, the old point-picking test accepted a channel hidden
behind the opaque floor; it did not test visible-target selection correctly.

`ux-audit/UX_AUDIT.md` records the 18 original findings. `ux-audit/REPAIRS.md` maps
them to changes and identifies what remains to be checked by a person in-browser.
The original verification prose and source snapshots are under `ux-audit/before/`.

## Repeating the lightweight checks

```sh
node verify-interactions.cjs
python3 build.py
```

These require Node and Python, not browser automation. The browser test scripts
are historical development material and should not be run as a workaround for
the access restriction recorded above.
