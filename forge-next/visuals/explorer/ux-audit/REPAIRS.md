# Interaction repairs — 23 October 2026

The eighteen audit findings have corresponding implementation changes. The
standalone page and stable entry point are rebuilt from these sources. The
original model and native engine are unchanged.

| Audit | Applied repair |
|---|---|
| U01: conflicting orbit directions | Both drag axes follow the object-grab convention; radius remains fixed. |
| U02: zoom ignores the pointer | Surface-point anchoring scales camera and pivot together. Empty space uses a view plane; wheel units are normalized. Pinch uses the same transform. |
| U03: no component focus | Click selects; double-click, F and Focus selected frame the entity and set its pivot. The pivot is named, and Back restores the earlier frame. |
| U04: lost custom camera | Resize changes projection only. Flight exit restores its starting camera/pivot. Focus can follow a tracked piece or a channel through position updates. |
| U05: floor covers the weight field | The floor, rings and ground grid belong to an environment group hidden in weight view. |
| U06: hidden/ambiguous weight picking | Architecture picking respects opaque mesh occlusion. The field picker filters by screen distance. Hover names the weight; a selection marker and exact row/channel access remove dependence on crowded point picking. |
| U07: unreadable selected connections | Default connection view shows the strongest twelve absolute arithmetic terms. All terms remains available. Other internal edges are subdued; hover and selected-object markers identify targets. |
| U08: unfold removes connections | Input, value and query paths are rebuilt at the unfolded endpoints and remain visible. |
| U09: wrong head’s output | Every output pick carries its head, kind and coordinate. Inspectors use that head and label counterfactuals. The large W/D/L columns retain the engine head. |
| U10: weight identity lost | Selection stores bucket, row and channel. The inspector, marker, indexed controls and ledger share them. Actual usage by each current-board perspective is listed; unused weights are explicitly identified. |
| U11: transient channel text | Channel selection is persistent state. The inspector includes raw sum, truncation/clipping, all piece terms and the corresponding packed input. |
| U12: disconnected explanations | Piece → weight → channel → input → hidden → value/query → move links share the selection mechanism. The calculation trail and Back provide a return route. A move has its full sixteen-term dot product and a board arrow. |
| U13: head/colour/piece changes silently | Explicit follow/lock controls retain the chosen head and colour. Piece identities survive moves; captures show an absent piece instead of a substitute. |
| U14: walkthrough uses stale routes | Walkthrough steps resolve from the current position. Changing game/ply ends it; interacting pauses autoplay. There is no independently cached animation route. |
| U15: views lose selection and camera | Each main view stores its context and camera; history stores prior states. Picks only reopen the inspector when they have a meaningful action. |
| U16: misleading or dead controls | The field label opens indexed inspection; Unfold is disabled there. Home and Overview fit the current view. The tensor ledger distinguishes hover from committed selection. |
| U17: forced fast tour | Manual Next/Previous is the default, with optional ten-second autoplay and Pause. Inspection is possible between steps. |
| U18: overlays dominate / tiny controls | Selected-component mode reduces the title to a small label, expands and prioritizes the inspector, adds readable indexed controls, and places the selected identity above the scene. Mobile rules keep controls explicit and put inspection below the scene. |

## What was checked

`repair-check.json` records 21 passing non-browser regression groups and the
checked source hashes. These include independent camera projection assertions,
geometry/raycast checks, controller state transitions, exact term sums and the
128 existing native-engine comparisons. The native integer comparisons remain
4,480 value-path integers and 3,938 policy logits with no mismatches. The model
SHA256 is unchanged.

The checks also exercise a position-specific failure mode: the bishop selected
after 31.Ba6 is correctly absent after 31…bxa6; selecting the capturing pawn is
a separate action. Returning to the earlier position recovers the bishop.

## Limits of this handoff

Fresh hands-on browser QA is **outstanding**, not marked passed. The browser
tool denied local-file access. There was no localhost, alternate browser, raw
CDP or other workaround. Current claims come from source, arithmetic, geometry
and controller tests. The previous browser receipts and screenshots belong to
the old page and are not reused as approval of this version.

In particular, U07 and U18 involve visual judgment. Their implementations have
changed, but legibility, comfortable navigation and mouse/touch feel still need
to be judged in the rebuilt page. The dense all-weights view remains dense by
design; layer isolation and indexed navigation provide practical alternatives.

## Short acceptance journey in the rebuilt page

1. Refresh the open page. Choose Walk through, then Next at your own pace.
2. Orbit a selected neuron both horizontally and vertically. Aim the pointer
   at an off-centre neighbour and zoom. Double-click it, orbit it, then Back.
3. Hide/show the inspector and resize the window: the camera should stay put.
4. Follow the bishop through a weight and channel into a hidden unit. Use the
   calculation trail to return. Inspect an inactive head’s exact value output.
5. Open the weight field, choose channel zero, read its exact cell and return.
   Both low and high channels should be visible without a ground plane.
6. Unfold a head, switch views, change position, and inspect a quiet move’s
   sixteen products. The selection, labels, camera context and numbers should
   continue to refer to the same thing.

These are manual acceptance steps, not a claim they were performed this time.
