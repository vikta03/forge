#!/usr/bin/env python3
"""Replay one FORGE evaluation in Python, printing every intermediate."""
import json, struct, sys, math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
NET = ROOT / "models/netB-best.forge"
ROUTES, ODD, EVEN, BINS, W, PSQ, KB, H = 8, 57, 44, 129, 8, 16, 32, 16
Z = 3 * W + PSQ  # mlpProduct

ODD_KEEP  = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,20,21,22,23,24,25,26,27,28,29,30,31,32,33]
EVEN_KEEP = [0,1,2,3,4,7,9,12,14,15,35,36,37,39,40,41,58,63]
plan = json.load(open(f"{ROOT}/rescale_shift.json"))
ODD_BIN  = [plan["odd"][i]  for i in ODD_KEEP]
EVEN_BIN = [plan["even"][i] for i in EVEN_KEEP]
WIDE = json.loads((ROOT / "experiments/wide_trace/wide_ratios.json").read_text())
ODD_BIN += [v["odd"] for v in WIDE.values()]
EVEN_BIN += [v["even"] for v in WIDE.values()]

SECTIONS = ["PAWNS","KNIGHT_BONUS","BISHOP_BONUS","ROOK_BONUS","QUEEN_BONUS",
            "KNIGHT_MOB","BISHOP_MOB","ROOK_MOB","QUEEN_MOB",
            "KING_SAFETY","THREATS","PASSERS"]
ODD_NAMES = (["boring score","non-pawn material diff"]
             + [f"{p} count diff" for p in ("pawn","knight","bishop","rook","queen")]
             + ["psqt.mg","psqt.eg"]
             + [f"{s}.{ph} diff" for s in SECTIONS for ph in ("mg","eg")]
             + ["space diff"])
EVEN_NAMES = (["|boring score|","phase","rule50","endgame scale"]
              + [f"total {p}s" for p in ("pawn","knight","bishop","rook","queen")]
              + [f"|{p} count diff|" for p in ("pawn","knight","bishop","rook","queen")]
              + ["|psqt.mg|","|psqt.eg|"]
              + [f"{s}.{ph} sum" for s in SECTIONS for ph in ("mg","eg")]
              + [f"{s}.{ph} |diff|" for s in SECTIONS for ph in ("mg","eg")]
              + ["space sum","|space diff|","opposite bishops","pure opp bishops","both colours"])
assert len(ODD_NAMES) == 34 and len(EVEN_NAMES) == 69


def load_net(path=NET):
    raw = open(path, "rb").read()
    header = struct.unpack("<8sIIIIIIIfQII8s", raw[:64])
    import zlib
    assert header[0] == b"FORGEF08" and header[1] == 6, "expected FORGE v6"
    assert header[2:8] == (64, 8, 57, 44, 129, 8), "expected netB dimensions"
    assert len(raw) == 730614 and zlib.crc32(raw[64:]) == header[10], "model integrity"
    pay, off = raw[64:], 0
    def take(n, dt, shape):
        nonlocal off
        a = np.frombuffer(pay[off:off + n], dt).reshape(shape); off += n; return a
    n = {}
    n["adv"]   = take(ROUTES*ODD*BINS*2, "<i2", (ROUTES, ODD, BINS))
    n["dec"]   = take(ROUTES*EVEN*BINS*2, "<i2", (ROUTES, EVEN, BINS))
    n["oddL"]  = take(ODD*BINS*W, "i1", (ODD, BINS, W))
    n["evenL"] = take(EVEN*BINS*W, "i1", (EVEN, BINS, W))
    n["advS"]  = take(ROUTES*4, "<f4", (ROUTES,))
    n["decS"]  = take(ROUTES*4, "<f4", (ROUTES,))
    n["oddSc"] = take(W*4, "<f4", (W,))
    n["evenSc"]= take(W*4, "<f4", (W,))
    n["decB"]  = take(ROUTES*4, "<f4", (ROUTES,))
    n["advO"]  = take(ROUTES*W*4, "<f4", (ROUTES, W))
    n["decO"]  = take(ROUTES*W*4, "<f4", (ROUTES, W))
    n["psqE"]  = take(KB*768*PSQ, "i1", (KB, 768, PSQ))
    n["psqSc"] = take(PSQ*4, "<f4", (PSQ,))
    n["psqA"]  = take(ROUTES*PSQ*4, "<f4", (ROUTES, PSQ))
    n["psqD"]  = take(ROUTES*PSQ*4, "<f4", (ROUTES, PSQ))
    n["lutA"]  = take(425, "u1", (25, 17))
    n["lutC"]  = take(425, "u1", (25, 17))
    n["w1"]    = take(ROUTES*Z*H*4, "<f4", (ROUTES, Z, H))
    n["b1"]    = take(ROUTES*H*4, "<f4", (ROUTES, H))
    n["mAdv"]  = take(ROUTES*H*4, "<f4", (ROUTES, H))
    n["mDec"]  = take(ROUTES*H*4, "<f4", (ROUTES, H))
    n["mSc"]   = take(3*4, "<f4", (3,))
    assert off == len(pay), (off, len(pay))
    return n


def round_ratio(value, num, shift):
    scaled = value * num
    mag = abs(scaled)
    q, r = mag >> shift, mag & ((1 << shift) - 1)
    half = 1 << (shift - 1)
    rounded = q + (1 if (r > half or (r == half and (q & 1))) else 0)
    return -rounded if scaled < 0 else rounded


def parse_tracedump(line):
    t = line.split()
    def grab(key, k):
        i = t.index(key)
        return [int(x) for x in t[i+1:i+1+k]]
    d = {}
    d["stm"] = grab("stm",1)[0]; d["boring"] = grab("boring",1)[0]
    d["known"] = grab("known",1)[0]; d["phase"] = grab("phase",1)[0]
    d["rule50"] = grab("rule50",1)[0]; d["scale"] = grab("scale",1)[0]
    d["npm"] = grab("npm",2); d["psqt"] = grab("psqt",2); d["space"] = grab("space",2)
    d["counts"] = grab("counts",10); d["bishops"] = grab("bishops",4)
    sec = grab("sections",48)
    d["wide"] = grab("wide",52)
    d["sec"] = [[(sec[s*4+c*2], sec[s*4+c*2+1]) for c in (0,1)] for s in range(12)]
    return d


def build_features(d):
    """Returns full 34 odd raws and 69 even raws, side-to-move relative."""
    us, them = d["stm"], 1 - d["stm"]
    orient = 1 if us == 0 else -1
    cw, cb = d["counts"][:5], d["counts"][5:]
    ours, theirs = (cw, cb) if us == 0 else (cb, cw)
    signed = [ours[i] - theirs[i] for i in range(5)]
    totals = [cw[i] + cb[i] for i in range(5)]
    odd = [d["boring"], d["npm"][us] - d["npm"][them], *signed,
           d["psqt"][0]*orient, d["psqt"][1]*orient]
    for s in range(12):
        odd.append((d["sec"][s][0][0] - d["sec"][s][1][0]) * orient)
        odd.append((d["sec"][s][0][1] - d["sec"][s][1][1]) * orient)
    odd.append((d["space"][0] - d["space"][1]) * orient)

    ev = [abs(d["boring"]), d["phase"], d["rule50"], d["scale"], *totals,
          *[abs(x) for x in signed], abs(d["psqt"][0]), abs(d["psqt"][1])]
    for s in range(12):
        ev.append(d["sec"][s][0][0] + d["sec"][s][1][0])
        ev.append(d["sec"][s][0][1] + d["sec"][s][1][1])
    for s in range(12):
        ev.append(abs(d["sec"][s][0][0] - d["sec"][s][1][0]))
        ev.append(abs(d["sec"][s][0][1] - d["sec"][s][1][1]))
    ev.append(d["space"][0] + d["space"][1])
    ev.append(abs(d["space"][0] - d["space"][1]))
    wd, wl, bd, bl = d["bishops"]
    od, ol, ed, el = (wd, wl, bd, bl) if us == 0 else (bd, bl, wd, wl)
    opposite = ours[2] == 1 and theirs[2] == 1 and bool(od) != bool(ed)
    pure = (opposite and ours[1] == 0 and theirs[1] == 0
            and ours[3] == 0 and theirs[3] == 0 and ours[4] == 0 and theirs[4] == 0)
    ev += [int(opposite), int(pure), int(od and ol) + int(ed and el)]
    assert len(odd) == 34 and len(ev) == 69
    return odd, ev


def evaluate(d, planes, net, verbose=True):
    odd_raw, even_raw = build_features(d)
    odd_kept  = [odd_raw[i]  for i in ODD_KEEP]
    even_kept = [even_raw[i] for i in EVEN_KEEP]
    white, black = d["wide"][:26], d["wide"][26:]
    orient = 1 if d["stm"] == 0 else -1
    odd_kept += [(a-b)*orient for a,b in zip(white,black)]
    even_kept += [a+b for a,b in zip(white,black)]
    odd_sign = [(1 if v > 0 else (-1 if v < 0 else 0)) for v in odd_kept]
    odd_bin  = [min(max(round_ratio(abs(v), *ODD_BIN[i]), 0), 128)
                for i, v in enumerate(odd_kept)]
    even_bin = [min(max(round_ratio(v, *EVEN_BIN[i]) + 64, 0), 128)
                for i, v in enumerate(even_kept)]

    phase, pawns = min(d["phase"], 24), even_raw[4]
    rA, rC = int(net["lutA"][phase][pawns]), int(net["lutC"][phase][pawns])

    directA = sum(odd_sign[f] * int(net["adv"][rA, f, odd_bin[f]]) for f in range(ODD))
    directC = sum(int(net["dec"][rC, f, even_bin[f]]) for f in range(EVEN))
    oddSum  = sum(odd_sign[f] * net["oddL"][f, odd_bin[f]].astype(np.int32) for f in range(ODD))
    evenSum = sum(net["evenL"][f, even_bin[f]].astype(np.int32) for f in range(EVEN))

    # own-king bucket: own king is plane 320..383, side-to-move relative
    king_plane = [p for p in planes if 320 <= p < 384][0]
    ksq = king_plane - 320
    bucket = (ksq // 16) * 8 + (ksq % 8)
    psqSum = net["psqE"][bucket][planes].astype(np.int32).sum(axis=0)

    A = directA * float(net["advS"][rA])
    C = float(net["decB"][rC]) + directC * float(net["decS"][rC])
    directA_logit, directC_logit = A, C

    o = oddSum.astype(np.float64) * net["oddSc"]
    e = evenSum.astype(np.float64) * net["evenSc"]
    p = psqSum.astype(np.float64) * net["psqSc"]
    z = np.concatenate([o * net["mSc"][0], e * net["mSc"][1], p * net["mSc"][2],
                        np.tanh(o) * (1.0 / (1.0 + np.exp(-e)))])
    hidden = np.clip(z @ net["w1"][rA] + net["b1"][rA], 0.0, 8.0)
    mlpA = float(hidden @ net["mAdv"][rA]); mlpC = float(hidden @ net["mDec"][rC])
    A += mlpA; C += mlpC

    q, c = 1/(1+math.exp(-A)), 1/(1+math.exp(-C))
    E = 0.5 + c * (q - 0.5)
    logit = math.log(E / (1 - E))
    score = 187.0 * logit

    if verbose:
        print(f"  route  advantage={rA}  decisiveness={rC}   (phase {phase}, {pawns} pawns)")
        print(f"  king bucket {bucket}  (own king on plane {king_plane}, rel square {ksq})")
        print(f"  direct advantage  raw={directA:>8}  x{net['advS'][rA]:.3e} = {directA_logit:+.5f}")
        print(f"  direct decisive   raw={directC:>8}  x{net['decS'][rC]:.3e} "
              f"+ bias {net['decB'][rC]:+.5f} = {directC_logit:+.5f}")
        print(f"  odd latent   {np.array2string(o, precision=3, floatmode='fixed')}")
        print(f"  even latent  {np.array2string(e, precision=3, floatmode='fixed')}")
        print(f"  placement    {np.array2string(p, precision=3, floatmode='fixed')}")
        print(f"  mixer contribution  advantage {mlpA:+.5f}   decisive {mlpC:+.5f}")
        print(f"  A = {A:+.6f}  ->  q = {q:.6f}")
        print(f"  C = {C:+.6f}  ->  c = {c:.6f}")
        print(f"  W/D/L = {c*q:.4f} / {1-c:.4f} / {c*(1-q):.4f}")
        print(f"  E = {E:.6f}   logit(E) = {logit:+.6f}   x187 = {score:+.3f}"
              f"  ->  {int(round(score)):+d} cp")
    return dict(rA=rA, rC=rC, bucket=bucket, odd_raw=odd_raw, even_raw=even_raw,
                odd_kept=odd_kept, even_kept=even_kept, odd_sign=odd_sign,
                odd_bin=odd_bin, even_bin=even_bin, directA=directA, directC=directC,
                dA=directA_logit, dC=directC_logit, mlpA=mlpA, mlpC=mlpC,
                A=A, C=C, q=q, c=c, E=E, score=score, o=o, e=e, p=p, hidden=hidden, z=z,
                odd_contributions=[odd_sign[f] * int(net["adv"][rA,f,odd_bin[f]]) * float(net["advS"][rA]) for f in range(ODD)],
                even_contributions=[int(net["dec"][rC,f,even_bin[f]]) * float(net["decS"][rC]) for f in range(EVEN)])


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("trace", type=Path, help="saved forgeinspect output")
    ap.add_argument("--model", type=Path, default=NET)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    lines = args.trace.read_text().splitlines()
    d = parse_tracedump(next(l for l in lines if l.startswith("tracedump")))
    if d["known"]: raise SystemExit("Special endgame evaluator; no neural replay")
    planes = [int(x) for x in next(l for l in lines if l.startswith("planedump")).split()[2:]]
    result = evaluate(d, planes, load_net(args.model))
    runtime = next(l for l in lines if l.startswith("forge exact 0")).split()
    def number(key): return float(runtime[runtime.index(key)+1])
    errors = {"advantage": abs(result["A"]-number("advantage")),
              "decisive": abs(result["C"]-number("decisive"))}
    if max(errors.values()) > 0.00003: raise SystemExit(f"Replay disagreement: {errors}")
    result["runtime"] = {k:number(k) for k in ("score","searchscore","advantage","decisive","route")}
    result["max_logit_error"] = max(errors.values())
    result["feature_names"] = {"odd":[ODD_NAMES[i] for i in ODD_KEEP] + [k+" difference" for k in WIDE],
                               "even":[EVEN_NAMES[i] for i in EVEN_KEEP] + [k+" total" for k in WIDE]}
    if args.json:
        args.json.write_text(json.dumps(result, indent=2, default=lambda a:a.tolist()) + "\n")
