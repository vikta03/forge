"""Small UCI harness for the specifically approved J35 job."""
import json
from pathlib import Path
import queue
import subprocess
import threading
import time

MODEL=Path(__file__).resolve().parents[2]/'models/placement.fg2'

class Engine:
    def __init__(self,binary,log):
        self.log=Path(log).open('w');self.lines=queue.Queue()
        self.p=subprocess.Popen([str(binary),'--model',str(MODEL)],stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
        def read():
            try:
                for line in self.p.stdout:
                    self.log.write(line);self.log.flush();self.lines.put(line.rstrip())
            finally:self.lines.put(None)
        self.reader=threading.Thread(target=read,daemon=True);self.reader.start()
        try:
            self.send('uci');self.until(lambda s:s=='uciok')
            self.send('setoption name Hash value 64');self.send('setoption name DebugStats value true');self.ready()
        except BaseException:
            self.p.kill();self.p.wait();self.reader.join();self.log.close();raise

    def send(self,s):
        self.log.write('> '+s+'\n');self.log.flush()
        self.p.stdin.write(s+'\n');self.p.stdin.flush()

    def until(self,pred,timeout=30):
        deadline=time.monotonic()+timeout;lines=[]
        while True:
            item=self.lines.get(timeout=max(.001,deadline-time.monotonic()))
            if item is None:raise RuntimeError(f'Engine exited {self.p.poll()}')
            lines.append(item)
            if pred(item):return lines
            if time.monotonic()>=deadline:raise TimeoutError('UCI deadline')

    def ready(self):
        self.send('isready');return self.until(lambda s:s=='readyok')

    def load(self,path,expect_error=False):
        self.send('setoption name Forge2File value '+str(path))
        line=self.until(lambda s:s.startswith('info string FORGE2 ') or s.startswith('info string error FORGE2 '))[-1]
        if ('error FORGE2' in line)!=expect_error:raise RuntimeError(line)
        self.ready();return line

    def inspect(self,fen):
        self.send('setoption name UCI_Chess960 value true')
        self.send('position fen '+fen);self.send('forge2inspect')
        return json.loads(self.until(lambda s:s.startswith('forge2inspect '))[-1].split(' ',1)[1])

    def evaluate(self,fen):
        self.send('position fen '+fen);self.send('eval')
        return int(self.until(lambda s:s.startswith('static eval:'))[-1].split()[2])

    def search(self,fen,nodes=8192):
        self.send('setoption name UCI_Chess960 value false');self.send('ucinewgame')
        self.send('position fen '+fen);self.ready()
        started=time.perf_counter();self.send(f'go nodes {nodes}')
        lines=self.until(lambda s:s.startswith('bestmove '));elapsed=time.perf_counter()-started
        info=next(s for s in reversed(lines) if s.startswith('info depth '))
        fields=info.split();get=lambda k:int(fields[fields.index(k)+1])
        telemetry=[json.loads(s.split('forge2probe ',1)[1]) for s in lines if 'forge2probe {' in s]
        # J35 patch prints the actual final visited-node count. Completed
        # iteration nodes do not include the partially searched final depth.
        final=next(int(s.split()[-1]) for s in lines if s.startswith('info string forge2nodes '))
        return dict(wall_seconds=elapsed,actual_nodes=final,wall_nps=final/elapsed,
                    completed_iteration=dict(nodes=get('nodes'),nps=get('nps'),time_ms=get('time'),
                        depth=get('depth'),info=info),bestmove=lines[-1].split()[1],
                    probe=telemetry[-1] if telemetry else None)

    def close(self):
        try:
            if self.p.poll() is None:self.send('stop');self.send('quit');self.p.wait(timeout=5)
        finally:
            if self.p.poll() is None:self.p.kill();self.p.wait()
            self.reader.join(timeout=2);self.log.close()
        if self.p.returncode!=0:raise RuntimeError('Engine exit '+str(self.p.returncode))
