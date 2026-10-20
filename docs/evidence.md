# Evidence ledger

## Historical artifacts

The original executable SHA256 was
`0e362dd6aa59619c355e4b5d6db56b4aba616269ea22f557ba59a7e73bd9abe5`.
The released model keeps SHA256
`49cdece06bf5006835808ce4c2fde904765b2da26487f430c3f845a749323a6b`.
The source snapshot comes from `engine/experiments/wide_trace/src`.
Imported-file hashes identify that snapshot before the documented release edits.

The netB-best 200-game PGN at 8+0.08 records 47 W / 103 D / 50 L:
49.25% score. It compares against the older next-strength model, not a matched
net0 control. Single-seed offline improvements do not establish a strength gain.
The archived runtime parity receipt covers 10,000 evaluated positions,
not every legal position, search state or rule edge case.

The four training histories are preserved as machine-readable evidence.
Top-one agreement means agreement with the deeper-search move labels in that
offline set. It is neither a human chess rating nor a match win rate.

## Release verification

The release adds clock-aware static-evaluation reuse, portable packaging and
inspection tools. J32 compares static evaluations with the historical binary
on a deterministic 64-position prefix, runs inherited rules/Chess960 fixtures,
and checks both cache paths, model errors and relocation.

A short interleaved timing screen compares original and corrected search
under stated conditions. It is sensitive to platform, position, compiler and
clock-dependent search changes. A repeatable median slowdown over 10% blocks
release pending investigation. It is not a benchmark sweep or strength test.

The Ubuntu package is separately built and checked in GitHub Actions.
The downloadable verification receipt records actual outcomes, environment,
hashes and limitations. Failed attempts remain in the private preparation
archive and are summarized in the final receipt; success is not assumed in
advance.

J32 passed locally on 21 October 2026: 28 inherited perft cases covering
845,352,218 nodes, rules/Chess960/cache tests, 64 static fixtures (63 neural,
one in-check diagnostic), protocol/error/relocation checks, and seven retained
neural positions from the eight-row extraction fixture. All compared integer
scores matched. The independent worked-example replay matched C++ logits within
0.00003. The two preparation failures were an extractor link dependency omission
and a wrapper CLI argument mismatch; both were fixed and retained in the receipt.

The three-position, five-repeat interleaved timing screen measured a median
elapsed-time ratio of **1.02554** (about **2.6% slower**) at 32,768 nodes,
below the 10% investigation threshold. Individual searches took roughly
30–40 ms. This is a small noise-sensitive screen, not a reliable estimate of
gameplay NPS or Elo. The historical binary was compiled for the laptop's native
ISA; the candidate uses the fixed Haswell target.
