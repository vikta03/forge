import math
"""Retained J37 UCI client and J35 independent fixtures. GPL-3.0."""
import json
from pathlib import Path
import queue
import subprocess
import threading
import time
class UCI:
    def __init__(self,argv,log,cwd='/tmp',env=None):
        self.log=Path(log).open('w');self.q=queue.Queue()
        self.p=subprocess.Popen([str(x) for x in argv],cwd=cwd,env=env,
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        def read():
            try:
                for line in self.p.stdout:
                    self.log.write(line);self.log.flush();self.q.put(line.strip())
            finally:self.q.put(None)
        self.thread=threading.Thread(target=read,daemon=True);self.thread.start()
        try:
            self.send('uci');self.hello=self.until(lambda s:s=='uciok');self.ready()
        except BaseException:
            self.p.kill();self.p.wait();self.thread.join();self.log.close();raise
    def send(self,line):self.log.write('> '+line+'\n');self.log.flush();self.p.stdin.write(line+'\n');self.p.stdin.flush()
    def until(self,pred,timeout=15):
        end=time.monotonic()+timeout;lines=[]
        while True:
            line=self.q.get(timeout=max(.001,end-time.monotonic()))
            if line is None:raise RuntimeError('Engine exited unexpectedly')
            lines.append(line)
            if pred(line):return lines
            if time.monotonic()>=end:raise TimeoutError('UCI deadline')
    def ready(self):self.send('isready');return self.until(lambda s:s=='readyok')
    def option(self,n,v):self.send(f'setoption name {n} value {v}');return self.ready()
    def inspect(self,fen):
        self.option('UCI_Chess960','true');self.send('position fen '+fen);self.send('forge2inspect')
        return json.loads(self.until(lambda s:s.startswith('forge2inspect '))[-1].split(' ',1)[1])
    def search(self,fen,go='nodes 8192',moves=None):
        self.option('UCI_Chess960','false');self.send('ucinewgame');self.send('position fen '+fen+((' moves '+moves) if moves else ''));self.ready()
        self.send('go '+go);return self.until(lambda s:s.startswith('bestmove '))
    def close(self):
        try:
            if self.p.poll() is None:self.send('quit');self.p.wait(timeout=5)
        finally:
            if self.p.poll() is None:self.p.kill();self.p.wait()
            self.thread.join(timeout=2);self.log.close()
        if self.p.returncode!=0:raise RuntimeError('Engine exit '+str(self.p.returncode))

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

def cp_from_raw(raw):
    a,c=(x/256. for x in raw)
    def logadd(x,y):return max(x,y)+math.log1p(math.exp(-abs(x-y)))
    t=logadd(0,a);v=187*(logadd(t,math.log(2)+a+c)-logadd(t,math.log(2)+c))
    return max(-12000,min(12000,math.floor(v+.5) if v>=0 else math.ceil(v-.5)))
