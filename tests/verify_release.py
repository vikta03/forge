#!/usr/bin/env python3
"""Bounded protocol/model/relocation checks. Run only with hardware authorization."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import threading
import time
import chess

ROOT=Path(__file__).resolve().parents[1]
MODEL_HASH="49cdece06bf5006835808ce4c2fde904765b2da26487f430c3f845a749323a6b"

class Engine:
    def __init__(self, command, log, cwd=None):
        self.queue=queue.Queue(); self.log=Path(log).open("w")
        self.p=subprocess.Popen(list(map(str,command)),cwd=cwd,stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        def reader():
            for line in self.p.stdout:
                self.log.write(line); self.log.flush(); self.queue.put(line.strip())
            self.queue.put(None)
        self.thread=threading.Thread(target=reader,daemon=True); self.thread.start()
    def send(self,command):
        self.p.stdin.write(command+"\n"); self.p.stdin.flush()
    def until(self,predicate,timeout=20):
        end=time.monotonic()+timeout; lines=[]
        while True:
            line=self.queue.get(timeout=max(.001,end-time.monotonic()))
            if line is None: raise RuntimeError(f"Engine exited {self.p.poll()} before response")
            lines.append(line)
            if predicate(line): return lines
    def ready(self):
        self.send("uci"); lines=self.until(lambda x:x=="uciok")
        self.send("isready"); self.until(lambda x:x=="readyok")
        return lines
    def search(self,fen,limit="nodes 4096",timeout=20):
        self.send("position fen "+fen); start=time.monotonic(); self.send("go "+limit)
        lines=self.until(lambda x:x.startswith("bestmove "),timeout)
        move=lines[-1].split()[1]
        board=chess.Board(fen)
        assert move!="0000" and chess.Move.from_uci(move) in board.legal_moves, (fen,move)
        return lines,time.monotonic()-start
    def close(self):
        if self.p.poll() is None:
            self.send("quit")
            try:self.p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.p.kill();self.p.wait()
                raise AssertionError("quit timeout")
        self.thread.join(timeout=2); self.log.close()
        assert self.p.returncode==0, self.p.returncode
    def __enter__(self):return self
    def __exit__(self,*args):self.close()

def static_rows(command,model,fens,log):
    result=[]
    with Engine(command,log) as e:
        e.ready()
        if model:
            e.send(f"setoption name NetPath value {model}")
            e.until(lambda x:x.startswith("info string FORGE factor net loaded"))
        for fen in fens:
            e.send("position fen "+fen);e.send("forgeeval")
            line=e.until(lambda x:x.startswith(("forge exact","info string error")))[-1]
            result.append(line)
    return result

def verify(package,out):
    out.mkdir(parents=True,exist_ok=True)
    model=package/"models/netB-best.forge"
    assert hashlib.sha256(model.read_bytes()).hexdigest()==MODEL_HASH
    receipt={"model_sha256":MODEL_HASH,"checks":[]}
    fens=(ROOT/"tests/positions.fen").read_text().splitlines()
    with Engine([package/"forge"],out/"uci.log",cwd="/tmp") as e:
        options=e.ready()
        assert "id name FORGE netB 1.0.0" in options
        assert "id author Remmy Victor (@vikta03)" in options
        for fen in [chess.STARTING_FEN,*fens[:3]]:
            e.search(fen)
        e.send("position startpos");e.send("go infinite")
        e.until(lambda x:x.startswith("info depth "))
        e.send("stop");e.until(lambda x:x.startswith("bestmove "),5)
        e.send("setoption name Hash value 1")
        e.send("setoption name Clear Hash")
        e.send("isready");e.until(lambda x:x=="readyok")
        e.search(chess.STARTING_FEN,"nodes 1024")
        # Failed model replacement cannot keep using the old weights.
        e.send(f"setoption name NetPath value {out/'absent.forge'}")
        e.until(lambda x:x.startswith("info string error "))
        e.send("go nodes 1")
        assert any("NetPath is not loaded" in x for x in
                   e.until(lambda x:x.startswith("bestmove ")))
        e.send(f"setoption name NetPath value {model}")
        e.until(lambda x:x.startswith("info string FORGE factor net loaded "))
        e.search(chess.STARTING_FEN,"nodes 1024")
    receipt["checks"] += ["identity/readiness","four legal short searches","stop/quit",
                          "Hash resize/clear","failed model replacement and recovery"]
    for name,payload in (("truncated",b"FORGEF08"),("wrong-format",b"INVALID"*10),
                         ("bad-checksum",model.read_bytes()[:-1]+bytes([model.read_bytes()[-1]^1]))):
        bad=out/(name+".forge");bad.write_bytes(payload)
        p=subprocess.run([package/"bin/forge-engine","--net",bad],capture_output=True,text=True,timeout=10)
        assert p.returncode!=0 and "startup error" in p.stderr,(name,p)
    p=subprocess.run([package/"bin/forge-engine"],capture_output=True,text=True,timeout=5)
    assert p.returncode!=0 and "startup error" in p.stderr
    receipt["checks"] += ["missing/truncated/incompatible/corrupt model errors"]
    relocated=out/"relocated package with spaces"
    shutil.copytree(package,relocated,ignore=shutil.ignore_patterns(".git",".venv","build","dist","runs"))
    with Engine([relocated/"forge"],out/"relocated.log",cwd="/tmp") as e:
        e.ready();e.search(chess.STARTING_FEN,"nodes 1024")
    env=dict(os.environ,FORGE_NET=str(out/"absent.forge"))
    p=subprocess.run([relocated/"forge"],env=env,capture_output=True,text=True,timeout=5)
    assert p.returncode!=0 and "model not found" in p.stderr
    receipt["checks"] += ["unrelated working directory","relocation with spaces","launcher model error"]
    receipt["passed"]=True
    (out/"verification.json").write_text(json.dumps(receipt,indent=2)+"\n")
    return receipt

if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--package",type=Path,default=ROOT)
    ap.add_argument("--out",type=Path,required=True)
    a=ap.parse_args()
    verify(a.package.resolve(),a.out.resolve())
