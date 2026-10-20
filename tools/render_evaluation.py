#!/usr/bin/env python3
"""Render a verified evaluation receipt into the public worked example."""
import argparse
import html
import json
from pathlib import Path
import chess

ROOT=Path(__file__).resolve().parents[1]
FEN="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
def main():
    ap=argparse.ArgumentParser();ap.add_argument("receipt",type=Path);a=ap.parse_args()
    d=json.loads(a.receipt.read_text())
    assert d["max_logit_error"]<.00003
    board=chess.Board(FEN);parts=['<svg xmlns="http://www.w3.org/2000/svg" width="720" height="560" viewBox="0 0 720 560">','<rect width="720" height="560" rx="20" fill="#111827"/>']
    for rank in range(8):
        for file in range(8):
            x,y=48+file*54,38+(7-rank)*54
            color="#e9d7b7" if (rank+file)%2 else "#78908c"
            parts.append(f'<rect x="{x}" y="{y}" width="54" height="54" fill="{color}"/>')
            piece=board.piece_at(chess.square(file,rank))
            if piece:
                # Filled silhouettes with contrasting outlines remain legible at thumbnail size.
                symbol=chess.Piece(piece.piece_type,chess.BLACK).unicode_symbol()
                fill,stroke=("#fff9ed","#17212d") if piece.color else ("#17212d","#e6e9eb")
                parts.append(f'<text x="{x+27}" y="{y+43}" text-anchor="middle" font-family="DejaVu Sans,serif" font-size="46" fill="{fill}" stroke="{stroke}" stroke-width="1" paint-order="stroke">{symbol}</text>')
    for i,c in enumerate("abcdefgh"):parts.append(f'<text x="{75+i*54}" y="492" text-anchor="middle" fill="#c0cbd9" font-family="sans-serif">{c}</text>')
    for i in range(8):parts.append(f'<text x="29" y="{72+i*54}" fill="#c0cbd9" font-family="sans-serif">{8-i}</text>')
    side=[("NETB / WHITE TO MOVE",16),("One static evaluation",19),
          (f'A = {d["A"]:+.4f}',23),(f'C = {d["C"]:+.4f}',23),
          (f'Win {100*d["c"]*d["q"]:.2f}%',23),(f'Draw {100*(1-d["c"]):.2f}%',23),
          (f'Loss {100*d["c"]*(1-d["q"]):.2f}%',23),
          (f'{int(d["runtime"]["score"]):+d} cp',36)]
    for i,(t,size) in enumerate(side):
        parts.append(f'<text x="505" y="{70+i*47}" fill="#e9d7b7" font-family="sans-serif" font-size="{size}">{html.escape(t)}</text>')
    parts.append('<text x="48" y="533" fill="#b9c4d5" font-family="sans-serif" font-size="16">Measured network output. No search and no strength claim.</text></svg>')
    (ROOT/"docs/assets/evaluation.svg").write_text("\n".join(parts))
    table=[]
    for branch,key in (("Signed","odd"),("Context","even")):
        contributions=d[key+"_contributions"]
        for i in sorted(range(len(contributions)),key=lambda n:abs(contributions[n]),reverse=True)[:6]:
            raw=d[key+"_kept"][i];bin_=d[key+"_bin"][i]
            table.append(f'| {branch} | {d["feature_names"][key][i]} | {raw} | {bin_} | {contributions[i]:+.6f} |')
    vector=lambda k:", ".join(f"{x:+.5f}" for x in d[k])
    text=f'''# Open one evaluation

![A measured netB evaluation](assets/evaluation.svg)

This crowded middlegame has both castling rights, central tension, vulnerable
kings and several minor-piece interactions. It is a useful accounting example,
not a claim about the position's best move.

```text
{FEN}
```

**White to move.** Everything below is one static neural evaluation, before
alpha-beta searches any continuation. The raw diagnostic trace and numeric
receipt are in [the example data](../evidence/worked-evaluation.json).

## Follow the numbers

1. The board produces 57 signed features and 44 context features. Each selects
   a bin from 0 to 128. Signed features retain a separate sign; a context bin
   is centred at 64. A bin index is not a centipawn score.
2. The model selects **advantage route {d["rA"]}** and **decisiveness route
   {d["rC"]}** from phase and total pawn count. Its own-king placement bucket
   is **{d["bucket"]}**. These are learned routing choices, not named strategies.
3. The signed direct-table sum is **{d["directA"]} integer units**, scaled to
   **{d["dA"]:+.8f}** advantage logit. The context sum is **{d["directC"]}
   integer units**; its scale and bias give **{d["dC"]:+.8f}** decisiveness logit.
4. The 8 signed, 8 context and 16 placement channels form 32 pooled values.
   Eight gated products make 40 mixer inputs. The 16 clipped-ReLU outputs
   contribute **{d["mlpA"]:+.8f}** to advantage and **{d["mlpC"]:+.8f}**
   to decisiveness.
5. Add the two branches: **A = {d["A"]:+.8f}** and **C = {d["C"]:+.8f}**.
   Then `q = sigmoid(A) = {d["q"]:.8f}` and
   `c = sigmoid(C) = {d["c"]:.8f}`.
6. **W = {d["c"]*d["q"]:.8f}**, **D = {1-d["c"]:.8f}**,
   **L = {d["c"]*(1-d["q"]):.8f}**. Their sum is one.
   These are the network's predictions, not guaranteed outcome frequencies.
7. `E = W + D/2 = {d["E"]:.8f}`.
   `187 × log(E/(1−E)) = {d["score"]:+.6f}`.
   The runtime reports **{int(d["runtime"]["score"]):+d} cp**.
   Its search readout is **{int(d["runtime"]["searchscore"]):+d} cp**.

## The largest direct responses

The following are the six largest absolute direct contributions in each branch.
They are measured contributions to **A or C**, not independent material values
and not a causal explanation of the eventual searched move.

| Branch | Feature | Raw input | Bin | Direct logit contribution |
|---|---|---:|---:|---:|
{chr(10).join(table)}

The mixer can reinforce or counteract these contributions. Removing a piece
changes many features and routes together; a row cannot be interpreted as
“the value of that piece.”

## Every pooled channel

Signed latents: `{vector("o")}`.

Context latents: `{vector("e")}`.

Placement channels: `{vector("p")}`.

Activated mixer units: `{vector("hidden")}`.

These vectors are real measurements. Their coordinates are learned and are
not labelled “king safety neuron” or “passed-pawn neuron” without further
controlled evidence.

## Verify it

Send `position fen ...` followed by `forgeinspect` to the launcher, save the
output as `trace.txt`, then run:

```bash
python tools/replay.py trace.txt --json replay.json
```

The Python replay independently reads the quantized model, constructs bins,
sums tables and placement embeddings, runs the mixer and calculates W/D/L.
Its maximum A/C disagreement with this C++ evaluation is
**{d["max_logit_error"]:.9g}**, against a gate of 0.00003.
Small floating-point differences can exist across compilers; the receipt
records them rather than claiming every intermediate is bit-identical.
'''
    (ROOT/"docs/evaluation.md").write_text(text)
    (ROOT/"evidence/worked-evaluation.json").write_text(json.dumps(d,indent=2)+"\n")
if __name__=="__main__": main()
