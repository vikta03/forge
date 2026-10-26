# J74 assessment — completed, small trade-off rather than a clear improvement

The job completed successfully in 605.47 seconds. Both Wide160 models started
from identical random weights and received 67,108,864 presentations in exactly
the same batches/order. This compares two equally young models; neither was
required to beat the fully trained daily driver.

The score-focused recipe was 80% expected-score binary cross entropy plus 20%
full WDL cross entropy. Control retained full WDL cross entropy. Architecture,
quiet-policy training, optimizer schedule and runtime scalar conversion stayed
fixed. This is one recipe and one seed, not every possible scalar objective.

## Main result

On 58,866 ordinary confirmation positions from 9,361 source games, candidate
game-mean expected-score MSE improved by **0.83%**: 0.02939173 → 0.02914751.
The paired game-bootstrap interval for the absolute difference was
[-0.000355, -0.000139], favouring the candidate at this fixed seed.

Average absolute expected-score error nevertheless rose from **11.5136 to
11.5914 percentage points**. Difference +0.0778 pp, paired 95% interval
[+0.0530, +0.1030]. WDL KL also worsened: 0.248202 → 0.255177.
These are different metrics: squared error weights larger misses more heavily,
so lower MSE need not imply lower average absolute error or fewer large errors.

The declared admission criterion required at least 2% ordinary MSE improvement,
an interval below zero, and no ordinary MAE regression, plus auxiliary guards.
The candidate missed the improvement-size and MAE requirements. Other guards
passed. The run completed normally; this was an experiment decision, not a crash.

## The draw concern

Ordinary teacher-draw positions: MAE **5.9943 → 6.2014 pp**, +0.2071 pp
(paired 95% +0.1619 to +0.2522).

Quiet-ending confirmation contains 10,685 positions from 4,020 games:
overall MAE **11.2618 → 11.3161 pp**; its interval crosses zero.
Within confident teacher draws, MAE **5.5297 → 5.6125 pp**, +0.0828 pp
(paired 95% +0.0211 to +0.1469). Within decisive examples, MAE worsened by
0.1883 pp (paired 95% +0.0450 to +0.3399). The candidate has not demonstrated
a remedy for the conversion/fortress issue. Teacher agreement is not chess truth.

## Learning and search

At 16.8M presentations the score-focused candidate led on selection MAE/MSE
and WDL KL. By 33.6M, control was ahead on MAE/KL; final confirmation retained
that pattern. The early advantage did not grow over the observed pilot.
This does not establish what would happen at full convergence.

Policy CE improved modestly: ordinary 3.02698 → 3.00332; quiet endings
2.74560 → 2.73177. With a shared representation and changed value-loss weighting,
policy and value learning can trade off even though policy's coefficient stayed
fixed; this run does not isolate the cause of that improvement.

Across 24 one-second historical/corpus searches, best moves matched on 21/24.
Median paired depth difference was zero; paired NPS change was -1.87%.
Separate-arm median NPS was 1,401,755 versus 1,372,723. These are short,
single-observation searches with different learned trees, not a statistical
speed or strength comparison. Lower scores on a historical position alone do
not prove the resulting move preserves a win or recognizes a fortress.

## Verification and decision

Loss algebra, stable extreme gradients, historical-control equality, checkpoint
restart, integer export/native agreement, incremental updates, UCI lifecycle
and relocation checks passed. A separate saved-data review recomputed the
headline errors, verified identical initial predictions, replayed 16,853 saved
PV plies, and checked package/daily-driver hashes (`review/REVIEW.json`).

Keep current WDL training as the default. Preserve both isolated pilot engines;
do not promote either, launch full training, or claim an Elo gain. The small
MSE benefit is real on this panel/seed but does not establish a better overall
evaluator. Panels are source-game-disjoint from training but previously used
in research; confidence intervals do not include training-seed variability.
