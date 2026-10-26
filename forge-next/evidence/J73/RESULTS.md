# J73 — new models on the earlier draw concerns

Completed existing-engine comparison: Wide160, Wide192 with four 32-unit readers, and Wide192 with two 64-unit readers. All use identical adopted search/move-ordering/TT source and unchanged defaults. No engine was modified or promoted.

This is a targeted diagnostic, not a general strength match. Raw W/D/L are network predictions. Finite Stockfish analysis is independent evidence, not a proof. A continuation hitting its move cap is unfinished (*), never counted as a draw.

## The sharpest old case: the rook-pawn fortress

Before White 80 the pawn is still on h4 and earlier independent analysis found winning plans. After the played h5 and ...Be4, earlier deep reference analysis supports a fortress. Avoiding h5 once does not establish that a model can convert the earlier position.

| Position / model | Raw W / D / L | 2-second search | 30-second search |
|---|---:|---|---|
| fortress-80w / Wide160 | 95.4% / 4.6% / 0.0% | +7.27 / Kg5 / d48 | +11.07 / Rd1 / d60 |
| fortress-80w / Wide192 / 4×32 | 91.6% / 8.4% / 0.0% | +5.47 / Rd7+ / d46 | +7.22 / Rd7+ / d55 |
| fortress-80w / Reader64 / 2×64 | 92.5% / 7.5% / 0.0% | +5.64 / Rd7+ / d44 | +6.36 / Rd1 / d59 † |
| fortress-81w / Wide160 | 93.8% / 6.2% / 0.0% | +6.52 / Kg5 / d42 | +6.52 / Kg5 / d67 |
| fortress-81w / Wide192 / 4×32 | 86.7% / 13.3% / 0.0% | +5.23 / Rd7+ / d47 | +5.23 / Rd7+ / d61 |
| fortress-81w / Reader64 / 2×64 | 89.6% / 10.4% / 0.0% | +5.91 / Kg5 / d41 | +5.94 / Kg5 / d63 |

## Actual continuations

Each model plays the side that was trying to convert, against Stockfish 19. One second per move for both sides; one thread and 128 MiB hash each. Full prehistory and retained within-game state. No tablebases or evaluation adjudication. A win/loss here is an observed case result at this budget, not a forced-result certificate or an Elo estimate.

| Start | Model | Model result | Added plies | Termination | Replay |
|---|---|---|---:|---|---|
| catalan-80w | Reader64 / 2×64 | **win** | 147 | checkmate | [PGN](continuations/catalan-80w__reader64/game.pgn) |
| catalan-80w | Wide160 | **win** | 95 | checkmate | [PGN](continuations/catalan-80w__wide160/game.pgn) |
| catalan-80w | Wide192 / 4×32 | **win** | 131 | checkmate | [PGN](continuations/catalan-80w__wide192/game.pgn) |
| corpus-1332191-109 | Reader64 / 2×64 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/corpus-1332191-109__reader64/game.pgn) |
| corpus-1332191-109 | Wide160 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/corpus-1332191-109__wide160/game.pgn) |
| corpus-1332191-109 | Wide192 / 4×32 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/corpus-1332191-109__wide192/game.pgn) |
| current-54b | Reader64 / 2×64 | **draw** | 196 | insufficient_material | [PGN](continuations/current-54b__reader64/game.pgn) |
| current-54b | Wide160 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/current-54b__wide160/game.pgn) |
| current-54b | Wide192 / 4×32 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/current-54b__wide192/game.pgn) |
| fortress-80w | Reader64 / 2×64 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-80w__reader64/game.pgn) |
| fortress-80w | Wide160 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-80w__wide160/game.pgn) |
| fortress-80w | Wide192 / 4×32 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-80w__wide192/game.pgn) |
| fortress-81w | Reader64 / 2×64 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-81w__reader64/game.pgn) |
| fortress-81w | Wide160 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-81w__wide160/game.pgn) |
| fortress-81w | Wide192 / 4×32 | **unfinished** | 200 | 200_ply_cap | [PGN](continuations/fortress-81w__wide192/game.pgn) |
| rook-bishop-64w | Reader64 / 2×64 | **win** | 41 | checkmate | [PGN](continuations/rook-bishop-64w__reader64/game.pgn) |
| rook-bishop-64w | Wide160 | **win** | 47 | checkmate | [PGN](continuations/rook-bishop-64w__wide160/game.pgn) |
| rook-bishop-64w | Wide192 / 4×32 | **win** | 81 | checkmate | [PGN](continuations/rook-bishop-64w__wide192/game.pgn) |

## Historical search decisions

Numbers are the engine’s displayed pawn-unit score, final selected move, and latest unbounded PV depth; side-to-move perspective. † means final selection differs from the latest unbounded PV first move: that score must not be attributed to the final move. Full scored/bound iterations and PVs remain in records/ and logs/. Stockfish uses a different score calibration.

| Position | Wide160 | Wide192 / 4×32 | Reader64 / 2×64 | Fresh reference, 20s |
|---|---|---|---|---|
| current-46b (time10s) | +0.91 / g4 / d32 † | +1.07 / Bf3 / d31 | +0.88 / Bf3 / d28 | +0.11 / Bf3 / d41 |
| current-54b (time30s) | +0.74 / Bb5 / d46 | +0.84 / Bb5 / d43 | +0.54 / Kd7 / d42 | +0.00 / Bg2 / d54 |
| current-79b (time10s) | +0.82 / Re5 / d34 | +0.81 / Re5 / d32 | +0.71 / Re5 / d32 | +0.00 / Bf1 / d59 |
| current-92b (time10s) | +0.32 / Kf3 / d31 | +0.58 / Kf3 / d32 | +0.35 / Kf3 / d30 | +0.00 / h4 / d51 |
| fortress-80w (time30s) | +11.07 / Rd1 / d60 | +7.22 / Rd7+ / d55 | +6.36 / Rd1 / d59 † | +10.04 / Rd1 / d43 |
| fortress-81w (time30s) | +6.52 / Kg5 / d67 | +5.23 / Rd7+ / d61 | +5.94 / Kg5 / d63 | +0.33 / Rd1 / d79 |
| rook-bishop-64w (time30s) | +317.53 / Rb8+ / d46 | M+36 / Rb8+ / d43 † | M+30 / Rb8+ / d42 | M+19 / Rb8+ / d79 |
| rook-bishop-65w (time30s) | +21.92 / Rh7+ / d48 | +16.02 / Rh7+ / d46 | +2.33 / Rh7+ / d46 | M+25 / Rh7+ / d70 |
| catalan-80w (time30s) | +3.63 / Be5 / d37 | +3.75 / Be5 / d34 | +3.32 / Be5 / d36 | +5.89 / Be5 / d30 |
| scotch-8b (time10s) | +0.00 / Ba3 / d32 | +0.00 / Ba3 / d36 | +0.00 / Ba3 / d35 | +0.00 / Ba3 / d35 |
| scotch-9b (time10s) | +0.05 / Bb4 / d30 | +0.00 / Bc5 / d36 | +0.15 / Bb4 / d28 | -0.02 / Bc5 / d35 |
| scotch-11b (time30s) | +0.00 / Bc5 / d41 | +0.00 / Bc5 / d44 | +0.00 / Bb4 / d32 † | +0.00 / Bc5 / d59 |

### Equal-node comparison

Separate cold searches, two million requested nodes. This removes much of the speed-budget difference, but search paths and selective depth still differ.

| Position | Wide160 | Wide192 / 4×32 | Reader64 / 2×64 |
|---|---|---|---|
| current-46b | +1.13 / Bf3 / d24 | +1.09 / Bf3 / d22 | +0.76 / Bf3 / d22 |
| current-54b | +1.08 / Bb5 / d26 | +0.84 / Bb5 / d24 | +0.54 / Kd5 / d26 |
| current-79b | +1.50 / Kc4 / d23 | +1.23 / Kc4 / d23 | +0.90 / Kc4 / d25 |
| current-92b | +0.73 / Kf3 / d22 | +0.94 / c5 / d22 | +0.70 / Kf1 / d21 |
| fortress-80w | +7.27 / Kg5 / d44 | +5.47 / Rd7+ / d37 | +5.64 / Rd7+ / d41 |
| fortress-81w | +6.72 / Kg5 / d41 | +5.23 / Rd7+ / d38 | +5.91 / Kg5 / d41 |
| rook-bishop-64w | +3.20 / Rb8+ / d27 | +2.74 / Ke6 / d25 | +8.21 / Rb8+ / d33 |
| rook-bishop-65w | +1.88 / Kf6 / d32 | +1.52 / Kf6 / d31 | +2.13 / Kf6 / d28 |
| catalan-80w | +1.99 / Rd8 / d25 | +2.71 / Be5 / d24 | +2.29 / Be5 / d25 |
| scotch-8b | +0.00 / Ba3 / d28 | +0.00 / Ba3 / d28 | +0.00 / Ba3 / d27 |
| scotch-9b | +0.00 / Bc5 / d23 | +0.00 / Bc5 / d27 | +0.30 / Bb4 / d23 |
| scotch-11b | +0.13 / Bb4 / d24 | +0.00 / Bc5 / d32 | +0.00 / Bb4 / d24 † |

### Scotch repetition and retained search state

There was no established forced win in the original Scotch game. Choosing a non-repeating move is not by itself a strength gain. The retained-state replay uses logged completed-iteration node budgets and cannot recreate the GUI’s unlogged partial iterations.

| Position / model | Cold 2s | Cold 2M nodes | Retained replay | Cold without history, 2s |
|---|---|---|---|---|
| scotch-8b / Wide160 | +0.00 / Ba3 / d30 | +0.00 / Ba3 / d28 | +0.00 / Ba3 / d29 | +0.00 / Ba3 / d30 |
| scotch-8b / Wide192 / 4×32 | +0.00 / Ba3 / d29 | +0.00 / Ba3 / d28 | +0.00 / Ba3 / d36 | +0.00 / Ba3 / d29 |
| scotch-8b / Reader64 / 2×64 | +0.00 / Ba3 / d29 | +0.00 / Ba3 / d27 | +0.00 / Ba3 / d28 | +0.00 / Ba3 / d29 |
| scotch-9b / Wide160 | +0.00 / Bc5 / d26 | +0.00 / Bc5 / d23 | +0.08 / Bb4 / d26 | +0.00 / Bc5 / d26 |
| scotch-9b / Wide192 / 4×32 | +0.00 / Bc5 / d29 | +0.00 / Bc5 / d27 | +0.00 / Bc5 / d36 | +0.00 / Bc5 / d29 |
| scotch-9b / Reader64 / 2×64 | +0.30 / Bb4 / d23 | +0.30 / Bb4 / d23 | +0.10 / Bb4 / d28 | +0.30 / Bb4 / d23 |
| scotch-11b / Wide160 | +0.13 / Bb4 / d24 | +0.13 / Bb4 / d24 | +0.00 / Bc5 / d37 → current_threefold | +0.14 / Bb4 / d24 |
| scotch-11b / Wide192 / 4×32 | +0.00 / Bc5 / d34 → current_threefold | +0.00 / Bc5 / d32 → current_threefold | +0.00 / Bc5 / d39 → current_threefold | +0.00 / Bc5 / d29 |
| scotch-11b / Reader64 / 2×64 | +0.00 / Bb4 / d25 | +0.00 / Bb4 / d24 † | +0.00 / Bc5 / d29 → current_threefold | +0.00 / Bc5 / d25 |
| current-54b / Wide160 | +0.74 / Bb5 / d29 | +1.08 / Bb5 / d26 | +0.85 / Bb5 / d37 | +0.74 / Bb5 / d29 |
| current-54b / Wide192 / 4×32 | +0.84 / Bb5 / d25 | +0.84 / Bb5 / d24 | +0.85 / Kd5 / d31 | +0.84 / Bb5 / d25 |
| current-54b / Reader64 / 2×64 | +0.54 / Kd5 / d29 | +0.54 / Kd5 / d26 | +0.56 / Kd7 / d32 | +0.54 / Kd5 / d29 |
| current-79b / Wide160 | +0.77 / Kc4 / d24 | +1.50 / Kc4 / d23 | +0.82 / Re5 / d26 | +1.06 / Kc4 / d25 † |
| current-79b / Wide192 / 4×32 | +0.99 / Bf1 / d24 † | +1.23 / Kc4 / d23 | +0.85 / Kc4 / d26 † | +0.99 / Bf1 / d24 † |
| current-79b / Reader64 / 2×64 | +0.92 / Kc4 / d27 | +0.90 / Kc4 / d25 | +0.76 / Kc4 / d25 † | +0.92 / Kc4 / d27 |

Raw hidden activations, logits and policy outputs matched cold-state outputs at every matched retained-state focus root. This checks neural cache/state consistency in these cases; it does not certify every transposition bound or pruning decision.

## The existing 100-game corpus panel

One position from each of 100 independent source games; selected earlier for errors/material families and ordinary controls. No new data were mined or trained on. These validation examples have already been examined and are not a fresh unseen test set. Errors below mean disagreement with saved teacher expected score, in percentage points. Searched scores are inverted through FORGE’s scalar formula only as a comparison proxy; they do not recover searched draw mass.

| Model | Raw expected-score MAE | 2s searched proxy MAE | ≥15-point errors after search |
|---|---:|---:|---:|
| Wide160 | 16.840 | 4.590 | 5/100 |
| Wide192 / 4×32 | 17.458 | 4.618 | 7/100 |
| Reader64 / 2×64 | 15.998 | 4.571 | 11/100 |

Paired changes versus Wide160 (negative = less teacher disagreement). 95% bootstrap intervals resample source games; they describe this fixed diagnostic mixture.

| Candidate | Raw change [95% interval] | Searched change [95% interval] | Search improves / worsens by >5 points |
|---|---|---|---|
| Wide192 / 4×32 | +0.618 [-0.343, +1.622] | +0.027 [-0.421, +0.482] | 3 / 3 |
| Reader64 / 2×64 | -0.842 [-2.072, +0.371] | -0.019 [-0.603, +0.583] | 4 / 4 |

### Previously identified corpus draw witnesses

The reference column reuses the saved J66/J65 analysis, with its original budget. Both witnesses were selected before the new-model results.

| Case | Model | Raw W / D / L | Fresh 2s search | Retained reference |
|---|---|---|---|---|
| corpus-1332191-109 | Wide160 | 25.8% / 73.4% / 0.8% | +2.96 / f5 / d42 | +0.00, 12s |
| corpus-1332191-109 | Wide192 / 4×32 | 41.2% / 58.4% / 0.4% | +3.43 / f5 / d44 | +0.00, 12s |
| corpus-1332191-109 | Reader64 / 2×64 | 27.2% / 72.2% / 0.6% | +1.47 / f5 / d41 | +0.00, 12s |
| corpus-1494057-118 | Wide160 | 73.4% / 26.3% / 0.3% | +0.74 / Ra5+ / d23 | +0.00, 12s |
| corpus-1494057-118 | Wide192 / 4×32 | 66.0% / 33.3% / 0.7% | +0.75 / Ra5+ / d24 | +0.00, 12s |
| corpus-1494057-118 | Reader64 / 2×64 | 82.4% / 17.3% / 0.3% | +1.72 / Ra5+ / d23 | +0.00, 12s |

### Strata and individual regressions

| Stratum | Games | Wide160 raw / searched | Wide192 raw / searched | Reader64 raw / searched |
|---|---:|---:|---:|---:|
| j65_category_random | 24 | 10.59 / 4.98 | 10.69 / 5.33 | 9.67 / 4.99 |
| j65_large_student_teacher_error | 8 | 54.22 / 6.91 | 53.57 / 5.11 | 52.00 / 6.46 |
| quiet_disagreement | 24 | 18.81 / 5.80 | 19.91 / 5.63 | 19.32 / 6.06 |
| target_family_control | 20 | 14.94 / 1.35 | 15.14 / 1.41 | 11.54 / 1.06 |
| ordinary_control | 24 | 10.24 / 4.91 | 11.66 / 5.40 | 10.72 / 4.97 |

Largest searched regressions for Wide192 / 4×32:

| Case | Wide160 error | Candidate error | Change |
|---|---:|---:|---:|
| corpus-1421554-75 | 14.70 | 23.36 | +8.65 |
| corpus-717448-44 | 27.50 | 34.87 | +7.36 |
| corpus-1373453-120 | 4.61 | 10.05 | +5.44 |
| corpus-849183-143 | 3.72 | 8.13 | +4.40 |
| corpus-1899565-29 | 14.89 | 18.29 | +3.40 |

Largest searched regressions for Reader64 / 2×64:

| Case | Wide160 error | Candidate error | Change |
|---|---:|---:|---:|
| corpus-1494057-118 | 8.47 | 20.20 | +11.73 |
| corpus-1421554-75 | 14.70 | 23.86 | +9.15 |
| corpus-717448-44 | 27.50 | 35.87 | +8.37 |
| corpus-1701705-75 | 10.31 | 17.74 | +7.43 |
| corpus-1370731-154 | 9.41 | 13.93 | +4.52 |

## Limits

- All histories, clocks, model identities and legal PVs were checked. Actual terminal claims take precedence over raw predictions.
- Successful conversion in one rollout is useful evidence; failure at one second per move does not prove no longer search could convert.
- Changing model capacity, training exposure and training outcomes together prevents attributing every difference solely to width or head count.
- Fixed evaluation predictions and searched move quality are separate measurements. A lower scalar is not automatically a better move.
- Full reference estimates, forced alternatives, node/time/depth records and logs remain available. No tuning or repairs followed these results.

## Audit

`{'status': 'passed', 'searches': 590, 'legal_pv_plies': 2848990, 'raw': 369, 'games': 18, 'game_plies': 2938, 'censored': 11}`

See [summary.json](summary.json), [protocol](job-source/PROTOCOL.md), and [completion receipt](completion.json).
