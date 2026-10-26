"""Shared paths and atomic receipts. GPL-3.0."""
import hashlib
import json
import os
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
WS = HERE.parents[2]
J35 = HERE.parent / 'reference'

def digest(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def atomic_json(path,obj):
    path=Path(path);tmp=path.with_name(path.name+'.tmp')
    with tmp.open('w') as f:
        json.dump(obj,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
    os.replace(tmp,path)

def config(): return json.loads((HERE/'config.json').read_text())

def shards(cache,split):
    return sorted(p for p in Path(cache).iterdir() if p.is_dir() and
        re.fullmatch(re.escape(split)+r'-[0-9]{5}',p.name) and (p/'receipt.json').is_file())

def require_job():
    if os.environ.get('FORGE_TRAINING_ALLOWED')!='1':
        raise RuntimeError('Set FORGE_TRAINING_ALLOWED=1 after arranging resource supervision')


def cache_root(run):
    marker=Path(run)/'cache-source.txt'
    return Path(marker.read_text().strip()) if marker.exists() else Path(run)/'cache'

sha=digest
save=atomic_json
