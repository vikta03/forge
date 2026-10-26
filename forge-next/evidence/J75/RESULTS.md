# J75 — policy guidance and defensive search

Observation-only diagnostic. No change to search, weights or installed engines.
Scores are for the side to move at each context. These are fixed-node diagnostics, not speed or strength tests.
Reference candidates come from earlier finite analysis; they are not certified unique defences.
Trace counts below use the exact full move prefix, exclude null-move paths and singular exclusion tests,
and count repeated search visits descriptively, not as independent samples. Missing records in a truncated trace are inconclusive.

| Model / context | Reference move | Raw policy rank | Raw eval | Unrestricted | Forced reference | Reference picked / examined / directly pruned |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| wide160 / fortress-80w@0 | Rd4 | 6 | +7.00 | +7.96 | +8.47 | 57 / 57 / 0 |
| wide160 / fortress-80w@1 | Bc2 | 2 | -5.51 | -7.45 | -6.65 | 2 / 2 / 0 |
| wide160 / fortress-80w@3 | Bd3 | 1 | -5.57 | -8.00 | -7.58 | 0 / 0 / 0 |
| wide160 / fortress-81w@0 | Rd1 | 7 | +6.43 | +6.72 | +7.47 | 47 / 47 / 0 |
| wide160 / fortress-81w@1 | Bc2 | 4 | -5.02 | -7.43 | -6.75 | 1 / 0 / 1 |
| wide160 / fortress-81w@3 | Kg8 | 1 (in check; unused) | -6.51 | -7.08 | -6.75 | 0 / 0 / 0 |
| wide160 / rook-bishop-65w@0 | Rh7+ | 26 | +3.27 | +2.71 | +1.85 | 92 / 92 / 0 |
| wide160 / rook-bishop-65w@1 | Kg8 | 1 (in check; unused) | -1.87 | -1.93 | -1.93 | 93 / 93 / 0 |
| wide160 / rook-bishop-65w@3 | Kh8 | 7 | -2.18 | -2.42 | -2.15 | 33 / 33 / 0 |
| wide160 / catalan-80w@0 | Be5 | 14 | +1.15 | +1.17 | +1.36 | 56 / 56 / 0 |
| wide160 / catalan-80w@1 | c3 | 13 | -0.80 | -0.98 | -1.63 | 11 / 7 / 4 |
| wide160 / catalan-80w@3 | Kg4 | 3 | -2.47 | -1.60 | -1.64 | 0 / 0 / 0 |
| wide160 / corpus-1332191-109@0 | f5 | 6 | +0.95 | +2.44 | +2.68 | 82 / 82 / 0 |
| wide160 / corpus-1332191-109@1 | fxe4 | n/a | -1.32 | -2.20 | -3.02 | 30 / 30 / 0 |
| wide160 / corpus-1332191-109@3 | Ba2 | 5 | -1.02 | -2.89 | -2.40 | 28 / 28 / 0 |
| wide160 / corpus-364233-225@0 | Kd7 | 5 | +0.14 | +0.69 | +0.75 | 54 / 54 / 0 |
| wide160 / corpus-364233-225@1 | Kc4 | 4 | -0.09 | -0.71 | -0.76 | 43 / 43 / 0 |
| wide160 / corpus-364233-225@3 | Re4 | 1 | -0.16 | -0.75 | -0.69 | 28 / 28 / 0 |
| wide160 / corpus-1176670-264@0 | Rb1 | 8 | -0.11 | -0.16 | -0.73 | 28 / 28 / 0 |
| wide160 / corpus-1176670-264@1 | Rxb1+ | n/a | +0.20 | +0.74 | +0.00 | 5 / 5 / 0 |
| wide160 / corpus-1630699-67@0 | b6 | 14 | +0.63 | +1.02 | +0.18 | 38 / 38 / 0 |
| wide160 / corpus-1630699-67@1 | a5 | 8 | -0.09 | -0.12 | -0.27 | 0 / 0 / 0 |
| wide160 / corpus-1630699-67@3 | bxa5 | n/a | -0.62 | -0.19 | -0.24 | 0 / 0 / 0 |
| wide160 / corpus-1494057-118@0 | Bg3 | 1 | +3.48 | +2.99 | +2.47 | 54 / 54 / 0 |
| wide160 / corpus-1494057-118@1 | Rf1+ | 1 | -3.02 | -2.08 | -1.49 | 17 / 10 / 0 |
| wide160 / corpus-1494057-118@3 | Kb6 | 6 | -3.40 | -0.44 | -1.73 | 0 / 0 / 0 |
| wide160 / corpus-2036369-274@0 | Bxg1 | n/a | -5.97 | +0.00 | +0.00 | 58 / 58 / 0 |
| wide160 / corpus-2036369-274@1 | Kf8 | 5 | -0.04 | +0.00 | +0.00 | 56 / 56 / 0 |
| wide160 / corpus-2036369-274@3 | Ke7 | 4 | -0.04 | +0.00 | +0.00 | 26 / 26 / 0 |
| wide160 / corpus-761265-138@0 | Rxa1 | n/a | +1.50 | M6 | M5 | 69 / 69 / 0 |
| wide160 / corpus-761265-138@1 | Kg7 | 1 | -14.92 | M-4 | M-5 | 67 / 67 / 0 |
| wide160 / corpus-761265-138@3 | Kh8 | 3 | -13.47 | M-3 | M-3 | 17 / 13 / 0 |
| wide160 / corpus-1421554-75@0 | g5 | 1 | -2.64 | -1.80 | -3.22 | 57 / 57 / 0 |
| wide160 / corpus-1421554-75@1 | Rb4 | 15 | +2.81 | +3.60 | +3.29 | 9 / 9 / 0 |
| wide160 / corpus-1421554-75@3 | gxf4 | n/a | +1.58 | +3.79 | +3.19 | 2 / 2 / 0 |
| pilot_wdl / fortress-80w@0 | Rd4 | 11 | +2.14 | +4.41 | +4.59 | 41 / 41 / 0 |
| pilot_wdl / fortress-80w@1 | Bc2 | 6 | -2.49 | -4.81 | -3.47 | 3 / 3 / 0 |
| pilot_wdl / fortress-80w@3 | Bd3 | 13 | -1.92 | -4.20 | -3.94 | 0 / 0 / 0 |
| pilot_wdl / fortress-81w@0 | Rd1 | 9 | +1.86 | +3.08 | +5.05 | 47 / 47 / 0 |
| pilot_wdl / fortress-81w@1 | Bc2 | 9 | -1.38 | -4.41 | -4.71 | 1 / 1 / 0 |
| pilot_wdl / fortress-81w@3 | Kg8 | 4 (in check; unused) | -1.81 | -3.76 | -4.13 | 0 / 0 / 0 |
| pilot_wdl / rook-bishop-65w@0 | Rh7+ | 20 | +3.46 | +2.63 | +2.83 | 67 / 67 / 0 |
| pilot_wdl / rook-bishop-65w@1 | Kg8 | 1 (in check; unused) | -3.75 | -2.42 | -2.42 | 64 / 64 / 0 |
| pilot_wdl / rook-bishop-65w@3 | Kh8 | 8 | -4.02 | -2.27 | -2.33 | 30 / 29 / 1 |
| pilot_wdl / catalan-80w@0 | Be5 | 12 | +0.49 | +0.40 | +0.68 | 57 / 57 / 0 |
| pilot_wdl / catalan-80w@1 | c3 | 3 | -0.03 | -0.40 | -1.48 | 42 / 37 / 5 |
| pilot_wdl / catalan-80w@3 | Kg4 | 1 | -1.09 | -1.40 | -1.45 | 3 / 3 / 0 |
| pilot_wdl / corpus-1332191-109@0 | f5 | 9 | +1.39 | +1.53 | +1.73 | 54 / 54 / 0 |
| pilot_wdl / corpus-1332191-109@1 | fxe4 | n/a | -0.79 | -1.64 | -2.22 | 20 / 18 / 0 |
| pilot_wdl / corpus-1332191-109@3 | Ba2 | 7 | -0.81 | -2.15 | -2.32 | 7 / 7 / 0 |
| pilot_wdl / corpus-364233-225@0 | Kd7 | 2 | +0.55 | +2.17 | +2.23 | 64 / 64 / 0 |
| pilot_wdl / corpus-364233-225@1 | Kc4 | 2 | -0.56 | -1.79 | -1.89 | 37 / 37 / 0 |
| pilot_wdl / corpus-364233-225@3 | Re4 | 16 | -1.68 | -2.05 | -2.10 | 4 / 4 / 0 |
| pilot_wdl / corpus-1176670-264@0 | Rb1 | 3 | -0.29 | -0.57 | -2.48 | 47 / 47 / 0 |
| pilot_wdl / corpus-1176670-264@1 | Rxb1+ | n/a | +0.49 | +2.08 | +0.00 | 4 / 4 / 0 |
| pilot_wdl / corpus-1630699-67@0 | b6 | 17 | +0.20 | +0.40 | +0.16 | 29 / 29 / 0 |
| pilot_wdl / corpus-1630699-67@1 | a5 | 1 | -0.08 | -0.03 | -0.18 | 0 / 0 / 0 |
| pilot_wdl / corpus-1630699-67@3 | bxa5 | n/a | -0.94 | -0.11 | -0.08 | 0 / 0 / 0 |
| pilot_wdl / corpus-1494057-118@0 | Bg3 | 3 | +1.72 | +2.58 | +1.63 | 42 / 42 / 0 |
| pilot_wdl / corpus-1494057-118@1 | Rf1+ | 7 | -0.82 | -0.71 | -0.88 | 27 / 12 / 0 |
| pilot_wdl / corpus-1494057-118@3 | Kb6 | 2 | -0.37 | -1.17 | -1.04 | 0 / 0 / 0 |
| pilot_wdl / corpus-2036369-274@0 | Bxg1 | n/a | -1.53 | +0.00 | +0.00 | 53 / 53 / 0 |
| pilot_wdl / corpus-2036369-274@1 | Kf8 | 5 | -0.31 | +0.00 | -0.04 | 51 / 51 / 0 |
| pilot_wdl / corpus-2036369-274@3 | Ke7 | 1 | -0.10 | -0.04 | -0.09 | 23 / 23 / 0 |
| pilot_wdl / corpus-761265-138@0 | Rxa1 | n/a | +1.32 | M13 | M6 | 61 / 61 / 0 |
| pilot_wdl / corpus-761265-138@1 | Kg7 | 1 | -8.43 | M-5 | M-4 | 58 / 58 / 0 |
| pilot_wdl / corpus-761265-138@3 | Kh8 | 3 | -12.13 | M-3 | M-3 | 32 / 31 / 0 |
| pilot_wdl / corpus-1421554-75@0 | g5 | 3 | -0.44 | -1.58 | -2.10 | 59 / 59 / 0 |
| pilot_wdl / corpus-1421554-75@1 | Rb4 | 10 | +0.76 | +2.27 | +2.43 | 21 / 21 / 0 |
| pilot_wdl / corpus-1421554-75@3 | gxf4 | n/a | +0.82 | +2.63 | +2.63 | 7 / 7 / 0 |
| pilot_score / fortress-80w@0 | Rd4 | 5 | +1.94 | +4.57 | +4.45 | 51 / 51 / 0 |
| pilot_score / fortress-80w@1 | Bc2 | 7 | -2.01 | -3.69 | -3.32 | 0 / 0 / 0 |
| pilot_score / fortress-80w@3 | Bd3 | 13 | -1.66 | -3.42 | -4.60 | 0 / 0 / 0 |
| pilot_score / fortress-81w@0 | Rd1 | 5 | +1.28 | +3.40 | +4.37 | 50 / 50 / 0 |
| pilot_score / fortress-81w@1 | Bc2 | 7 | -1.11 | -3.53 | -4.01 | 0 / 0 / 0 |
| pilot_score / fortress-81w@3 | Kg8 | 4 (in check; unused) | -1.38 | -4.40 | -4.80 | 0 / 0 / 0 |
| pilot_score / rook-bishop-65w@0 | Rh7+ | 20 | +3.87 | +2.35 | +2.16 | 61 / 61 / 0 |
| pilot_score / rook-bishop-65w@1 | Kg8 | 1 (in check; unused) | -2.89 | -2.01 | -2.01 | 64 / 64 / 0 |
| pilot_score / rook-bishop-65w@3 | Kh8 | 8 | -2.79 | -2.15 | -2.21 | 44 / 41 / 0 |
| pilot_score / catalan-80w@0 | Be5 | 12 | +0.43 | +0.34 | +0.60 | 54 / 54 / 0 |
| pilot_score / catalan-80w@1 | c3 | 5 | -0.17 | -0.30 | -1.46 | 30 / 29 / 1 |
| pilot_score / catalan-80w@3 | Kg4 | 1 | -1.01 | -1.49 | -2.07 | 8 / 8 / 0 |
| pilot_score / corpus-1332191-109@0 | f5 | 7 | +1.55 | +1.64 | +1.76 | 55 / 55 / 0 |
| pilot_score / corpus-1332191-109@1 | fxe4 | n/a | -1.07 | -1.72 | -2.03 | 8 / 7 / 0 |
| pilot_score / corpus-1332191-109@3 | Ba2 | 6 | -1.08 | -1.99 | -1.94 | 7 / 7 / 0 |
| pilot_score / corpus-364233-225@0 | Kd7 | 1 | +0.70 | +1.82 | +2.02 | 75 / 75 / 0 |
| pilot_score / corpus-364233-225@1 | Kc4 | 4 | -0.52 | -1.62 | -1.95 | 42 / 42 / 0 |
| pilot_score / corpus-364233-225@3 | Re4 | 16 | -1.59 | -1.93 | -1.98 | 12 / 12 / 0 |
| pilot_score / corpus-1176670-264@0 | Rb1 | 6 | -0.17 | -0.56 | -2.11 | 47 / 47 / 0 |
| pilot_score / corpus-1176670-264@1 | Rxb1+ | n/a | +0.43 | +1.93 | +0.00 | 4 / 4 / 0 |
| pilot_score / corpus-1630699-67@0 | b6 | 15 | +0.15 | +0.26 | +0.06 | 27 / 27 / 0 |
| pilot_score / corpus-1630699-67@1 | a5 | 1 | -0.02 | +0.00 | -0.14 | 0 / 0 / 0 |
| pilot_score / corpus-1630699-67@3 | bxa5 | n/a | -0.88 | -0.16 | -0.14 | 0 / 0 / 0 |
| pilot_score / corpus-1494057-118@0 | Bg3 | 9 | +1.39 | +2.37 | +0.53 | 38 / 38 / 0 |
| pilot_score / corpus-1494057-118@1 | Rf1+ | 4 | -0.57 | -0.71 | -0.70 | 27 / 15 / 0 |
| pilot_score / corpus-1494057-118@3 | Kb6 | 6 | -0.12 | -1.02 | -0.80 | 0 / 0 / 0 |
| pilot_score / corpus-2036369-274@0 | Bxg1 | n/a | -1.24 | +0.00 | +0.00 | 53 / 53 / 0 |
| pilot_score / corpus-2036369-274@1 | Kf8 | 4 | -0.23 | +0.00 | -0.02 | 52 / 52 / 0 |
| pilot_score / corpus-2036369-274@3 | Ke7 | 2 | -0.11 | -0.05 | -0.04 | 24 / 24 / 0 |
| pilot_score / corpus-761265-138@0 | Rxa1 | n/a | +0.82 | M14 | M6 | 84 / 84 / 0 |
| pilot_score / corpus-761265-138@1 | Kg7 | 1 | -7.35 | M-5 | M-5 | 83 / 83 / 0 |
| pilot_score / corpus-761265-138@3 | Kh8 | 3 | -11.50 | M-3 | M-3 | 19 / 11 / 0 |
| pilot_score / corpus-1421554-75@0 | g5 | 2 | -0.32 | -1.85 | -2.15 | 48 / 48 / 0 |
| pilot_score / corpus-1421554-75@1 | Rb4 | 14 | +1.16 | +2.40 | +2.40 | 14 / 14 / 0 |
| pilot_score / corpus-1421554-75@3 | gxf4 | n/a | +1.24 | +2.22 | +2.63 | 4 / 4 / 0 |

A forced-root search has a different allocation of work. Improvement there does not by itself prove that
policy protection in normal search would help. The policy-top alternative and its child raw reading are retained in contexts/.
For a causal claim, inspect the exact node windows, history/policy scores and return reasons in natural/ first.
A defence absent from the trace may be on an unvisited reference path; this is not automatically pruning of that defence.
No generic policy-protection treatment was run, and no playing-strength conclusion follows.
