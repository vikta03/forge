# J66 — which evaluation disagreements survive search?

Prepared 25 October 2026. **Awaiting specific hardware approval.** No engines,
builds, corpus decompression or training run during preparation.

## Decision this job supports

J65 found substantial endgame supervision and imperfect raw imitation. All
eight error-selected reference examples began with a capture. A raw model
mistake that ordinary search resolves is different from a persistent conversion
blind spot. This diagnostic separates those cases before choosing training data.

It does not establish Elo, certify teacher labels, prove all endings drawn, or
authorize training. There is no architecture or playing-search change.

## Frozen engine and data

Use the standing Wide160 daily driver, with its exact model, J47/J48 search,
QuietPolicy enabled and other search defaults. Hash64, one thread, MultiPV1,
ponder off. Its original package remains unchanged.

- Engine: `workspace/engines/forge-wide160/bin/forge2-engine`
- Engine SHA256: `a9e6cc0036f0a2a82eda5b61fabb9337bafbe4b9fe351d161518c2e01de09da6`
- Model: `workspace/engines/forge-wide160/models/capacity.fgc`
- Model SHA256: `23e167e99d1a14cbde73d838e295cecf50e0f81d74f6639af6584de6934ae2a6`
- Independent reference: the same pinned SF19 executable and NNUE as J65;
  one thread, Hash128, WDL output, no tablebases.
- Input evidence: J65 assessment (archived local reference),
  its saved model sample, original-game contexts and reference outputs.
- Historical witnesses: J64 assessment (archived local reference).

Source and dependencies are hashed before execution and rechecked afterwards.
The isolated diagnostic source is copied from the installed engine's verified
source receipt. `diagnostic.diff` must change exactly three source files: a
declaration, one UCI dispatch, and a wrapper calling existing quiescence.
The quiescence template, normal search and evaluator are unchanged.

## Position selection — fixed before engine work

Exactly **100 different validation source games**, one position per game:

| Stratum | Games | Selection |
|---|---:|---|
| Retained J65 reference panel | 32 | Its 24 category-random and eight error-selected examples, preserving those labels. |
| Quiet disagreements | 24 | Largest saved raw expected-score disagreements among positions without legal captures/promotions: 12 rook+bishop versus rook with pawns, eight rooks with opposite bishops, four rook+rook-pawn versus bishop. |
| Target-family controls | 20 | Seeded hash ordering of remaining sampled games: 12 rook+bishop versus rook with pawns, eight rook+rook-pawn versus bishop. |
| Ordinary controls | 24 | Seeded hash ordering from J65's all-material validation sample, excluding already used games. |

`selection.json` records every ID and the selection inputs. Quotas must be
available; preparation fails rather than silently changing the panel.
“Quiet” only means no immediate capture/promotion, not absence of tactics.
The panel deliberately overrepresents disagreement and rare endings. It is not
a random sample of the entire corpus; no pooled prevalence or Elo estimate.
Validation has previously selected checkpoints and is not a pristine test set.
The reserved test partition is untouched.

Separately revisit four historical positions from three games: before/after
the rook-pawn fortress mistake, the older rook+bishop conversion, and the
Catalan conversion. These are witnesses, not independent corpus observations.

Reuse the 32 verified J65 histories. Decode at most 68 additional original games
through J65's existing reader. Bounds: 64 MiB compressed reads, 1 GiB total
inflated bytes, at most 64 MiB per game. Verify source hash, encoder features,
teacher labels and every legal prefix move. Chess960 castling representation is
explicit. Known incomplete histories or already claimable/terminal contexts
are retained as exclusions; never substitute a historyless FEN. Invalid hashes,
labels or replay are fatal. No compressed arrays are expanded onto disk.

## Measurements and correctness gates

1. One isolated C++ build, capped at 150 seconds.
2. Direct production-quiescence fixtures for stalemate, checkmate, insufficient
   material, the fifty-move rule and a legal check evasion. On eight existing
   fixtures, require repeatable complete quiescence, identical native evaluation,
   and identical ordinary fixed-node trajectories between installed and probe
   binaries: bestmove, nodes, depths, score/bound and PV. Time/NPS are excluded
   from this equality check. Failure stops the job.
3. For each eligible corpus/witness position, record native logits/WDL, cold
   quiescence and original-binary full search at 250 ms and 2 s. Alternate the
   order of the two full-search budgets. Reset TT and heuristic state each time,
   but preserve the entire actual legal game prefix, including repetition and
   clock state. Verify all returned moves/PVs independently.
4. Require raw values for corpus positions to match saved J65 logits/centipawns.
5. Reuse the 32 saved independent reference baselines (4 s or 15 s, reported
   explicitly). Run 4 s reference searches for the other 68 corpus positions and
   four witnesses, subject to history eligibility.
6. Before inspecting new reference outcomes, freeze up to 12 extra reference
   allocations: four largest 2 s teacher disagreements without immediate
   captures/promotions, then eight largest remaining disagreements. Run fresh
   12 s reference searches. This is diagnostic error selection, not a random
   teacher-error audit. A 12 s follow-up is not necessarily deeper than a retained
   15 s baseline; both receipts stay separate.

The diagnostic `forgeqprobe` starts the unchanged selective quiescence routine
with a full window and cold state. It caps work at 262,144 nodes or 250 ms,
checked at existing 1,024-node checkpoints; the client also has a three-second
deadline. It preserves position/key state. An interrupted result is explicitly
incomplete and cannot enter score comparisons. It is not a playing UCI option
and will not be installed. Production interior calls have varying windows and
accumulated histories, so this wrapper isolates a mechanism rather than copying
every circumstance of gameplay.

## Analysis and interpretation

Raw expected score is `E = W + D/2`. The engine's scalar map is
`cp ≈ 187 log(E / (1-E))`. For scalar quiescence/search estimates, invert it as
`E_proxy = sigmoid(cp / 187)`; this is a common-scale comparison, **not searched
WDL or a calibrated probability**. Scalar scores cannot recover draw mass.
Mate scores map to the corresponding endpoint for this diagnostic only.
Never apply this map to Stockfish; its scale and WDL calibration differ.

Report absolute teacher disagreement for raw, quiescence, 250 ms and 2 s by
selection stratum. Show incomplete counts, native scores, PVs, exact limits and
every case. No silently omitted failed positions and no pooled confidence
interval for this mixed selection. Scores are side-to-move throughout.

Prespecified triage flags, not chess verdicts:

- Raw disagreement ≥15 percentage points, subsequently ≤5 points: closer to
  the teacher after the relevant search stage.
- Disagreement ≥15 points after 2 s: candidate for further chess assessment.
- Other cases, including search moving farther from a teacher, stay visible.

Save the final bestmove independently from score/PV records. Display the latest
unbounded estimate with its own PV; preserve later bound-only output separately.
“Unbounded” is a UCI reporting property, not a proof of the score's chess truth.
Reference agreement narrows explanations; it does not automatically create
replacement training labels.

After the job, review persistent cases against their actual continuations.
If search already resolves most raw errors, do not claim those cases need a
new architecture. If quiet disagreements survive and independent analysis
supports the teacher, they become leads for game-separated corpus mining and a
later matched-budget learning experiment. That later experiment needs its own
design and approval; no promised strength gain.

## Allowance and failure behaviour

- Expected **10–13 minutes**; maximum **15 minutes** wall time for all work.
- Two physical CPU cores; 4 GiB aggregate host RAM; reduced process priority.
- CPU only: no GPU, training, games, downloads or tablebase use.
- One isolated build; source, installed packages and frozen prior runs preserved.
- Shared hardware lock, charger and STOP guards, process-group cleanup, hard
  systemd memory/CPU/runtime bounds. The outer service permits 30 seconds for
  cleanup; this is not additional compute allowance.
- Stop at any failed gate, exhausted resource limit or charger removal. Retain
  raw outputs, partial receipts and failure reason. No automatic retry or cap
  extension. Require review before a new attempt.
- Run as a durable user service; one desktop completion/failure notification.
  Local resource monitoring is part of the supervisor; no conversational polling.

Only after specific approval:

```bash
bash workspace/research/forge_search_labels_20261025/start_j66.sh --approve-j66
```

Saved status, read once:

```bash
python3 workspace/research/forge_search_labels_20261025/progress.py
```

Create `STOP` in this directory to stop the job. All outputs go to a new
timestamped `runs/` directory; rerunning after any attempt requires review.
