# The shared idea, and the architectures that test it

FORGE tries to put learned chess judgement inside a very cheap CPU evaluation.
The expensive learning happens during training. During a game, most of the
network is an indexed table of small integers.

## A position becomes two memories

Each side sees pieces in a relative board orientation. A king context chooses
one of 32 buckets; piece type, colour and square choose one of 768 entries.
Every entry contains a learned vector. Sum the occupied entries to describe
the board from that perspective, then do it for the other perspective.

After an ordinary move, subtract the departing entries and add the arriving
ones. A king-bucket change rebuilds the affected perspective. The vectors are
learned together: there is no instruction saying that a particular channel
must mean a weak square, a bad bishop or king danger.

Integer truncation, offsets and clipping turn those sums into reader inputs.
Eight additional values provide halfmove clock, material phase, both castling
states, en-passant square, piece count, check and a constant 32. Side to move
determines orientation. The clock is an active learned input; it does not give
the network the correct answer to every fortress.

## One small reader judges the board

In the four-head models, total piece count selects a reader: 2–8, 9–16, 17–24,
or 25–32 pieces. All four stacks are stored, but only one executes for a normal
uncached evaluation. The heads are dense learned matrices, not prewritten
opening/endgame rules. Boundary-overlap training in the capacity models also
shows neighbouring heads positions immediately across a boundary; inference
still chooses a single head.

The reader produces advantage A and decisiveness C. With q = sigmoid(A) and
c = sigmoid(C), W = cq, D = 1−c, L = c(1−q). Expected score E = W + D/2 is
converted to the scalar used by alpha-beta through the existing stable
equivalent of 187 × log(E/(1−E)). It is the model's estimate, not an exact
winning probability against an arbitrary opponent.

A 16-channel query also scores moves against shared learned from-square,
to-square and special-move tables. This quiet-policy signal helps ordering;
search histories and tactical ordering still matter. It neither selects the
final move directly nor replaces evaluation at the leaves.

## Capacity choices

| Model | Shared table | Reader inputs | Stored readers | Selected path | Runtime model bytes |
|---|---|---:|---|---|---:|
| Specialists | 24,576 × 128 | 328, including 64 reserved zeros | 4 × 32 | 32 → value/query | 4,271,904 |
| Wide160 | 24,576 × 160 | 328, no reserved zero block | 4 × 32 | 32 → value/query | 4,001,184 |
| Deep32 | 24,576 × 128 | 328, including reserved block | 4 × (32 → 32) | 32 → 32 → value/query | 3,218,848 |
| **Wide192** | **24,576 × 192** | **392** | **4 × 32** | **32 → value/query** | **4,795,816** |
| Reader64 | 24,576 × 192 | 392 | 2 × 64 | 64 → value/query | 4,795,816 |

The Specialists export retains inactive relation tables for compatibility;
the capacity formats omit that storage. File size is therefore not a direct
active-capacity comparison. Deep32 does **not** use a 32-channel placement
table: its additional reader layer is 32 channels. Reader64 uses two wider
readers, split at 16/17 pieces. Changing width, head count and routing together
does not isolate one cause of its result.

Wide192 stores 4,795,776 signed-int8 coefficients plus a 40-byte header. Its
shared placement table holds 4,718,592 of them. The active dense hidden matrix
is only 392 × 32. Stored capacity, arithmetic per evaluation, total search
nodes and playing strength are different quantities.

## Training and what actually happened

The models learn WDL and policy targets from retained Lc0 records using
quantization-aware training. The forward pass models integer rounding,
truncation and clipping; surrogate gradients let those discrete parameters
learn. Cross-entropy trains the WDL distribution and move policy. The
capacity experiments train from random initialization, rather than adding
new channels to an already converged model and declaring a short pilot final.

Wide192 completed the twelve-pass schedule; validation selected the checkpoint
after pass 10. Reader64 also completed twelve passes, with pass 11 selected.
Validation sees 2,106,816 records grouped into source games for uncertainty
estimates. It is used for model selection, not an untouched test set.

Wider is an opportunity, not a guarantee. Wide192's 400-game screen was
113 wins / 171 draws / 116 losses against Wide160. Reader64's slightly better
validation loss did not overcome its playing-speed/search tradeoff in its
screen. [Full conditions and limits](EVIDENCE.md).

## Where Boring ends and the learned evaluator begins

Boring is my earlier engine. It provides the rules, alpha-beta framework,
move generation, histories, hash table, time management and UCI foundation.
The newer evaluator replaces the ordinary concept-scoring route with learned
placement features and readers. The board still has a designed representation,
king buckets and state inputs; “learned” does not mean no inductive bias.

The source snapshots preserve the adopted J47/J48 corrections. The recent
Stockfish comparison shows that mature CPU evaluation can recognise a draw
FORGE still overvalues. Adding more capacity, tables, search cutoffs or
correction histories is not automatically a cure. That unresolved boundary is
part of the research, rather than being hidden by a polished demo.
