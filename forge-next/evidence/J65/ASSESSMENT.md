# J65 assessment — 25 October 2026

The corpus contains substantial relevant endgame supervision. The strongest
lead from this audit is imperfect student imitation, combined with sparse
coverage of some narrow material families. It does not establish a general
opposite-coloured-bishop defect, widespread bad teacher labels, or a proven
training cure for the user's conversion failures.

J65 completed successfully in **529.39 seconds (8m49s)**, with **652.77 MiB**
peak monitored aggregate RSS. All declared gates passed. It counted the full
retained cache, inspected 3,367 native-model sample records (3,352 distinct
game/frame pairs across overlapping categories), audited 384 original games,
and ran 40 reference searches on 32 additional validation games. No training,
GPU, builds, games, tablebases or engine edits occurred.

## The data are there, but not evenly distributed

Counts below are retained training examples, not globally unique positions.
Games provide a useful second measure of coverage; repeated frames from one
ending do not provide the diversity of independent games.

| Family | Training examples | Source games |
|---|---:|---:|
| At most seven pieces | 29,375,930 | 406,178 |
| One opposite-coloured bishop each, with any pawns | 2,740,861 | 45,724 |
| One rook and opposite-coloured bishop each, with any pawns | 3,429,951 | 104,307 |
| Rook+bishop versus rook, with pawns and no queens/knights | 1,508,514 | 169,405 |
| Exactly king+rook+rook-pawn versus king+bishop | 18,263 | 1,289 |

The last family is only **0.00854%** of the retained training examples—about
one in 11,712. A precise fortress geometry inside it may be much rarer, but this
audit did not count that geometry. Nor does every bishop ending in the larger
categories represent a difficult blockade or conversion. The full counts
disprove general absence, not every possible coverage gap.

## Wide160 has not absorbed all the available signal

The metric is absolute error in **expected score = win probability + half the
draw probability**, compared with the stored Lc0 searched WDL. It measures
teacher imitation, not objective chess truth, Elo or game win rate.

| Validation sample | Games | Mean error, percentage points | 95% game-bootstrap interval |
|---|---:|---:|---:|
| All material | 128 | 9.83 | 8.12–11.83 |
| Opposite bishops without rooks | 128 | 7.45 | 5.84–9.30 |
| Rooks and opposite bishops | 128 | 9.58 | 7.68–11.60 |
| Rook+bishop versus rook, with pawns | 128 | 18.86 | 16.36–21.31 |
| Rook+rook-pawn versus bishop | 18 | 16.46 | 9.62–23.49 |

These are one-frame-per-sampled-game estimates. They differ from full validation
loss, which weights retained positions. Validation also selected checkpoints;
it is not an untouched final test. Categories overlap and their samples cannot
be added as independent observations.

The rook+bishop versus rook-with-pawns group also has **21.08-point** mean error
on its training-game sample. That supports a limitation in fitting the available
signal, rather than a story based solely on unseen validation positions. It does
not distinguish capacity, optimization, quantization, sampling and the inherent
difficulty of approximating searched values with a board-only evaluation.

Opposite-coloured bishops are **not** the worst group in these samples. The
earlier game anecdotes do not establish a bishop-colour-specific model defect.

## Teacher evidence and the tactical qualification

The corpus already contains draw-aware targets. Across retained training rows,
the teacher's mean draw probability is **89.8%** for opposite-bishop-only
endings and **98.9%** for pawnless rook+bishop versus rook. These are average
probabilities, not measured draw-result percentages.

Independent Stockfish analysis supported the teacher's broad result direction
in several student-disagreement cases. For example, game 1279645/frame 149
has teacher W/D/L **98.1/1.9/0.0%**, but raw Wide160 **1.8/97.9/0.3%**;
Stockfish's latest unbounded estimate is **+9.21**, beginning **Kxc3**. The
opposite error also exists: game 2036369/frame 274 has a near-certain teacher
draw but raw Wide160 **92.2% loss**; Stockfish finds **Bxg1**, evaluating 0.00.

Both examples start with captures. In fact **all eight** cases deliberately
selected for large teacher/student errors begin with a capture in the retained
reference PV. Eight of the 24 category-random cases also do. Quiescence/search
can repair such raw mistakes, so these figures must not be presented as eight
new conversion failures or proof of an architectural weakness.

A saved-data-only descriptive split reinforces that qualification. In
rook+bishop versus rook with pawns, 110/128 validation examples permit an
immediate capture or promotion; their mean error is 19.70 points. The other
18 average 13.71 points. In opposite-bishop-only endings, the 88 examples with
neither available average 7.44 points. These post-hoc small subsets have no
new inferential claim; absence of an immediate capture does not prove tactical
stability. No FORGE searches were run by J65 to establish which errors survive.

The reference panel is small, selected and analysed for finite time. Its WDL
calibration differs from Lc0's. It cannot give a teacher-error prevalence or
certify all the labels. It provides no reason to discard the teacher wholesale.

## Original-game records need careful interpretation

Of 40,494 eligible frames in the 384 uniformly sampled source games, 26,238
carry an adjudication flag. These are not all played-to-completion outcomes;
the engine was trained on searched WDL, not automatically on a final result.
The 53 low-teacher-draw / eventual-played-draw leads all come from **one game**,
1753747, and are not 53 independent label failures. They need separate chess
assessment before being called wrong labels.

Only 10.02 MB of compressed original games were read, expanding to 413.02 MB
cumulatively in memory. No raw archives were expanded on disk. The full retained
census read 5.84 GB of selected compressed arrays. The reserved test split was
not used.

## What this changes about targeted learning

The two posted games should remain diagnostic examples. The existing corpus
offers thousands of independent games in even the narrow family, and far more
in the broader families. Any proposed training set can therefore be mined from
many independent source games rather than duplicating the two examples.

The next useful evidence would distinguish persistent evaluation errors at
quiet/search-frontier positions from mistakes already repaired by quiescence.
Retain real wins, real defensive resources and ambiguous cases; inspect useful
teacher disagreements instead of labelling entire material families drawn.
Before training, define the selected population, weights and game-separated
holdouts, and compare against a matched training control and the unchanged
daily driver. A sampling strategy may concentrate gradient effort without
adding runtime operations, but improved playing strength remains unproved.

J65 has not selected or launched training, changed the model, or diagnosed a
new search bug. It has narrowed the data question substantially.

## Evidence

- [Frozen generated report](RESULTS.md)
- [Full retained coverage](coverage.json)
- [Sample metrics](metrics.json)
- [Original-game inspection](source-audit.json)
- [Reference cases and full prefixes](reference-contexts.json)
- [Saved reference search records](reference.json)
- [Read-only post-run review](review/REVIEW.json)

The post-run review independently checked native logits against the raw log,
recomputed probabilities/errors, replayed 32 original game prefixes and 1,034
moves in the retained reference PVs, and checked score bounds and zero tablebase
hits. Original reports and run source remain intact.
