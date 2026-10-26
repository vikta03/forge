# J69 / J70 — authorized capacity training, ready engine, 400 games

The user explicitly authorized all tests through full training and a finished
engine for a clear capacity upgrade with acceptable depth, and added 400 games
under the established conditions. No further routine approval is needed.

## Revised scope: candidate only

The user cancelled fresh Wide160 training as wasteful. It was stopped after
93seconds, with its partial checkpoint retained and excluded. Resume only
Wide192, comparing against installed Wide160. No extra control or extra
candidate epochs. The reviewed change is detailed in `PLAN_REVISION.md`.
The protocol below records the original design where relevant; this revision
supersedes every fresh-control or two-arm requirement.

## Selection and control (original design, revised above)

J68's frozen speed/depth rule selects one architecture before any new learning.
The installed Wide160 package stays intact. Train that candidate and a fresh
Wide160 control with the same random seed, teacher targets, examples, sample
order, optimizer, batch size, learning-rate schedule and twelve-pass budget.
The architectural variables are recorded in `configuration.json`; no change to
the corrected J47/J48 search, evaluator algebra or policy-search settings.

Capacity candidates: placement192, 48-unit reader, eight 32-unit routed experts,
or eight 48-unit routed experts. Eight-expert routing retains four piece-count
bands and learns a binary choice within each. Only one expert runs in native
inference. Training also uses a hard choice in its forward pass, with a sigmoid
straight-through gradient, temperature1024, and 0.01 balancing penalty on the
active band's mean soft choice. Integer weights/bias, tie-to-first behavior,
and quantized algebra match native inference. Experts start with distinct
random weights. These routing details are hypotheses, not established gains.

## J69 data and learning

Reuse the authenticated compact cache: 213,899,602 training examples and
2,106,816 validation examples. Load eight compressed shards at a time; a loader
prefetches the next group while batches are prepared and copied to GPU. No
expanded corpus on disk. One pass covers every retained training row once;
twelve passes give 2,566,795,224 presentations per arm. No teacher re-analysis,
targeted resampling, tablebases or handcrafted chess scores.

AdamW, effective batch4096, initial LR0.01, minimum0.001, 2M-example warmup,
one high-rate pass then cosine decay, gradient clip1, weight decay1e-6, quiet
policy loss weight0.25. Piece-count boundary supervision overlap0.25 matches
the control. Validate on the full validation set each pass; choose the lowest
WDL cross-entropy checkpoint separately for each arm. Selection-data intervals
are exploratory; compare game-grouped teacher error and ordinary/endgame slices
against both installed Wide160 and the matched fresh control. No validation
metric is itself a strength claim. Record learned expert use and boundary gaps.

Before full training: independent integer/native-layout and GPU forward checks,
new-capacity gradients, deterministic initialization, actual trainer interrupted
checkpoint/resume check, and discarded calibration updates. Calibration is
discarded; both real runs start from their frozen random seed. Errors stop the
pipeline; no silent retries or discarded failures.

Expected remaining training around2–3hours for Wide192; allowance
seven cumulative hours for training work, maximum3h20m per arm, plus fifteen
minutes for gates. Four physical CPU cores,6GiB host RAM,3GiB GPU-process memory
(2.5GiB allocator). GPU visibility/preflight is checked outside the sandbox.
Stop on charger loss, competing GPU workloads, low disk, explicit STOP or caps;
request a checkpoint before forced termination. No paid compute or downloads.

## J70 delivery

Maximum fifteen minutes, two CPU cores,4GiB RAM, no GPU. Build the selected
native architecture with fixed Haswell/AVX2 flags; only the UCI name differs
from the J68 source. Load the trained selected checkpoint, run existing rules
and Chess960 tests, dense/incremental selftests, independent NumPy parity for
value/policy/routing, short legal searches with policy on/off, stop/quit and
stalemate regression, missing/corrupt-model rejection, and relocation into a
path containing spaces. Repeat paired NPS/depth observations on the frozen48
independent positions and two start-position measurements per engine.

Before games require lower paired NPS95% bound at least−10% and median depth
change at least−2 relative to the installed model. This looser trained-model
guard recognizes that new evaluations change the tree; J68 already isolated
the architecture's cost. A failure is investigated, never hidden by an automatic
gate change. Install only a separate `engines/forge-capacity-<architecture>`
package with source, model checksums, launcher and verification provenance.

## J70 match

400 games against the **original installed Wide160**;8seconds per side per
game +0.08seconds per move, concurrency8. Two hundred frozen unique12-ply
human-game openings from two hundred source games, both colours, no engine
filter. Exclude J68 timing games. This is an existing engineering pool and its
training overlap is unknown. Same1thread, Hash64MiB, quiet policy enabled,
10ms move overhead, unchanged search options. Eight distinct physical cores;
both colour games of each opening use the same core. Clear hash/history between
games and transmit complete history on every move. No tablebases, evaluation
adjudication, resignation or result-dependent stopping. Natural/current rule
draws only.600-ply games are censored and stop for review, never assigned draws.
Record all flags in results; >8 clock expiries stop for infrastructure review.

Expected20–30minutes, hard30-minute cap, eight CPU cores and4GiB RAM, no GPU.
Independently replay every PGN/move log and verify clocks/results/pairs. Report
W/D/L, colours, paired-opening bootstrap intervals and bounded-score interval.
No claim of precise Elo or long-time-control strength; no automatic daily-driver
replacement. A failed check or incomplete match is reported as such.

The entire durable pipeline has an eight-hour hard cap. One shared hardware
lock prevents overlap, logs/checkpoints survive the chat, and a completion or
failure notification fires. `progress.py` reads small receipts on demand.
