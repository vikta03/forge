# J65 — where the endgame weakness enters the learning pipeline

Prepared 25 October 2026 for “dive into the corpus and find out”. Source and
metadata preparation is authorized; the bounded hardware allowance below
requires specific approval under AGENTS.md. No training candidate is selected.

## Questions

1. How much relevant material-family coverage actually survived preprocessing?
2. Does deployed Wide160 approximate the recorded teacher labels in those
   families, including examples excluded from gradient training?
3. On a small independently analysed sample, do the teacher and student disagree
   with the reference? Does recorded termination censor the evidence?

These questions do not establish that every material-family example is a
fortress or difficult conversion. A large count can still conceal poor coverage
of a particular geometry. We will not call the two posted games a dataset or
infer a universal architectural defect from them.

## Inputs and scope

- Existing immutable J38 compact cache: **213,899,602 training examples** and
  **2,106,816 validation examples**. Both are a subset of the 249,309,004-frame
  original indexed corpus. Cross-shard training duplicates can remain.
- Read only `ids`, `meta`, `wdl` and `refs` arrays. Skip quiet-policy payloads.
  Verify every consumed array against its saved SHA256; check counts against
  the original producer receipts. Cache data remain compressed on disk.
- Original games under `external-input/lc0-raw`, reached through the
  read-only SQLite manifest and exact game/frame references. Gzip CRC, full
  decompressed game SHA256 and archive identity are checked on consumption.
- Exact original Wide160 model and executable; raw inspection only, no FORGE
  searches. Four saved J64 native witnesses must match exactly before samples.
- Existing Stockfish 19 and pinned NNUE, one thread, Hash128, no tablebases.
- Use existing encoder library and decoder; no recompilation or dependencies
  downloaded. The reserved test partition is not sampled or modified.

## Ordered work

1. **Gates:** frozen source/model/engine checks, synthetic material-family and
   canonical input roundtrip checks, charger/shared lock/STOP checks.
2. **Full retained census:** traverse both retained splits. Count rows and
   distinct source games in the declared categories. Record teacher mean WDL,
   draw >=90% and draw <=10% counts. Categories overlap. No unique-board claim.
3. **Frozen representative samples:** independently for each split/category,
   retain up to 128 games by a deterministic uniform game priority. Within
   each game choose one uniformly sampled retained frame in that category.
   This estimates equal-game averages, not the position-weighted training loss.
4. **Original-record audit:** uniformly select 256 training and 128 validation
   source games by primary-key rejection sampling, without scanning/decompressing
   the whole raw corpus. Examine searched labels, eligibility, repetition flags,
   invalid labels, observed result and adjudication/cap flags. Check/nonstandard-
   castling/rule-50 exclusions are counted separately; category breakdowns here
   are restricted to the boards the production encoder emits. Report this scope.
   A played draw after an optimistic label is a lead, not proof of label error.
5. **Student versus teacher:** run existing native raw inference on the frozen
   samples. Reconstruct each canonical FEN independently and require exact
   cached input equality, respecting the model's halfmove-clock quantization.
   Measure WDL KL, expected-score MAE, draw-probability error and biases.
   Bootstrap games within categories. Validation selected checkpoints, so it
   is not a pristine final test. Categories can share games; do not pretend
   cross-category estimates are independent or combine their sample counts.
6. **Independent reference:** up to 24 category-random validation examples
   (three per each of eight declared families) and eight validation examples
   selected for large teacher/student disagreement. No more than one per game
   across the reference panel. Freeze this selection before reference searches.
   Re-read original frames; require cached features and labels to match. Replay
   recorded version-6 moves from a known initial setup to preserve history and
   the exact clock. Retain exclusions for version 5, unknown prefixes or already
   terminal/claimable states; do not silently substitute a historyless FEN.
   Four seconds per usable case; up to eight preselected cases additionally
   receive a fresh fifteen-second search (four random and four error-selected).
   Preserve every PV, score bound, identity and final bestmove separately.
   Validate all PV moves and reject any tablebase hit.
7. **Report:** full census, representative student errors with game-level
   uncertainty, original-record findings, reference cases and explicit gaps.
   These finite analyses and differently calibrated WDL estimates are not
   automatic training targets. Assess the cases before proposing retraining.

## Declared categories

All positions; queenless positions with at most 16 pieces; at most seven pieces;
pawn-only; pure rook endings with rooks on both sides; one bishop each with
same/opposite colours and no queens/rooks/knights; exactly one rook and bishop
each with same/opposite bishops and no queens/knights; exactly K+R+P versus K+B
with any pawn file and separately a/h pawns; K+R+B versus K+R without pawns and
with pawns; queenless positions with at least 40 halfmoves since a pawn move or
capture. All include kings in piece totals. These are analysis categories only,
never engine evaluation features, hand-written draw rules or labels.

The eight reference families are same/opposite bishop-only, same/opposite
rook-and-bishop, rook+rook-pawn versus bishop, pawnless/pawnful rook+bishop
versus rook, and pure rook endings. Missing cohorts stay missing.

## Allowance and operation

**Expected 10–15 minutes, maximum 20 minutes**, two physical CPU cores and
4 GiB aggregate host RAM. The full-census timing is an estimate, not a measured
promise. No GPU, training, games, builds, downloads, tablebase probes, tuning or
changes to installed packages. One prefetch thread overlaps lossless array
decoding with the census; other native libraries are restricted to one thread.

At most 448 original-game loads, 256 MiB compressed original-game reads and
4 GiB cumulative in-memory inflated source bytes. A single original game is
capped at 64 MiB inflated. Retained cache arrays are separately inventoried;
they are streamed one shard plus one prefetched shard at a time. No raw archive
is expanded on disk. Sampling and witnesses are bounded.

The durable systemd supervisor enforces the allowance, charger and shared
hardware lock, retains partial/failed receipts and notifies once on completion
or failure. Twenty-minute worker cap plus 30 seconds for cleanup. No automatic
retry, silent scope reduction or unapproved time extension. If the full scan
does not finish, report partial counts as partial. No conversational polling.

Preparation (source and small metadata only):

    python3 workspace/research/forge_corpus_audit_20261025/run_j65.py --prepare

After specific hardware approval:

    bash workspace/research/forge_corpus_audit_20261025/start_j65.sh --approve-j65

Manual saved progress:

    python3 workspace/research/forge_corpus_audit_20261025/progress.py

The existing models, original archive, compact cache and published research
remain intact. Whatever this finds, any training proposal needs a separate
dataset specification, control, held-out evaluation and hardware allowance.
