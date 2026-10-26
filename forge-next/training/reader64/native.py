"""J68 model transformations and independent integer oracle; no work at import."""
import importlib.util
import json
import struct
import zlib
from pathlib import Path
import numpy as np
from common import sha, save
from engine_client import cp_from_raw

MAGIC = b'FGCP681\0'
HEAD_KEYS = ('hidden', 'value', 'query')
TAIL_KEYS = ('from_table', 'to_table', 'special')

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    obj = importlib.util.module_from_spec(spec); spec.loader.exec_module(obj)
    return obj

def dimensions(w):
    p = w['placement'].shape[1]
    k, inputs, h = w['hidden'].shape
    assert p in (160, 192) and h in (32, 48, 64) and k in (2, 4, 8)
    assert inputs == 2*p+8
    shapes = dict(placement=(24576,p), hidden=(k,inputs,h), value=(k,h,2),
                  query=(k,h,16), from_table=(768,16), to_table=(768,16), special=(8,16))
    if k == 8: shapes.update(router=(4,inputs), router_bias=(4,))
    assert set(w) == set(shapes)
    for name, shape in shapes.items():
        assert w[name].shape == shape, (name,w[name].shape,shape)
        assert w[name].dtype == (np.dtype('<i4') if name == 'router_bias' else np.dtype('int8'))
    if k == 8: assert np.max(np.abs(w['router_bias'].astype(np.int64))) <= 10000000
    return p,h,k

def body(w):
    p,h,k = dimensions(w)
    parts = [w['placement'].tobytes()]
    for i in range(k): parts.extend(w[key][i].tobytes() for key in HEAD_KEYS)
    parts.extend(w[key].tobytes() for key in TAIL_KEYS)
    if k == 8: parts.extend((w['router'].tobytes(), w['router_bias'].tobytes()))
    return b''.join(parts)

def read_parent(export, deployed):
    export = Path(export); receipt = json.loads(export.with_suffix('.json').read_text())
    assert sha(export) == receipt['sha256'] and receipt['architecture'] == 'wide160'
    with np.load(export, allow_pickle=False) as data: w={k:data[k] for k in data.files}
    assert dimensions(w) == (160,32,4)
    assert {k:list(a.shape) for k,a in w.items()} == receipt['shapes']
    payload = body(w)
    old = b'FGCAP01\0'+struct.pack('<6I',1,1,160,len(payload),zlib.crc32(payload),4)+payload
    assert old == Path(deployed).read_bytes(), 'Training export differs from installed model'
    return w

def expand(parent, p, h, k):
    assert dimensions(parent) == (160,32,4)
    rng = np.random.default_rng(680068)
    w = {name:parent[name].copy() for name in TAIL_KEYS}
    w['placement'] = rng.integers(-8,9,(24576,p),dtype=np.int8)
    w['placement'][:,:160] = parent['placement']
    w['hidden'] = rng.integers(-8,9,(k,2*p+8,h),dtype=np.int8)
    w['value'] = np.zeros((k,h,2),np.int8)
    w['query'] = np.zeros((k,h,16),np.int8)
    for expert in range(k):
        src = expert if k == 4 else expert//2
        w['hidden'][expert,:,:32] = 0
        w['hidden'][expert,:160,:32] = parent['hidden'][src,:160]
        w['hidden'][expert,p:p+160,:32] = parent['hidden'][src,160:320]
        w['hidden'][expert,2*p:,:32] = parent['hidden'][src,320:]
        w['value'][expert,:32] = parent['value'][src]
        w['query'][expert,:32] = parent['query'][src]
    if k == 8:
        # Opposed random view coefficients limit constant-offset dominance;
        # nonzero state coefficients exercise clock/state-dependent routing.
        v = rng.integers(-8,9,(4,p),dtype=np.int8)
        w['router'] = np.concatenate((v,-v,rng.integers(-2,3,(4,8),dtype=np.int8)),axis=1)
        w['router_bias'] = np.zeros(4,dtype='<i4')
    assert dimensions(w) == (p,h,k)
    return w

def write_model(w, path, label):
    path = Path(path); p,h,k = dimensions(w); payload = body(w)
    header = MAGIC + struct.pack('<8I',1,p,h,k,2*p+8,int(k==8),len(payload),zlib.crc32(payload))
    path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(header+payload)
    receipt = dict(label=label,format='FGCP681',placement=p,hidden=h,heads=k,
                   bytes=len(header)+len(payload),sha256=sha(path),trained=False,
                   function_preserving=label=='cost',promoted=False)
    save(path.with_suffix(path.suffix+'.json'),receipt)
    return receipt

def read_model(path):
    blob=Path(path).read_bytes(); assert blob[:8] == MAGIC and len(blob) >= 40
    version,p,h,k,inputs,route,size,crc=struct.unpack('<8I',blob[8:40])
    assert version==1 and inputs==2*p+8 and route==int(k==8)
    assert len(blob)==size+40 and zlib.crc32(blob[40:])==crc
    at=40
    def take(shape,dtype=np.int8):
        nonlocal at
        n=int(np.prod(shape)); a=np.frombuffer(blob,dtype,n,at).reshape(shape).copy()
        at+=n*np.dtype(dtype).itemsize; return a
    w={'placement':take((24576,p))}
    for key in HEAD_KEYS: w[key]=[]
    for _ in range(k):
        w['hidden'].append(take((inputs,h)))
        w['value'].append(take((h,2)));w['query'].append(take((h,16)))
    for key in HEAD_KEYS:w[key]=np.stack(w[key])
    for key,shape in [('from_table',(768,16)),('to_table',(768,16)),('special',(8,16))]:w[key]=take(shape)
    if k==8:w['router']=take((4,inputs));w['router_bias']=take((4,),'<i4')
    assert at==len(blob);dimensions(w);return w

def trunc(x,d): return np.sign(x)*(np.abs(x)//d)

def input_vector(w,ids,meta):
    p=w['placement'].shape[1]
    # Do not copy/convert the full table per board. Only referenced rows expand.
    sums=np.stack([w['placement'][view[view!=24576]].astype(np.int32).sum(0) for view in ids])
    return np.concatenate((np.clip(trunc(sums,2)+32,0,127).reshape(2*p),meta)).astype(np.int32)

def forward_x(w,x,meta):
    k=w['hidden'].shape[0]; gate=None
    bands=2 if k==2 else 4
    band=int(np.clip((int(meta[5])-1)//(32//bands),0,bands-1))
    if k==8:
        gate=int(x.astype(np.int64)@w['router'][band].astype(np.int64))+int(w['router_bias'][band])
    selected=band if k!=8 else 2*band+int(gate>0)
    hidden=np.clip(trunc(x@w['hidden'][selected].astype(np.int32),64),0,127)
    value=hidden@w['value'][selected].astype(np.int32)
    query=np.clip(trunc(hidden@w['query'][selected].astype(np.int32),64),-127,127)
    return selected,hidden,value,query,gate

def oracle(w,fen,encoder):
    ids,meta,moves=encoder.fen_inputs(fen);x=input_vector(w,ids,meta)
    selected,hidden,value,query,gate=forward_x(w,x,meta)
    quiet={u:int(query@(w['from_table'][a].astype(np.int32)+w['to_table'][b].astype(np.int32)+w['special'][s].astype(np.int32)))
           for u,(a,b,s) in moves.items()}
    return dict(selected_head=selected,hidden=hidden.tolist(),raw_value_logits=value.tolist(),
                score=cp_from_raw(value),quiet_policy=quiet),gate

def sentinel(w):
    """Different experts; state-driven +/-/zero routes, never used in timing."""
    out={k:v.copy() for k,v in w.items()}; p,h,k=dimensions(out)
    out['hidden'].fill(0);out['value'].fill(0);out['query'].fill(0)
    # x[-1]=32 makes h[0]=1..8; downstream tensors differ too.
    for e in range(k):
        out['hidden'][e,-1,0]=2*(e+1)
        out['value'][e,0]=[31+e,17-e]
        out['query'][e,0]=np.arange(16,dtype=np.int8)+8+e
        if p==192:
            out['hidden'][e,160,0]=9
            out['hidden'][e,p+160,0]=-7
        if h>32:
            out['hidden'][e,-1,h-1]=20+e
            out['value'][e,h-1]=[17,-3]
            out['query'][e,h-1]=np.arange(16,dtype=np.int8)+16
    if k==8:
        out['router'].fill(0);out['router'][:,2*p]=1
        out['router_bias'][:]=-16
    return out
