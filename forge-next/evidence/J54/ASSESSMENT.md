# J54: zero skipping now gives a small, measured speed gain

Completed 24 October 2026 after explicit user approval. **Median paired search
NPS increased 2.64% against the rebuilt control**, with a 95% game-bootstrap
interval of **+2.35% to +2.99%**. The candidate preserved every sampled raw
evaluation and reported search trajectory. The daily driver was not replaced.

## What changed

The placement evaluator reserves 64 inputs for relations and currently fills
them with zero. Their products need not contribute to its dense mixer. J52
removed these products but also changed input packing; the resulting packing
lost vectorization and search slowed by 9.07%.

J54 retains the original packing, range checks and scalar fallback verbatim.
It specializes only the traversal of dense multiply blocks for placement mode:
blocks 0–7 and 10 remain; blocks 8–9 are omitted. Metadata still participates.
Other modes retain the complete kernel. Neither weights nor search settings
changed. The adopted J47/J48 search corrections remain present.

Inspection of the compiled functions confirms 30 vector packing instructions
in both control and candidate. The specialized function has nine multiply
blocks versus the original eleven. The candidate's generic function retains
eleven. These are assembly diagnostics, not an isolated measurement of each
instruction's contribution to elapsed time. Removing two blocks does not remove
two-elevenths of the whole engine's work.

## Search measurements

One thread, 128 MiB hash, quiet policy enabled, two million requested nodes per
observation. Forty-eight positions come from 48 separate games: twelve each
from openings, forcing middlegames, quiet middlegames and endings. Three arms
and two reversed execution orders give **288 timed searches**. TT/history were
cleared before each search, outside its timer; model loading was also excluded.
Only one engine searched at a time on the same CPU core.

Repeated elapsed times were averaged within each position before taking paired
ratios. Actual node counts match exactly, so their ratio is the NPS ratio.

| Comparison | Median paired NPS change | 95% game-bootstrap interval |
| --- | ---: | ---: |
| Candidate / rebuilt control | +2.64% | +2.35% to +2.99% |
| Candidate / shipped engine | +2.88% | +2.14% to +3.35% |
| Rebuilt control / shipped engine | +0.10% | −0.21% to +0.45% |

The rebuilt control and shipped engine are **byte-identical**. Their comparison
measures timing noise, not a compiler upgrade.

Against the control, 47 of 48 positions were faster. The remaining opening
position, P122, was 0.72% slower. Median gains by stratum were opening +2.73%,
forcing +3.14%, quiet +2.34%, and endgame +1.98%.

Absolute median measured throughput was about 1.749M NPS for the rebuilt control
and 1.810M for the candidate. Dividing these separate medians is **not** how the
paired +2.64% estimate was calculated.

These are the known J48/J52 engineering fixtures, not an untouched strength
sample. The interval resamples these source games; it does not cover every
possible session, hardware configuration or position. This supports a modest
throughput improvement. It establishes neither an Elo gain nor better learned
chess understanding. At equal work, the sampled chess decisions were identical.

## Correctness and provenance

- All three loaded-model self-tests passed, each reporting 4,632 checks,
  including special moves, null moves, lazy updates and state restoration.
  The candidate additionally checks specialized/generic dense arithmetic
  against the scalar reference, including nonzero omitted-column weights and
  out-of-range fallback inputs.
- All 61 raw fixtures matched exactly: hidden activations, raw value logits,
  scalar evaluation and every legal quiet-policy output.
- Eight policy-on/off cases matched across three engines (24 searches).
- All 288 timing searches matched in actual nodes, reported depths, scores,
  bounds, principal variations and best moves. The worker replayed PV legality.
- The corrected stalemate-child witness and stop/readiness/quit passed for all
  three engines.
- A separate read-only recomputation checked the saved comparisons and
  exactness, rehashed 99 source/model inputs and all three binaries, and
  confirmed the original engine remained intact. No engines were rerun.

Wall time: **344.13 seconds (5m44s)**. Peak recorded aggregate RSS:
**469.84 MiB**. Two CPU cores allowed; no GPU, training or games.

The source-only protocol and preparation README remain frozen with their
original pending wording. The approval, completion receipt and this assessment
record the later execution; they do not rewrite the plan after seeing results.

## Retained evidence

All files below are alongside this assessment:

- `timings.json`, `summary.json`: raw observations and paired comparisons.
- `raw.json`, `policy-signatures.json`, `lifecycle.json`, `*-selftest.json`:
  correctness evidence.
- `*-kernel-disassembly.txt`, `codegen-inspection.json`: compiled kernels.
- `completion.json`, `resources.json`: execution and resource receipts.
- `POSTCHECK.json`, `postcheck.py`: independent saved-data recomputation.
- `source-lock.json`, `binaries.json`: source/model/binary identities.
- `candidate-engine`: isolated working candidate; requires the unchanged
  Specialist model via `--model`. No launcher or daily-driver promotion is
  implied by this file.

**Decision:** retain this patch as a verified optimization candidate. It recovers
the intended zero-case saving without changing the model. It does not make
future nonzero relational features free, and does not rescue J52's costly local
pattern implementation.
