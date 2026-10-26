# J66 — raw evaluation through search

Generated diagnostic summary; a chess assessment is still required. No strength, forced-result or training-benefit claim follows.

Measured 100 independent corpus games and 4 historical witnesses. Excluded contexts: 0; incomplete bounded quiescence probes: 0.

## What the numbers mean

Raw WDL comes directly from the model. For quiescence and full search, sigmoid(cp / 187) maps the scalar back onto the model’s expected-score scale. This is a comparison proxy, not measured searched WDL: a scalar cannot recover draw probability. Teacher expected score is W + D/2. Every score is from the root side to move. Stockfish has a different calibration; its centipawns are shown separately and never passed through FORGE’s inverse map.

Search is cold at each budget, with original legal game history preserved. Quiescence uses the unchanged production routine with a full window, cold heuristics and a 250 ms / 262,144-node cap. Ordinary interior quiescence has different windows and accumulated state. The 250 ms and 2 s runs use the original installed engine. This panel is not a warm-tree gameplay simulation.

## Descriptive teacher disagreement

These strata have different selection rules. Large-error cases are deliberately oversampled; no pooled population estimate or confidence interval is claimed. “Quiet” means no immediate legal capture or promotion, not a proof of tactical stability. A lower error means closer teacher imitation, not automatically better chess.

| Stratum | Games | Raw MAE (pp) | Q MAE (pp; n) | 250 ms MAE (pp) | 2 s MAE (pp) |
|---|---:|---:|---:|---:|---:|
| j65_category_random | 24 | 10.59 | 7.35; n=24 | 6.25 | 5.56 |
| j65_large_student_teacher_error | 8 | 54.22 | 22.98; n=8 | 6.59 | 6.58 |
| ordinary_control | 24 | 10.24 | 9.70; n=24 | 5.87 | 5.55 |
| quiet_disagreement | 24 | 18.81 | 18.81; n=24 | 8.34 | 6.52 |
| target_family_control | 20 | 14.94 | 3.01; n=20 | 1.33 | 1.60 |

## Triage, not verdicts

Of 44 cases initially at least 15 percentage points from the teacher, 12 are within 5 points after quiescence and 23 after 2 s. These subsets may overlap. 9 corpus cases remain at least 15 points away after 2 s.

Inspect surviving disagreements with the independent reference and actual chess continuations before assigning a cause. Finite reference searches do not certify a teacher label or a forced win/draw. Reference baselines reuse J65’s 4 s / 15 s records where available; other baselines are 4 s. The selected follow-ups are fresh 12 s searches. Budgets are deliberately disclosed rather than presented as a uniform reference test.

## Corpus cases

| Case / category | Teacher E % | Raw E % | Q scalar | 250 ms scalar | 2 s scalar / PV start | 2 s disagreement pp | Reference baseline | Follow-up |
|---|---:|---:|---:|---:|---|---:|---|---|
| corpus-717448-44 / all | 2.32 | 43.51 | -0.49 | -0.77 | -1.31 / exd6 cxd6 Bc3 f5 | 30.85 | -4.29 / 4s; exd6 cxd6 Rc1 f5 | -4.48 / 12s; exd6 cxd6 Rc1 f5 |
| corpus-1332191-109 / bishop_opposite | 52.37 | 62.48 | +2.70 | +2.96 | +2.96 / f5 Ba2 Kg5 fxe4 | 30.59 | +0.00 / 4s; f5 fxe4 fxe4 Ba2 | +0.00 / 12s; f5 fxe4 fxe4 Ba2 |
| corpus-1708569-65 / all | 96.62 | 72.08 | +1.77 | +2.03 | +1.53 / Rh5 Rfd1 Rd7 Rbc1+ | 27.23 | +3.78 / 4s; Rh7 b6 Kb8 Qd2 | +4.72 / 12s; Rh7 Rfc1+ Kb8 Rc5 |
| corpus-680658-103 / rook_bishop_opposite | 90.72 | 54.50 | +0.34 | +1.05 | +1.37 / g5 Rc8 c3 Bb3 | 23.18 | +2.58 / 4s; g5 f3 g4 fxg4 | +3.21 / 12s; g5 Ba4 c3 Ke2 |
| corpus-1494057-118 / rook_bishop_vs_rook_with_pawns | 51.30 | 86.53 | +3.48 | +1.99 | +1.55 / Ra5+ Kb7 Bg3 Rf1+ | 18.31 | +0.00 / 4s; Ra5+ Kb7 Bg3 Ra1 | +0.00 / 12s; Bg3 Rf1+ Ke2 Kb6 |
| corpus-118832-86 / rook_bishop_opposite | 97.44 | 81.80 | +2.81 | +2.09 | +2.63 / Rb7 Rxa2 Bd6 Bf3 | 17.12 | +4.54 / 4s; Rb7 Kg8 Be7 Rxa2 | +5.07 / 12s; Rb7 Kg8 Be7 Rxa2 |
| corpus-1421554-75 / rook_bishop_same | 1.09 | 19.62 | -2.64 | -1.86 | -2.81 / Re7 Ra1 Re6 Ra3 | 17.11 | -5.65 / 4s; g5 Rb4 gxf4 gxf4 | -6.17 / 12s; g5 Rb4 gxf4 gxf4 |
| corpus-1094218-83 / rook_bishop_opposite | 93.31 | 69.87 | +1.57 | +2.15 | +2.20 / Kg5 Bf3 Kf5 Re1 | 16.88 | +3.37 / 4s; Kg5 Bf3 Ra8 Re1 | +3.55 / 12s; Kg5 Bf3 Ra8 Re1 |
| corpus-935521-45 / all | 96.30 | 58.53 | +2.33 | +2.68 | +2.59 / dxc3 Rxd8 Rxd8 Qxc6 | 16.32 | +3.98 / 4s; dxc3 Rxd8 Rxd8 Qxc6 | +5.13 / 12s; dxc3 Rxd8 Rxd8 Qxc6 |
| corpus-327446-97 / rook_bishop_vs_rook_with_pawns | 24.50 | 53.12 | +0.23 | -0.82 | -0.86 / Bf7 Ra8+ Kb7 e8=Q | 14.20 | -3.45 / 4s; Bf7 Rf8 Bg6 e8=R | -4.51 / 12s; Bf7 Rf8 Bg6 e8=Q |
| corpus-364233-225 / rook_bishop_vs_rook_pawnless | 50.00 | 51.83 | +0.14 | +0.98 | +1.07 / Kd7 Kc4 Kd6 Rb1 | 13.93 | +0.09 / 4s; Kd7 Rg1 Bf6 Kc4 | +0.10 / 12s; Kd7 Kc4 Kd6 Re4 |
| corpus-1630699-67 / rook_only | 52.11 | 58.35 | +0.63 | +0.96 | +1.19 / Kd4 Rg1 Kc3 Rg7 | 13.28 | +0.00 / 4s; b6 a5 bxa5 bxa5 | +0.00 / 12s; b6 a5 bxa5 bxa5 |
| corpus-1899565-29 / all | 25.99 | 48.81 | -0.09 | -0.48 | -0.82 / dxc4 bxc4 Nd7 Ne5 | 13.22 | -1.18 / 4s; Nd7 Ke2 Rfd8 Rag1 | — |
| corpus-464857-87 / rook_bishop_vs_rook_with_pawns | 91.53 | 35.41 | +1.18 | +3.01 | +2.41 / fxe4 Ra4 Kf7 Kf1 | 13.13 | +2.40 / 4s; fxe4 Ra4 Kf8 Kf1 | — |
| corpus-22331-97 / rook_bishop_opposite | 95.92 | 27.81 | +1.39 | +2.75 | +2.96 / hxg6 a5 Ra1 Rc3 | 12.96 | +3.38 / 15s; Kxg6 Re2 Ra1 Re6+ | — |
| corpus-1701705-75 / bishop_same | 97.47 | 82.85 | +2.94 | +2.75 | +3.41 / Bxh4 Bf4 Bf6 Bd6+ | 11.37 | +6.39 / 4s; Bxh4 Bf4 f5 d6+ | — |
| corpus-1370731-154 / rook_bishop_vs_rook_with_pawns | 96.74 | 86.43 | +3.46 | +3.68 | +3.32 / Bb7 Rd1 Ba6+ Kc5 | 11.22 | +3.78 / 4s; Kf6 Rd1 Bb7 Rc1 | — |
| corpus-1613806-174 / rook_rookpawn_vs_bishop | 50.00 | 44.33 | -0.03 | -0.15 | -0.79 / Kxh5 Rf8 Bd3+ Kc5 | 10.41 | -0.38 / 4s; Kxh5 Rh8 Kg6 Kc5 | — |
| corpus-943534-70 / all | 47.86 | 50.82 | +0.06 | +0.49 | +0.61 / Rf6 a4 Kf4 Kc8 | 10.22 | -0.17 / 4s; Rf6 Be6 d5 Bxd5 | — |
| corpus-1709284-67 / rook_bishop_opposite | 51.71 | 68.47 | +1.45 | +1.02 | +0.91 / Rd1+ Kf2 g5 Bc5 | 10.22 | +0.08 / 4s; g5 h3 Rd1+ Kh2 | — |
| corpus-380139-68 / rook_bishop_opposite | 52.00 | 65.58 | +1.21 | +1.05 | +0.93 / Bb4 Rf7 Rb8 Bc6 | 10.18 | +0.15 / 4s; Bb4 Rd7 Bc3 e4 | — |
| corpus-2078836-81 / rook_bishop_vs_rook_with_pawns | 78.25 | 25.57 | +2.04 | +2.15 | +1.49 / Rxe4+ Kf3 Rf4+ Kg3 | 9.33 | +3.23 / 4s; Rxe4+ Kf3 Rf4+ Kg3 | — |
| corpus-1245598-60 / rook_bishop_opposite | 99.82 | 49.83 | +0.64 | +3.81 | +4.27 / Rxd1 a4 Rc1 Re8 | 9.07 | +7.11 / 4s; Rxd1 a4 Re1 Rd6 | — |
| corpus-1949834-74 / rook_bishop_same | 75.85 | 58.50 | +0.64 | +1.35 | +1.35 / Re6 Rd8 Be2 Kg6 | 8.54 | +1.76 / 4s; Re6 Rd8 Be2 Kg6 | — |
| corpus-1040359-81 / rook_bishop_vs_rook_with_pawns | 0.83 | 23.33 | -2.23 | -4.16 | -4.36 / Kg7 Be3 Ra6 Rb1 | 8.02 | -5.38 / 4s; Kg7 Rd3 Ra2+ Kf1 | — |
| corpus-1176670-264 / rook_bishop_vs_rook_pawnless | 49.98 | 48.48 | -0.11 | -0.13 | -0.58 / Rf8 Ke6 Rd8 Ke7 | 7.67 | +0.00 / 4s; Rb1 Rxb1+ Kxb1 Kd4 | — |
| corpus-1009513-71 / rook_bishop_vs_rook_with_pawns | 38.61 | 24.75 | -1.49 | -0.43 | -0.30 / dxc5 Rxh6 a5 Re6 | 7.38 | +0.00 / 4s; dxc5 a5 Rg1 Rxh6 | — |
| corpus-1373453-120 / rook_bishop_vs_rook_with_pawns | 2.21 | 25.31 | -2.02 | -2.60 | -4.29 / Re8+ Kd3 Re3+ Kd2 | 6.95 | -4.70 / 4s; Re8+ Kd3 Bc5 Ra6 | — |
| corpus-849183-143 / rook_bishop_vs_rook_with_pawns | 47.21 | 45.77 | +0.01 | -0.87 | -0.72 / Kxd8 Rf6 Rc2+ Kf3 | 6.72 | +0.00 / 4s; Kxd8 Rf6 Rc2+ Kf3 | — |
| corpus-236777-61 / bishop_same | 90.04 | 57.57 | +0.60 | +1.68 | +3.03 / Bxd5 exd5 Kd6 Kf4 | 6.55 | +5.44 / 4s; Bxd5 exd5 Kd6 h4 | — |
| corpus-1892396-102 / rook_bishop_vs_rook_with_pawns | 65.20 | 72.93 | +1.85 | +2.19 | +1.70 / Rb6 Rd7 Bf4 Ra7 | 6.09 | +0.62 / 4s; Rb6 Re4 Bc1 Kf7 | — |
| corpus-2097844-61 / rook_bishop_opposite | 80.79 | 58.07 | +0.61 | +1.93 | +2.04 / Re2 Kg1 Rc2 Re1 | 5.93 | +1.25 / 4s; h6 Re1 Rb8 Re4 | — |
| corpus-297981-98 / rook_bishop_opposite | 77.31 | 63.83 | +1.06 | +1.26 | +1.71 / Rb1 Rc5 Ke2 Bxg3 | 5.92 | +1.63 / 4s; Ke2 Rc6 Rb1 Bxg3 | — |
| corpus-2006056-84 / rook_bishop_vs_rook_with_pawns | 98.57 | 44.17 | +2.31 | +4.20 | +4.97 / gxf5 Rd8 d7 b4 | 5.13 | +7.03 / 15s; gxf5 Rd8 d7 f6 | — |
| corpus-1462647-27 / all | 63.34 | 57.03 | +0.53 | +0.86 | +0.63 / Nxe4 Bxe4 Qd6 Nd2 | 5.00 | +0.73 / 4s; Bf5 Nbc5 Qc8 Qe2 | — |
| corpus-2019343-77 / rook_only | 50.44 | 59.69 | +0.73 | +0.48 | +0.37 / Rb8 Kf2 Rb6 Rc7 | 4.49 | +0.00 / 4s; Kg7 Rc5 Re8+ Kd3 | — |
| corpus-514340-135 / rook_bishop_vs_rook_with_pawns | 51.09 | 53.99 | +0.30 | +0.32 | +0.38 / Bc3 Kg5 Rg2+ Kf5 | 3.97 | +0.00 / 4s; Bb2 Ra8+ Kh7 f7 | — |
| corpus-301323-87 / all | 51.55 | 53.20 | +0.24 | +0.18 | +0.38 / b5 Rb7 R8c5 Nb6 | 3.52 | +0.00 / 4s; Rb8 Nb6 Rc1+ Kg2 | — |
| corpus-146174-46 / rook_bishop_opposite | 53.66 | 68.85 | +1.48 | +0.48 | +0.54 / Kg2 Re8 g5 hxg5 | 3.51 | +0.14 / 4s; Kg2 Re8 a4 Bxb4 | — |
| corpus-1413887-77 / all | 57.95 | 55.05 | +0.68 | +0.94 | +0.34 / Qxf3+ Kxf3 f6 Ke2 | 3.42 | +0.00 / 4s; Qxf3+ Kxf3 f6 Ke4 | — |
| corpus-1277095-16 / all | 42.79 | 55.89 | +0.52 | -0.32 | -0.29 / Bf1 Bg4 f3 Bf5 | 3.34 | -0.68 / 4s; Qe2 c4 Kf1 Nd3 | — |
| corpus-184732-79 / rook_bishop_same | 30.70 | 27.39 | -1.82 | -1.32 | -1.81 / Rc7 Kb2 e5 Rd8+ | 3.17 | -0.79 / 15s; Rc7 Kb2 Re7 Rc8 | — |
| corpus-496661-66 / all | 49.45 | 51.87 | +0.14 | -0.27 | -0.26 / Ke1 Kd6 Ra1 Re8 | 2.92 | +0.00 / 4s; Ke1 Rc8 Ra1 Ra8 | — |
| corpus-643329-101 / rook_bishop_vs_rook_with_pawns | 49.48 | 27.00 | -0.02 | -0.16 | -0.25 / Rxd4 Ke1 Kf6 h6 | 2.82 | +0.00 / 4s; Rxd4 Ke1 Re4+ Kd2 | — |
| corpus-1636661-82 / all | 51.33 | 65.58 | +1.21 | +0.47 | +0.31 / Be1 Kf6 Rf8+ Ke5 | 2.80 | +0.00 / 4s; Bc3 Ra3 Rg7+ Kd8 | — |
| corpus-2033243-73 / rook_bishop_vs_rook_with_pawns | 49.60 | 49.14 | +0.08 | -0.13 | -0.23 / Rxd2 Kf3 Ra2 h4 | 2.67 | +0.00 / 4s; Rxd2 Kf3 Ra2 Ra8+ | — |
| corpus-209634-8 / all | 54.22 | 50.34 | +0.03 | +0.04 | +0.12 / e4 Nxe4 Nxe4 dxe4 | 2.61 | +0.31 / 4s; Ngf3 c5 b3 Be7 | — |
| corpus-490191-65 / rook_only | 50.57 | 49.10 | -0.04 | +0.07 | +0.23 / Rxe4 Rxa7+ Ke6 a3 | 2.50 | +0.00 / 4s; Rxe4 a3 Rg4+ Kf1 | — |
| corpus-314618-25 / all | 53.67 | 51.41 | +0.30 | +0.01 | +0.12 / a5 Rd1 Qb6 Qxb5 | 2.07 | +0.00 / 4s; Nxd4 Bxd4 Ne4 Nc3 | — |
| corpus-1973778-34 / all | 50.70 | 55.70 | +0.43 | +0.03 | -0.10 / Qd3 Bxc3 Rxc3 Re4 | 2.03 | +0.04 / 4s; Qd3 Re8 Red1 Bf8 | — |
| corpus-1065385-13 / all | 52.02 | 55.63 | +0.42 | +0.11 | +0.00 / Kxf8 Nxe5 Na6 Nf3 | 2.02 | +0.12 / 4s; Kxf8 Nxe5 Na6 Nf3 | — |
| corpus-1203076-138 / bishop_same | 2.35 | 51.72 | +0.36 | -4.18 | -9.65 / gxh4 g3 Kd6 g2 | 1.78 | -7.78 / 4s; gxh4 g3 Bf1 d3 | — |
| corpus-909695-19 / all | 51.68 | 57.77 | +0.59 | +0.19 | +0.00 / c1=Q+ Rxc1 Qxa3 Bxb5 | 1.68 | +0.00 / 4s; c1=R+ Rxc1 Qxa3 Bxb5 | — |
| corpus-845032-161 / rook_bishop_vs_rook_with_pawns | 50.64 | 53.06 | +0.23 | +0.20 | +0.17 / Kh7 Rc3 Be6 Kg3 | 1.63 | +0.19 / 4s; Kh7 Rc3 Be6 Rf3 | — |
| corpus-1613106-101 / rook_bishop_opposite | 50.85 | 55.05 | +0.38 | +0.10 | +0.18 / Rxd5 Rxf7 Bc5+ Kf3 | 1.55 | +0.00 / 4s; a3 Bc4 Ra7 Ba2 | — |
| corpus-154045-80 / rook_bishop_vs_rook_with_pawns | 51.18 | 58.43 | +0.64 | +0.35 | +0.00 / Kf2 Kg6 Rg3+ Kf6 | 1.18 | +0.00 / 4s; Kf2 Kg6 Bd6 c4 | — |
| corpus-1244218-14 / all | 49.11 | 46.10 | -0.29 | +0.05 | +0.02 / g4 hxg3 Qxg3 Nbd7 | 1.16 | -0.13 / 4s; Bb2 e5 g4 hxg3 | — |
| corpus-1141305-169 / all | 2.60 | 25.46 | -0.29 | -6.39 | -6.09 / Bxf4 Ke6+ Kf8 Kf6 | 1.11 | -6.41 / 4s; Bb2 Ke6+ Kf8 Ra5 | — |
| corpus-1959108-81 / rook_bishop_vs_rook_with_pawns | 49.02 | 10.30 | -1.00 | +0.00 | +0.00 / exf5 Kf2 Ke6 Ke3 | 0.98 | +0.00 / 4s; exf5 Kf2 Ke6 Ke3 | — |
| corpus-1279645-149 / rook_bishop_vs_rook_with_pawns | 99.04 | 50.79 | +4.07 | +7.82 | +13.16 / Kxc3 Rc8+ Kb2 Rb8+ | 0.87 | +9.21 / 15s; Kxc3 Rc8+ Kb2 Rb8+ | — |
| corpus-1328585-62 / rook_bishop_vs_rook_with_pawns | 50.89 | 48.97 | -0.08 | +0.00 | +0.01 / gxf3 Ke6 Ke4 f5+ | 0.76 | +0.00 / 15s; Kxf3 f5 g3 Kd5 | — |
| corpus-218446-73 / all | 50.68 | 49.89 | -0.01 | +0.19 | +0.00 / Rc5 Ra1 Rb5 Ra4 | 0.68 | +0.00 / 4s; Rc5 Ra1 Bd8 Rhh1 | — |
| corpus-1720229-68 / rook_bishop_opposite | 50.56 | 49.49 | -0.04 | +0.09 | +0.00 / Kb4 Rc2 Rd1 Be6 | 0.56 | +0.00 / 15s; Rc1 Rxc1 Bxc1 Kh7 | — |
| corpus-199826-190 / rook_rookpawn_vs_bishop | 99.43 | 99.85 | +12.11 | +8.07 | +20.36 / Ra1+ Kb6 Ra2 Kc6 | 0.56 | +48.55 / 4s; Ra1+ Kb6 Ra2 Kc6 | — |
| corpus-1267543-140 / rook_bishop_vs_rook_with_pawns | 49.47 | 24.13 | -2.14 | +0.00 | +0.00 / Rf2 Bc4 f6 d4 | 0.53 | +0.00 / 4s; Rf2 Bc4 f6 d4 | — |
| corpus-1214528-169 / rook_bishop_vs_rook_with_pawns | 49.40 | 83.33 | +3.01 | +0.00 | -0.01 / Rd3 Rxg2 Rxd4+ Ke3 | 0.47 | +0.00 / 4s; Rc4 Rxg2 Rxd4+ Ke3 | — |
| corpus-355901-78 / rook_bishop_vs_rook_with_pawns | 50.45 | 39.71 | -0.78 | -0.20 | +0.00 / h4 Rg8 Rd5 Bc3 | 0.45 | +0.00 / 4s; h4 Kc6 Rb3 Kd6 | — |
| corpus-1153474-119 / rook_bishop_vs_rook_with_pawns | 99.56 | 44.71 | +6.99 | +9.18 | +13.65 / Rxe6 Bxb3 Rf6 Ke1 | 0.37 | +8.65 / 15s; Rxe6 Bxb3 Rf6 Bd5+ | — |
| corpus-1228809-150 / rook_bishop_vs_rook_with_pawns | 48.70 | 44.50 | -0.41 | -0.34 | -0.07 / Rf7+ Kxg4 Rf6 Bb7 | 0.36 | +0.00 / 4s; Rf7+ Kg5 Rg7+ Kf6 | — |
| corpus-1289387-98 / rook_bishop_vs_rook_with_pawns | 49.66 | 32.39 | -0.50 | -0.16 | +0.00 / exd4 h3 Kd3 Ke6 | 0.34 | +0.00 / 4s; exd4 Rxd4 Rxb7+ Kg6 | — |
| corpus-1945120-116 / all | 49.79 | 39.07 | -0.83 | -0.17 | -0.04 / Rg5 Re5+ Kd3 Kc7 | 0.32 | +0.00 / 4s; Rg5 Kb7 Rh5 Ra3 | — |
| corpus-1542086-89 / rook_bishop_vs_rook_with_pawns | 49.71 | 18.05 | -2.83 | +0.00 | +0.00 / Rh4 Rxf6 Rxe4+ Kb3 | 0.29 | +0.00 / 4s; Rh4 Kb3 Rxe4 Rxf6 | — |
| corpus-1611418-82 / rook_bishop_opposite | 49.07 | 34.71 | -1.18 | +0.00 | -0.05 / d6 Rd8 Rf6 Bb5 | 0.26 | -0.15 / 4s; Kd2 Bb7 d6 Bc6 | — |
| corpus-230146-65 / all | 54.03 | 52.82 | +0.21 | +0.78 | +0.32 / Rb2 Rd1 Rxc2 Nd4 | 0.24 | +0.00 / 4s; Rb2 a4 Rxc2 a5 | — |
| corpus-1519694-120 / all | 50.22 | 36.07 | -0.16 | +0.00 | +0.00 / gxh5 Rb2 Kf3 gxh5 | 0.22 | +0.00 / 4s; gxh5 Rb2 Kf3 gxh5 | — |
| corpus-1012194-116 / rook_bishop_vs_rook_with_pawns | 0.08 | 15.71 | -3.14 | -9.98 | -10.90 / Rd3 Rc4+ Kb2 Bxf3 | 0.21 | -10.87 / 4s; Re1 g3 Re8 Bc4 | — |
| corpus-1748890-120 / bishop_same | 50.21 | 49.01 | -0.07 | +0.00 | +0.00 / Ke2 Bb4 Bf4 Bc3 | 0.21 | +0.00 / 4s; Ke2 Bb4 Kd3 Be1 | — |
| corpus-1434392-83 / rook_bishop_vs_rook_with_pawns | 49.82 | 36.70 | +0.18 | +0.02 | +0.00 / cxb5 axb5 Rd3 bxa6 | 0.18 | +0.02 / 4s; cxb5 axb5 Rxb4 bxa6 | — |
| corpus-1906574-102 / rook_bishop_vs_rook_with_pawns | 50.11 | 40.69 | +0.17 | +0.07 | +0.02 / Kf3 Rc2 Rxc7 Rxa2 | 0.16 | +0.00 / 4s; Kf3 Rc2 Rxc7 Rxa2 | — |
| corpus-664116-79 / rook_bishop_vs_rook_with_pawns | 49.19 | 33.87 | -0.13 | -0.08 | -0.07 / Kxf8 Rxb4 hxg4 hxg4 | 0.12 | +0.00 / 4s; Kxf8 gxh5 Rh2 Rxb4 | — |
| corpus-141060-75 / all | 49.90 | 49.38 | -0.05 | +0.00 | +0.00 / Rd2 Rxa5 Rxc2 Ra7 | 0.10 | +0.00 / 4s; Rd2 Kg1 Kg7 Be4 | — |
| corpus-1959559-178 / rook_rookpawn_vs_bishop | 99.92 | 68.72 | +1.47 | +10.85 | +15.47 / Rc6 Bb2 h6 Kf5 | 0.06 | +6.76 / 4s; Rc6 Bf4 Rc5+ Kf6 | — |
| corpus-2092972-216 / rook_rookpawn_vs_bishop | 1.74 | 7.60 | -4.67 | -7.50 | -7.48 / Be5 Rf7+ Kg1 h4 | 0.05 | -6.05 / 4s; Be5 Rg5 Bd6 Rg4 | — |
| corpus-1883526-87 / all | 49.97 | 48.21 | -0.13 | +0.00 | +0.00 / a4 Ra7 a3 Bc4 | 0.03 | +0.00 / 4s; g6 g4 Rb2+ Be2 | — |
| corpus-1335839-137 / bishop_opposite | 49.98 | 46.87 | -0.02 | +0.00 | +0.00 / Ka7 Bb5 Bxe3 Ke2 | 0.02 | +0.00 / 4s; Ka7 Bb5 Bxe3 Ke2 | — |
| corpus-1289140-206 / rook_rookpawn_vs_bishop | 49.99 | 27.91 | -0.01 | +0.00 | +0.00 / Bxg7 Kxg7 Kh2 Kf6 | 0.01 | +0.00 / 4s; Bxg7 Kxg7 Kh2 Kg6 | — |
| corpus-1267173-158 / rook_rookpawn_vs_bishop | 99.98 | 70.32 | +1.61 | +11.43 | +15.74 / a5 Kf7 a6 Ke7 | 0.01 | +6.92 / 4s; a5 Kf6 a6 Ke7 | — |
| corpus-918035-96 / rook_bishop_vs_rook_with_pawns | 99.99 | 99.93 | +13.45 | mate 11 | mate 6 / Kxf5 Rxf3+ Kg6 Rxf6+ | 0.01 | mate 6 / 4s; Kxf5 Rxf3+ Kg6 Rxf6+ | — |
| corpus-1950223-195 / bishop_opposite | 50.00 | 50.04 | +0.00 | +0.01 | +0.00 / Kd7 Kf4 Bc2 Ke3 | 0.00 | +0.00 / 15s; Kd7 Bf6 Be2 Bh8 | — |
| corpus-255248-196 / rook_rookpawn_vs_bishop | 100.00 | 99.69 | +13.68 | mate 5 | mate 5 / a8=Q Be5 Re6+ Kc7 | 0.00 | mate 5 / 4s; a8=Q Be5 Rxe5 Kc7 | — |
| corpus-186671-132 / rook_rookpawn_vs_bishop | 100.00 | 98.70 | +11.09 | mate 16 | mate 11 / Rxg1 Kd6 h4 Kc6 | 0.00 | mate 10 / 4s; Rxg1 Ke5 h4 Kd5 | — |
| corpus-1457580-61 / rook_bishop_vs_rook_with_pawns | 50.00 | 40.88 | -0.13 | -0.18 | +0.00 / fxe5 Rxc3 Ra1+ Kg2 | 0.00 | +0.00 / 4s; fxe5 Rxc3 Ra1+ Kg2 | — |
| corpus-122321-128 / rook_rookpawn_vs_bishop | 50.00 | 19.84 | +0.03 | +0.00 | +0.00 / Kxe3 h3 Bf3 Ke5 | 0.00 | +0.00 / 4s; Kxe3 Kg5 Bd1 h3 | — |
| corpus-568369-186 / rook_bishop_vs_rook_pawnless | 50.00 | 33.97 | +0.00 | +0.00 | +0.00 / Rxd5+ Bxd5 | 0.00 | +0.00 / 4s; Rxd5+ Bxd5 Ke7 Kc6 | — |
| corpus-1419637-177 / rook_rookpawn_vs_bishop | 50.00 | 33.94 | +0.00 | +0.00 | +0.00 / Bxg5 h7 Bf6 Kh2 | 0.00 | +0.00 / 4s; Bxg5 h7 Bf6 Kg2 | — |
| corpus-1900467-174 / rook_rookpawn_vs_bishop | 50.00 | 28.79 | +0.11 | +0.00 | +0.00 / Kxf5 h3 Be6 h2 | 0.00 | +0.00 / 4s; Kxf5 Kg7 Bf1 Kf7 | — |
| corpus-2036369-274 / rook_rookpawn_vs_bishop | 50.00 | 3.94 | +0.04 | +0.00 | +0.00 / Bxg1 h2 Bxh2 | 0.00 | +0.00 / 4s; Bxg1 Kf8 Bh2 Ke7 | — |
| corpus-905323-139 / rook_rookpawn_vs_bishop | 50.00 | 4.95 | -0.29 | -0.01 | +0.00 / Bxf5 Kxf5 Kh6 Kg4 | 0.00 | +0.00 / 4s; Bxf5 Kxf5 Kh6 Kg4 | — |
| corpus-1523703-216 / rook_rookpawn_vs_bishop | 100.00 | 100.00 | +18.55 | mate 5 | mate 5 / Kxg6 Ke7 h7 Ke6 | 0.00 | mate 5 / 4s; Kxg6 Ke7 h7 Ke6 | — |
| corpus-761265-138 / rook_rookpawn_vs_bishop | 100.00 | 69.01 | +14.92 | mate 5 | mate 5 / Rxa1 Kg7 Ra6 Kh8 | 0.00 | mate 5 / 4s; Rxa1 Kg7 Ra6 Kh8 | — |

## Historical witnesses — separate from the corpus statistics

| Witness | Raw scalar | Q scalar | 250 ms scalar | 2 s scalar / PV start | Reference |
|---|---:|---:|---:|---|---|
| fortress-80w | +7.00 | +7.00 | +7.31 | +7.27 / Kg5 Kg7 h5 Be4 | +6.48 / 4s; Rd4 Bc2 Rg4 Bd3 |
| fortress-81w | +6.43 | +6.43 | +6.72 | +6.72 / Kg5 Kg7 Rd4 Bb1 | +0.10 / 4s; Rd1 Bc2 Rf1+ Kg8 |
| rook-bishop-65w | +3.27 | +3.27 | +2.37 | +1.88 / Kf6 Rg7 Rb8+ Rg8 | +6.98 / 4s; Rh7+ Kg8 Re7 Kh8 |
| catalan-80w | +1.15 | +1.15 | +0.97 | +1.45 / Rd8 Ra7 Kc3 Kg4 | +5.08 / 4s; Be5 c3 Kxc3 Kg4 |

All displayed search estimates use their own latest unbounded PV. Final bestmoves and any later bound-only records remain separate in the JSON. A UCI unbounded estimate is not a mathematical proof. The four historical positions come from three games and are not four independent population samples.

## Evidence

- [Native outputs and all FORGE iterations](forge.json)
- [Independent reference iterations](reference.json)
- [Exact original histories and exclusion receipts](contexts.json)
- [Correctness and unchanged-search gates](gates.json)
- [Checks and descriptive counts](audit.json)
- [Pre-outcome allocation of reference follow-ups](deeper-selection.json)
- [Bounded source-read receipt](source-resources.json)
