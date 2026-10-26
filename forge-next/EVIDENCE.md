# Results worth keeping, including the disappointments

These are local experiments, not a public Elo list. A stronger-looking game,
more depth, a faster kernel or a lower validation loss is not by itself a
playing-strength gain. Current daily-driver preference is separate from the
historical experimental promotion flags.

## Match screens

The screens below use **8 seconds per side for the whole game + 0.08 seconds
per move**, concurrency 8, paired openings and swapped colours. They do not
establish behaviour at tournament time controls. See each report for hash,
threads, opening selection and full audit conditions.

| Candidate / reference | Games | W / D / L | Candidate score | 95% opening-pair interval | Conclusion |
|---|---:|---|---:|---|---|
| Wide160 / zero-skip Specialists | 100 | 35 / 31 / 34 | 50.5% | 42.5–58.5% | Unresolved |
| Deep32 / zero-skip Specialists | 100 | 24 / 35 / 41 | 41.5% | 35.5–47.0% | Evidence against Deep32 at these clocks |
| Wide160 FullKey / original Wide160 | 100 | 20 / 61 / 19 | 50.5% | 45.0–56.0% | No strength difference resolved |
| Wide192 / original Wide160 | 400 | 113 / 171 / 116 | 49.6% | 45.9–53.4% | No strength difference resolved |
| Reader64 / Wide192 | 400 | 92 / 158 / 150 | 42.75% | 39.25–46.25% | Reader64 underperformed |

[J59](evidence/J59/RESULTS.md) · [J63](evidence/J63/RESULTS.md) ·
[J70](evidence/J70/RESULTS.md) · [J72](evidence/J72/RESULTS.md).
Each directory includes PGNs and machine-readable match receipts. Opening
overlap with training is not ruled out; intervals are per comparison.

## Engineering and diagnosis

- **J47/J48:** corrected demonstrated search/rule failures: unusable
  en-passant repetition identity, dead-material handling, stale TT pointers
  across recursive search, leaking sibling extension state, abort handling,
  capture delta, mate-window and depth issues, lifecycle handling and stalemate
  before selective shortcuts. The 200-game J49 screen scored 51/110/39;
  correctness was established on fixtures, while strength remained uncertain.
- **J54:** skip arithmetic on reserved zero inputs without changing the learned
  function. More speed is useful even without a new chess concept.
- **J62/J63:** full 64-bit TT key verification removes partial-key false hits
  in the tested mechanism, but fewer entries fit the same Hash budget. Median
  paired wall NPS changed −1.66%; median depth stayed 20. This is a separate
  Wide160 branch; the published Wide192 source remains its actual baseline.
- **J65:** census of 213.90M training + 2.11M validation records, followed by
  targeted inspections. Relevant endings are present; coverage, imitation and
  search errors are distinct questions.
- **J66:** 36 of 44 large raw disagreements dropped below the diagnostic
  threshold after two seconds of search. Some remained. A raw score alone
  can exaggerate the apparent practical problem.
- **J73:** longer draw-case investigations of Wide160, Wide192 and Reader64
  did not establish a reliable conversion/defence improvement.
- **J74:** a matched score-focused training pilot improved one MSE metric
  but worsened average absolute error; it did not earn a full run.
- **J75/J76:** policy influence and local tablebases were investigated
  separately. Neither authorises a claim that every fortress has been solved.
- **J77:** with tables disabled, Stockfish's learned evaluation already gives
  +0.02 on the known drawn h-pawn fortress, where Wide192 gives +4.95 before
  search. On another eight-piece root Stockfish needs search to approach zero.
  Engine centipawn scales are not calibrated identically; the known outcomes
  and contrasting controls are the meaningful evidence.
- **J78:** an early legal-repetition bound passed 61 contracts, 3,454 legal
  states and 20,400 history queries. Median paired NPS changed −0.27%
  (95% −0.63 to +0.47%); two cost positions lost three plies. The fortress
  score remained wrong. This candidate stays experimental and off by default.

## Audit trail

The [evidence index](evidence/index.json) lists each retained run and files.
Reports retain job IDs because those identify frozen experiments, rather than
marketing versions. `workspace/…` and `external-input/…` in historical receipts
are redacted provenance labels, **not paths you should create or execute**.
Result values, original run IDs and source hashes are retained. Some historical
reports mention additional raw logs or local files not included in this curated
snapshot; the index is the authoritative list of published evidence.

The publication does not rerun the experiments. It verifies file identity,
syntax, weights/header/checksums, packaging and links. Preserved failures in
J78 were orchestration parsing errors and are described in its report; no
chess check was relaxed to make the result pass.
