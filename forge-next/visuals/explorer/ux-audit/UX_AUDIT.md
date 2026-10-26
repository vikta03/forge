# FORGE Anatomy: usability audit before repair

The explorer is not yet a satisfactory interactive explanation. Its numerical
verification is useful, but it does not establish that a person can navigate,
select or understand the calculation. The implementation exposes plenty of
data without providing a continuous route through it.

This audit records the existing behaviour before changing it. The executable
page and model have not been modified during the audit.

## Evidence and limits

- Audited HTML SHA256:
  `c104a48d3b19ec209c2ca51bb860b59d130e89d3660b2c73e0fe3e5cf790e886`.
- Read the scene, controls, application state, inspector handlers, CSS, previous
  browser tests and their receipts.
- Reviewed the existing laptop, selected-weight and neuron screenshots. These
  are saved images from the preceding build, not new interactive observations.
- `control_geometry.py` independently reconstructs the camera/field equations.
  Its output is in `control-geometry.json`. It performs arithmetic only, without
  launching a browser, model workload or engine.
- Fresh hands-on browser inspection was blocked: the browser tool disallows
  this `file:` URL. No alternate browser, local HTTP workaround or indirect
  browser execution was used to bypass that restriction.

Below, **confirmed** means the source establishes the behaviour, sometimes
  supplemented by coordinate arithmetic or a saved receipt. **Design judgement**
  identifies an experience problem visible in the existing artifact; it is not
  presented as the result of a newly conducted user study.

Source line numbers refer to the audited v2 files and will change after edits.

## 1. Camera controls prevent precise exploration

### U01 — The orbit axes use conflicting drag conventions — high, confirmed

`scene.js:170` subtracts horizontal motion from theta but adds vertical motion
to phi. For a marker on the near side of the scene, a 40-pixel right drag moves
the marker about 22.5 pixels right, while a 40-pixel downward drag moves it about
25.1 pixels **up**. This is the mixed behaviour the user described as flipped.

Repair criterion: establish one coherent object-grab convention for both axes.
Both mouse and touch need the same predictable response. A preference may
reverse it, but the default must not mix conventions.

### U02 — Zoom ignores what the user points at — high, confirmed

`scene.js:172` uses only wheel delta. It changes distance to the existing orbit
target; it never reads pointer X/Y or the object under the cursor. An example
component drifts roughly **51.6 pixels** away from its initial screen position
under a -240 wheel delta at the audited laptop scene size.

Repair criterion: zoom toward a visible hit, or a stable view plane when there
is no hit. Keep that point under the pointer. Normalize wheel delta modes and
use sensible close limits based on the inspected component, rather than one
two-world-unit minimum for every scale.

### U03 — Selecting a neuron does not establish an orbit centre — high, confirmed

`selectNeuron` (`scene.js:151`) draws a ring and incoming edges, but does not
change the camera target. `onPick` for a neuron (`app.js:73`) also leaves the
old target in place. There is no explicit focus-selected command or double-click
framing action. Chapter cameras provide a few fixed targets instead.

Repair criterion: distinguish **select** from **focus**. A click identifies the
object without an unwanted camera jump; double-click or an obvious Focus
button frames it and makes it the orbit centre. Display the current selection
and focus target, and support returning to the previous frame.

### U04 — Unrelated layout actions can erase a custom camera view — medium, confirmed

`resize()` calls `go(0,false)` whenever the overview chapter is active
(`scene.js:158`). Hiding/showing the inspector changes the canvas dimensions;
the grid transition can trigger this repeatedly. The custom orbit/zoom is lost.
Leaving free flight creates a new target exactly 15 units ahead, regardless of
the component being inspected (`scene.js:167`).

Repair criterion: resize only updates projection and viewport. Intentional
Fit/Overview actions reframe. Exiting flight should restore or explicitly set a
meaningful pivot without silently inventing a different inspection context.

## 2. Visibility and picking disagree

### U05 — The floor hides nearly 40% of the weight field — high, confirmed

The opaque floor is at Y=-1.9 (`scene.js:25`). Weight channels occupy
Y=(channel-63.5)*0.15 (`scene.js:134`), so channels 0–50 are below it. From the
default above-floor universe camera, the floor lies on the line of sight to
their point centres: **39,168 of 98,304**, or 39.84%.

The universe box helper being visible does not make its enclosed points visible.
The claim that all points are present in the geometry is true but insufficient
for a useful presentation.

Repair criterion: use a universe-specific backdrop with no intervening floor,
or move both field and framing consistently above it. Verify the low and high
channel extremes are actually visible and reachable.

### U06 — A click can select a weight the floor hides — high, confirmed

The universe picker raycasts only against the points (`scene.js:176`), ignoring
the opaque floor. The prior `interaction-check.json` reported success after
selecting **channel 0**, precisely one of the hidden channels. That was not a
valid visibility-and-picking test.

Repair criterion: selection must agree with what is drawn. Use a legible marker
and exact identity on hover/selection. Test a known visible point, not merely
whether any raycast yields a result. In dense fields, offer layer isolation and
indexed navigation so the user can disambiguate nearby points.

### U07 — The most important close-up lacks a clear visual selection hierarchy —
medium, design judgement supported by source

The neuron screenshot contains hundreds of similarly bright incoming lines and
multiple overlapping stacks. The selected unit's small ring competes with this
visual density. There is no hover preview, selected-edge inspection or way to
isolate the strongest contributions. The pointer remains a generic grab hand
even over meaningful targets (`style.css`, `scene.js:168–177`).

Repair criterion: make the selected object visually unambiguous, dim unrelated
geometry, and offer incoming/outgoing/top-contribution views. Keep a complete
view available; do not replace exact data with an unexplained sample.

### U08 — Unfold makes the graph less connected — high, confirmed

`unfold()` moves head groups but sets their input, value and query routing paths
to invisible (`scene.js:157`). The user loses the very context that spreading
the stacks should clarify.

Repair criterion: recompute the connection endpoints when heads move. Unfold
should preserve a continuous, readable path from inputs through the chosen
head to its outputs.

## 3. Clicking a component often fails to inspect that component

### U09 — Output picking discards the clicked head and coordinate — high, confirmed

The query/value branch of `_pick` converts every output hit into just chapter5
or chapter6 (`scene.js:177`). It discards both the head number and the instance
index. Those chapters use `result.active` (`app.js:45,51–52`).

Consequently, in a position selecting H3, clicking a value/query unit belonging
to H0 opens H3's generic output explanation. This is misleading even though the
underlying numeric port remains correct.

Repair criterion: every pick preserves an exact entity identity: head, output
kind and coordinate. The inspector and highlighted edges must show that entity.
Counterfactual results must remain clearly separated from the selected engine
head, including when moving between panels.

### U10 — A selected weight has no persistent identity in application state —
high, confirmed

`app.js:75` only writes text to a floating box. It does not retain a selected
weight object, add a visible scene marker, focus the point, or select the same
cell in the ledger. `openMatrix` defaults to row0 and explicitly sets column0
(`app.js:81`); the weight-table buttons do not pass the selected coordinates.

Repair criterion: clicking a weight selects the same weight in the scene,
inspector and tensor table. Show its bucket, piece, square, channel, signed value
and whether it participates in the current board's lookup. A selected stored
weight that is not used by this position must not be presented as a contribution.

### U11 — Channel inspection is transient text, not a maintained selection —
high, confirmed

The accumulator/input handler appends a detail box after rendering
(`app.js:74`). There is no selected-channel state. A later `render()` replaces
the entire panel (`app.js:58`), removing the detail. Changing position therefore
cannot keep the same inspected channel and show how its value changed. There
is also no persistent marker on that channel in the scene.

Repair criterion: selections survive harmless re-renders. A locked channel
updates across positions; its piece-level terms and outgoing hidden-unit
contributions remain accessible.

### U12 — The supposed explanation path stops at each panel — high, confirmed

The piece vector is an uninteractive SVG (`smallVector`, `app.js:28`), channel
detail contains one scalar, contribution rows are plain table cells
(`app.js:78`), and the policy ranking rows are plain divs (`app.js:52`). There
are no links from a vector coordinate to its accumulator, from a contribution
to its source input, or from a ranked move to its 16-term dot product and board
arrow.

Repair criterion: support this complete investigation without losing context:

`piece → perspective / bucket / row → channel sum → packed input → selected
hidden unit → selected value or query coordinate → outcome or move score`.

At each step the user must be able to inspect the actual relevant arithmetic,
move forward or back, and see the same selection in the scene. Arithmetic
contributions must not be described as proven semantic chess explanations.

## 4. Views, selection and position state drift apart

### U13 — Changing position silently drops the inspected head and perspective —
medium, confirmed

`compute()` always overwrites `state.head` with the active head and `state.view`
with the new side to move (`app.js:18`). A user comparing a specific specialist
or colour cannot keep that context. Selected pieces are tracked by square only,
so a moving piece is not followed; an empty square causes a different piece to
be selected automatically.

Repair criterion: make follow-active and inspect-fixed modes explicit. Preserve
a locked colour/head/unit. Track a selected piece through the game's moves or
clearly say it was captured; do not silently replace it.

### U14 — The tour can run against a different game from its stored route —
medium, confirmed

`startTrace()` captures the initial head's geometry and piece-count text
(`scene.js:179`). Changing ply stops it, but the game dropdown's handler calls
`compute()` directly without stopping the trace (`app.js:96`). Unfold can also
change geometry while the trace's route still uses the old coordinates.

Repair criterion: a single position/layout update path coordinates all views.
Pause or rebuild an active demonstration whenever its inputs change. Verify
that its captions, route, active head and numbers refer to the same position.

### U15 — View switches lose camera context and force the inspector back open —
medium, confirmed

Entering the universe resets the layer and uses a preset camera. Returning to
the architecture calls the current chapter preset rather than restoring the
previous view (`app.js:77,97`, `scene.js:138,163`). Every scene pick starts by
calling `showPanel()`, including picks without a useful handler (`app.js:73`).
There is no navigation history or breadcrumb identifying the current object.

Repair criterion: preserve camera and selection per view. Provide a Back path
and a stable breadcrumb. Inspector visibility should be intentional and should
not make an unrelated camera jump.

### U16 — Some interactive-looking controls have no useful action — medium, confirmed

The weight-field label is a button emitting `universe-info`, but `onPick` does
not handle it. Unfold remains available in weight-universe view even though it
only moves the hidden architecture. The mouse Overview button resets the
universe camera, while the Home key switches to architecture chapter0. These
differences are not explained in the controls.

Repair criterion: one named command should have one meaning. Disable or hide
irrelevant actions. A clickable label must have a useful result, or be a label.

## 5. The presentation does not yet support learning at the user's pace

### U17 — The tour advances too quickly to inspect the accompanying explanation —
medium, confirmed timing plus design judgement

Seven stages receive 2.4 seconds each; camera movement can consume 1.1 seconds
of a stage (`scene.js:183,185`). Stop restarts the sequence next time. There is
no pause/resume, previous step, next step or hold-at-current-calculation state.
The side panel is much longer than the time allowed to read it.

Repair criterion: provide a manual step-through as the primary educational
journey. Optional autoplay can animate the same steps. Pausing must preserve
the selected calculation and let the user inspect it before continuing.

### U18 — Decorative overlays and small controls compete with the model —
medium, design judgement supported by screenshots and CSS

The saved laptop view allocates considerable space to the title and explanatory
copy while the model occupies a narrow central band. The weight controls cover
part of the field. Many labels and essential hints are 8–10px, and the specialist
detail sits below introductory cards in a narrow scrolling panel. Scrolling
around to find a number is not the same as inspecting a selected component.

Repair criterion: an exploration mode minimizes the introductory material,
puts the selected object's name/value/actions at the top, and gives the scene
usable space. Controls need readable type, sufficient target sizes and a compact
collapsible treatment. Verify on the actual laptop-sized viewport, not only a
large screenshot.

## Why the previous checks missed this

The tests proved useful but narrower properties:

- An exact number of points existed in a geometry buffer, not that all were
  visible above an opaque floor.
- A raycast returned something, not that it selected the visible object the
  person meant. The saved success selected a hidden channel.
- A camera coordinate changed after W, not that orbit and zoom were predictable
  or accurately controllable.
- A contribution table had 328 rows, not that a person could follow any of them
  to the originating piece and onward to an output.
- A tour reached chapter6, not that its information was readable or its state
  survived user interaction.
- A mobile page had no horizontal overflow, not that its controls were useful.

The 128-position arithmetic verification remains valuable. It is not evidence
that these interaction requirements passed. Presenting the full collection of
checks as sufficient validation of the experience was premature.

## Repair order and acceptance journeys

1. **Camera and visibility:** consistent orbit, cursor-anchored zoom, explicit
   focus, stable resizing, correct floor/picking. Approach and orbit a chosen
   neuron from the overview, return, and repeat for a low-channel weight.
2. **One selection model:** retain exact component identity across scene,
   inspector, ledger and game position. Inspect an inactive head's output and
   verify the panel is about that exact head and coordinate.
3. **Connected arithmetic:** follow the bishop on a6 through one of its actual
   placement channels to a hidden unit and output. Every drill-down needs a
   meaningful next/back action, synchronized highlighting and correct scope.
4. **Readable exploration:** persistent context, uncluttered selected-object
   details, optional graph isolation, manual lesson steps, small-screen review.

Release acceptance must include real mouse/touch navigation and comprehension
tasks, not just DOM queries and state flags. Fresh hands-on verification remains
outstanding because browser access was blocked in this audit. Source-derived
diagnosis should not be relabelled as that verification.
