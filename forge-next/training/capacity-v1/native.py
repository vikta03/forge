"""Explicit capacity format export and independent integer reference."""
import importlib.util,json,struct,zlib
from pathlib import Path
import numpy as np
from common import save,sha
from engine_client import cp_from_raw

def module(name,path):
    s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def export(source,destination,architecture):
    receipt=json.loads(source.with_suffix('.json').read_text())
    assert sha(source)==receipt['sha256'] and receipt['architecture']==architecture
    with np.load(source,allow_pickle=False) as z:w={k:z[k] for k in z.files}
    assert all(a.dtype==np.int8 for a in w.values())
    assert {k:list(a.shape) for k,a in w.items()}==receipt['shapes']
    payload=[w['placement'].tobytes()]
    for h in range(4):
        payload.append(w['hidden'][h].tobytes())
        if architecture=='deep32':payload.append(w['deep'][h].tobytes())
        payload.extend(w[k][h].tobytes() for k in ('value','query'))
    payload.extend(w[k].tobytes() for k in ('from_table','to_table','special'))
    body=b''.join(payload);assert len(body)==receipt['parameters']
    header=b'FGCAP01\0'+struct.pack('<6I',1,1 if architecture=='wide160' else 2,
        w['placement'].shape[1],len(body),zlib.crc32(body),4)
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(header+body)
    out=dict(source=str(source),source_sha256=receipt['sha256'],architecture=architecture,
        native_sha256=sha(destination),native_bytes=destination.stat().st_size,parameters=len(body),
        format='FGCAP01',weights_unchanged=True)
    save(destination.with_suffix('.json'),out);return out

def oracle(w,architecture,fen,encoder,serialization):
    ids,meta,moves=encoder.fen_inputs(fen);entries=list(moves.values())
    dt=np.dtype([('from','<i4'),('to','<i4'),('kind','<i4')])
    row=dict(ids=ids[None],meta=meta[None],moves=np.array(entries,dtype=dt),offsets=np.array([0,len(entries)]))
    o=serialization.integer_forward(w,row,architecture)
    raw=o['raw'][0].tolist()
    return dict(hidden=o['hidden'][0].tolist(),raw_value_logits=raw,score=cp_from_raw(raw),
        quiet_policy={u:int(p) for u,p in zip(moves,o['raw_policy'])},head=int(o['route'][0]))
