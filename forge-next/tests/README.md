# Preserved regression fixtures

`J47/contract_tests.cpp` and `J48/contract_tests.cpp` preserve the historical
search-correctness contracts. `J78` combines those retained fixtures with
repetition-state, cycle, legality and boundary checks. The latest native source
for that experiment is `variants/repetition-guard/src`.

The contract files include internal search implementations under instrumentation;
they are not standalone programs you can compile without the supporting source.
`J78/build.py` records the exact flags and instrumentation used in the approved
run. Its historical supervisor directory layout is documented by the code;
publication does not silently invoke it or claim the tests were rerun.

Independent expected move lists and saved positions are included. Expected
values were compared against independent legal-child/history oracles in the
recorded job. [J78 results](../evidence/J78/RESULTS.md) distinguish those checks,
sanitizer execution, disabled-option parity, timing outliers and unknown strength.

Do not substitute a synthetic pass for a playing-strength test. The unmodified
engine's `test`, `perftsuite` and accumulator checks also remain in each native
snapshot; their historical transcripts/receipts are scoped to the dated runs.
