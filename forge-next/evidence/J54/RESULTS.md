# J54 zero-block skip results

All sampled hidden/value/policy outputs, exact search trajectories and lifecycle checks matched. No promotion or strength claim.

- candidate_vs_control: median paired NPS +2.64%; 95% game bootstrap [2.3512341866871678, 2.9926134243620672].
- candidate_vs_shipped: median paired NPS +2.88%; 95% game bootstrap [2.1352989390207355, 3.3509504305770266].
- control_vs_shipped: median paired NPS +0.10%; 95% game bootstrap [-0.20646873381300335, 0.45197950712114743].

These are fixed-work search timings, with model load and TT clear outside the timer; no games or gameplay strength inference. The 48 positions reuse J48/J52, so this is a known engineering fixture set, not an untouched strength test.

Code generation is saved for inspection. Retained source packing is not by itself evidence that the compiler vectorized it. A nonpositive or uncertain gain does not justify promotion.
