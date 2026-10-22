"""Frozen J35 fixtures and independent CP algebra; extracted for publication. GPL-3.0."""
import math
import hashlib
from pathlib import Path
FENS=[
    'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1',
    'r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1',
    '6k1/6p1/6Pp/ppp5/3pP3/2P1P1P1/PP5P/K7 w - - 0 1',
    '1kbr3r/pp6/8/P1n2ppq/2N3n1/4Q1P1/3B1P2/R1R2BK1 w - - 0 1',
    '4rrk1/pp1n1pp1/1q2p2p/1P1bP3/3P4/P2NBP2/2Q2P1P/R4RK1 w - - 0 21',
    '8/8/8/1k6/8/2R2K2/6P1/4r3 w - - 0 1',
]
SPECIAL=[
    '4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1',
    'r6k/1P6/8/8/8/8/6p1/4K3 w - - 0 1',
    '4k3/8/8/8/8/8/8/6KR w H - 0 1',
    '4k3/8/8/8/8/8/8/5KR1 w G - 0 1',
    '4k3/8/8/8/8/8/8/1R1K4 w B - 0 1',
    # Unusable EP and a pinned EP pawn follow FORGE's existing pseudo-legal rule.
    'rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1',
    'k3r3/8/8/3pP3/8/8/8/4K3 w - d6 0 1',
]

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def cp_from_raw(raw):
    a,c=(x/256. for x in raw)
    def logadd(x,y):return max(x,y)+math.log1p(math.exp(-abs(x-y)))
    t=logadd(0,a);v=187*(logadd(t,math.log(2)+a+c)-logadd(t,math.log(2)+c))
    return max(-12000,min(12000,math.floor(v+.5) if v>=0 else math.ceil(v-.5)))
