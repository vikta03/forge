# J65 corpus audit

Full retained-cache census; bounded original-record and native-model checks. No training, games, GPU, builds, downloads, tablebases or engine changes.

## Coverage after training filters

Counts are retained examples, not globally unique boards. Categories overlap. Material categories describe geometry only; they do not certify a fortress, a conversion difficulty or a chess result. The reserved test split is untouched.

| Category | Training examples | Training games | Validation examples | Validation games | Teacher mean draw, train |
|---|---:|---:|---:|---:|---:|
| all | 213,899,602 | 2,058,958 | 2,106,816 | 20,944 | 63.9% |
| queenless_le16 | 69,144,035 | 1,281,780 | 720,178 | 13,191 | 70.4% |
| seven_or_fewer | 29,375,930 | 406,178 | 298,778 | 4,189 | 63.4% |
| pawn_only | 1,788,927 | 76,802 | 18,275 | 781 | 71.3% |
| rook_only | 18,131,388 | 383,801 | 189,603 | 3,911 | 82.9% |
| bishop_same | 1,275,951 | 34,431 | 14,557 | 382 | 76.8% |
| bishop_opposite | 2,740,861 | 45,724 | 28,739 | 463 | 89.8% |
| rook_bishop_same | 2,646,346 | 122,132 | 29,709 | 1,315 | 72.4% |
| rook_bishop_opposite | 3,429,951 | 104,307 | 35,987 | 1,095 | 75.8% |
| rook_pawn_vs_bishop | 47,820 | 3,628 | 520 | 46 | 12.3% |
| rook_rookpawn_vs_bishop | 18,263 | 1,289 | 225 | 18 | 11.7% |
| rook_bishop_vs_rook_pawnless | 253,127 | 9,057 | 2,473 | 103 | 98.9% |
| rook_bishop_vs_rook_with_pawns | 1,508,514 | 169,405 | 16,534 | 1,772 | 53.6% |
| long_quiet_queenless | 6,079,603 | 151,464 | 63,032 | 1,604 | 84.9% |

## Student agreement with existing labels

One uniformly selected eligible frame from each sampled game/category. Up to 128 games per group. This estimates an equal-game average, not the position-weighted training loss. Intervals bootstrap games within each group; no independent-position inflation. Very small groups remain weak evidence. Validation excluded gradient updates but selected checkpoints; it is not a pristine final test.

| Category | Validation games | Expected-score MAE, percentage points (95% interval) | Training MAE | Validation draw-probability MAE |
|---|---:|---:|---:|---:|
| all | 128 | 9.83 [8.12, 11.83] | 8.91 | 16.02 |
| queenless_le16 | 128 | 11.16 [8.94, 13.58] | 10.18 | 21.59 |
| seven_or_fewer | 128 | 6.56 [4.75, 8.61] | 7.89 | 13.12 |
| pawn_only | 128 | 8.46 [6.33, 10.84] | 9.59 | 17.03 |
| rook_only | 128 | 8.81 [7.08, 10.79] | 8.57 | 18.70 |
| bishop_same | 128 | 11.91 [9.70, 14.26] | 11.17 | 21.97 |
| bishop_opposite | 128 | 7.45 [5.84, 9.30] | 11.34 | 15.82 |
| rook_bishop_same | 128 | 9.60 [7.94, 11.52] | 11.43 | 20.19 |
| rook_bishop_opposite | 128 | 9.58 [7.68, 11.60] | 8.29 | 19.00 |
| rook_pawn_vs_bishop | 46 | 14.84 [10.20, 20.01] | 14.13 | 29.94 |
| rook_rookpawn_vs_bishop | 18 | 16.46 [9.62, 23.49] | 15.15 | 33.33 |
| rook_bishop_vs_rook_pawnless | 103 | 5.80 [4.17, 7.81] | 5.04 | 11.66 |
| rook_bishop_vs_rook_with_pawns | 128 | 18.86 [16.36, 21.31] | 21.08 | 34.23 |
| long_quiet_queenless | 128 | 2.15 [1.15, 3.37] | 2.60 | 4.00 |

## Original-record inspection

Sampled 384 original games by uniform game-ID rejection sampling. This sample checks filters, label provenance and termination flags. It does not provide a full raw-corpus census of filtered endings. Check, nonstandard-castling and rule-50 records not encoded by the production packer are listed separately; category filter counts apply only to encoded static-evaluation records.

A confident teacher label followed by a played draw is a lead, not a demonstrated label error. Players can lose a win; capped results and adjudicated results require separate interpretation.

| Category | Encoded source frames | Eligible before deduplication | Repetition exclusions | Invalid-label exclusions | Played-draw / low-teacher-draw leads |
|---|---:|---:|---:|---:|---:|
| all | 40889 | 40494 | 395 | 0 | 53 |
| queenless_le16 | 12414 | 12237 | 177 | 0 | 0 |
| seven_or_fewer | 5096 | 5045 | 51 | 0 | 53 |
| pawn_only | 198 | 198 | 0 | 0 | 0 |
| rook_only | 3114 | 3078 | 36 | 0 | 0 |
| bishop_same | 152 | 145 | 7 | 0 | 0 |
| bishop_opposite | 52 | 52 | 0 | 0 | 0 |
| rook_bishop_same | 378 | 365 | 13 | 0 | 0 |
| rook_bishop_opposite | 491 | 489 | 2 | 0 | 0 |
| rook_pawn_vs_bishop | 3 | 3 | 0 | 0 | 0 |
| rook_rookpawn_vs_bishop | 3 | 3 | 0 | 0 | 0 |
| rook_bishop_vs_rook_pawnless | 102 | 101 | 1 | 0 | 0 |
| rook_bishop_vs_rook_with_pawns | 220 | 219 | 1 | 0 | 0 |
| long_quiet_queenless | 931 | 920 | 11 | 0 | 0 |

Source exclusions before material classification: in_check: 2,768, nonstandard_castling: 242, rule50: 0.

Termination flags among eligible source frames (capped and adjudicated may overlap):

| Category | Capped frames | Adjudicated frames |
|---|---:|---:|
| all | 0 | 26,238 |
| queenless_le16 | 0 | 5,664 |
| seven_or_fewer | 0 | 257 |
| pawn_only | 0 | 33 |
| rook_only | 0 | 1,068 |
| bishop_same | 0 | 76 |
| bishop_opposite | 0 | 52 |
| rook_bishop_same | 0 | 258 |
| rook_bishop_opposite | 0 | 338 |
| rook_pawn_vs_bishop | 0 | 0 |
| rook_rookpawn_vs_bishop | 0 | 0 |
| rook_bishop_vs_rook_pawnless | 0 | 0 |
| rook_bishop_vs_rook_with_pawns | 0 | 147 |
| long_quiet_queenless | 0 | 26 |

## Independent reference sample

Up to 24 category-random cases and eight cases selected for large student/teacher disagreement, with distinct games. These strata must not be pooled into an estimated teacher error rate. Whole recorded prefixes are independently replayed; version-5 or unknown-prefix cases are retained as exclusions rather than silently searched without history. Selections, exclusions and deeper-search allocation were frozen before reference analysis. Stockfish WDL has its own calibration; these finite results are comparison evidence, not automatic replacement targets.

| Game/frame | Category / selection | Lc0 teacher W/D/L % | Wide160 W/D/L % | Reference PV estimate / depth / budget |
|---|---|---|---|---|
| 1701705/75 | bishop_same / category_random | 95.4/4.0/0.5 | 68.2/29.3/2.5 | Bxh4 +6.39, d29, 4s |
| 236777/61 | bishop_same / category_random | 81.1/17.9/1.0 | 17.7/79.8/2.5 | Bxd5 +5.44, d25, 4s |
| 1748890/120 | bishop_same / category_random | 0.7/99.0/0.3 | 1.3/95.3/3.3 | Ke2 +0.00, d71, 4s |
| 1335839/137 | bishop_opposite / category_random | 0.3/99.5/0.3 | 1.0/91.7/7.3 | Ka7 +0.00, d86, 4s |
| 1950223/195 | bishop_opposite / category_random | 0.0/100.0/0.0 | 0.1/99.9/0.0 | Kd7 +0.00, d95, 15s |
| 1332191/109 | bishop_opposite / category_random | 4.9/94.9/0.2 | 25.8/73.4/0.8 | f5 +0.00, d78, 4s |
| 1421554/75 | rook_bishop_same / category_random | 0.4/1.3/98.3 | 2.1/35.0/62.9 | g5 -5.65, d27, 4s |
| 184732/79 | rook_bishop_same / category_random | 2.6/56.1/41.2 | 2.9/48.9/48.1 | Rc7 -0.79, d32, 15s |
| 1949834/74 | rook_bishop_same / category_random | 53.1/45.4/1.4 | 22.4/72.2/5.4 | Re6 +1.76, d26, 4s |
| 1613106/101 | rook_bishop_opposite / category_random | 2.2/97.4/0.4 | 10.6/88.8/0.5 | a3 +0.00, d37, 4s |
| 2097844/61 | rook_bishop_opposite / category_random | 63.5/34.5/2.0 | 18.3/79.6/2.1 | h6 +1.25, d28, 4s |
| 1720229/68 | rook_bishop_opposite / category_random | 2.2/96.7/1.1 | 4.8/89.4/5.8 | Rc1 +0.00, d71, 15s |
| 1523703/216 | rook_rookpawn_vs_bishop / category_random | 100.0/0.0/0.0 | 100.0/0.0/0.0 | Kxg6 M5, d245, 4s |
| 761265/138 | rook_rookpawn_vs_bishop / category_random | 100.0/0.0/0.0 | 38.0/62.0/0.0 | Rxa1 M5, d245, 4s |
| 2036369/274 | rook_rookpawn_vs_bishop / category_random | 0.0/100.0/0.0 | 0.1/7.8/92.2 | Bxg1 +0.00, d79, 4s |
| 568369/186 | rook_bishop_vs_rook_pawnless / category_random | 0.0/100.0/0.0 | 0.2/67.5/32.3 | Rxd5+ +0.00, d72, 4s |
| 364233/225 | rook_bishop_vs_rook_pawnless / category_random | 0.0/100.0/0.0 | 3.7/96.3/0.0 | Kd7 +0.09, d30, 4s |
| 1176670/264 | rook_bishop_vs_rook_pawnless / category_random | 0.0/99.9/0.1 | 0.1/96.8/3.1 | Rb1 +0.00, d63, 4s |
| 154045/80 | rook_bishop_vs_rook_with_pawns / category_random | 5.8/90.8/3.4 | 23.6/69.7/6.7 | Kf2 +0.00, d51, 4s |
| 514340/135 | rook_bishop_vs_rook_with_pawns / category_random | 2.4/97.4/0.2 | 8.1/91.7/0.1 | Bb2 +0.00, d36, 4s |
| 1328585/62 | rook_bishop_vs_rook_with_pawns / category_random | 3.2/95.4/1.4 | 12.8/72.3/14.9 | Kxf3 +0.00, d53, 15s |
| 2019343/77 | rook_only / category_random | 1.2/98.5/0.3 | 20.5/78.3/1.1 | Kg7 +0.00, d39, 4s |
| 490191/65 | rook_only / category_random | 2.3/96.6/1.1 | 1.7/94.7/3.6 | Rxe4 +0.00, d38, 4s |
| 1630699/67 | rook_only / category_random | 5.3/93.5/1.1 | 20.1/76.4/3.4 | b6 +0.00, d38, 4s |
| 22331/97 | rook_bishop_opposite / large_student_teacher_error | 92.8/6.3/0.9 | 1.8/52.0/46.2 | Kxg6 +3.38, d35, 15s |
| 464857/87 | rook_bishop_vs_rook_with_pawns / large_student_teacher_error | 85.3/12.5/2.2 | 7.4/55.9/36.6 | fxe4 +2.40, d35, 4s |
| 1153474/119 | rook_bishop_vs_rook_with_pawns / large_student_teacher_error | 99.2/0.8/0.1 | 9.5/70.3/20.1 | Rxe6 +8.65, d36, 15s |
| 2006056/84 | rook_bishop_vs_rook_with_pawns / large_student_teacher_error | 97.5/2.2/0.3 | 15.9/56.6/27.5 | gxf5 +7.03, d41, 15s |
| 2078836/81 | rook_bishop_vs_rook_with_pawns / large_student_teacher_error | 57.5/41.5/1.0 | 3.6/43.9/52.5 | Rxe4+ +3.23, d24, 4s |
| 1245598/60 | rook_bishop_opposite / large_student_teacher_error | 99.8/0.1/0.1 | 3.8/92.1/4.1 | Rxd1 +7.11, d27, 4s |
| 1203076/138 | bishop_same / large_student_teacher_error | 0.2/4.3/95.5 | 7.6/88.2/4.2 | gxh4 -7.78, d32, 4s |
| 1279645/149 | rook_bishop_vs_rook_with_pawns / large_student_teacher_error | 98.1/1.9/0.0 | 1.8/97.9/0.3 | Kxc3 +9.21, d42, 15s |

## Decision boundary

Rare coverage, inaccurate supervision and student approximation are different hypotheses. This audit can measure material-family exposure and teacher imitation, and expose selected reference disagreements. It cannot alone certify general conversion skill, the prevalence of fortresses, exact teacher error rates or that retraining will help. No retraining, architecture change or search fix is authorized or performed by this job. Assess the saved cases before choosing a next job.
