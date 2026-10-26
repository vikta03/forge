"""Integer exports with complete dimensions and an independent NumPy forward."""
import json
from pathlib import Path
import numpy as np
from common import atomic_json,digest
from native import dimensions,write_model,input_vector,trunc

def export(model,path):
    path=Path(path);arrays={}
    for k,v in model.state_dict().items():
        x=v.detach().cpu().numpy();assert np.isfinite(x).all()
        arrays[k]=np.rint(np.clip(x,-10000000,10000000)).astype('<i4') if k=='router_bias' else np.rint(np.clip(x,-128,127)).astype(np.int8)
    p,h,k=dimensions(arrays);np.savez_compressed(path,**arrays)
    receipt=dict(architecture=model.architecture,placement_width=p,hidden_width=h,heads=k,
        parameters=sum(v.size for v in arrays.values()),shapes={k:list(v.shape) for k,v in arrays.items()},
        sha256=digest(path),bytes=path.stat().st_size,format='FGCP681-int8-npz',
        search_tested=False,hard_forward_training=True)
    atomic_json(path.with_suffix('.json'),receipt)
    return receipt

def read(path):
    path=Path(path);receipt=json.loads(path.with_suffix('.json').read_text());assert digest(path)==receipt['sha256']
    with np.load(path,allow_pickle=False) as z:w={k:z[k] for k in z.files}
    dimensions(w);assert {k:list(v.shape) for k,v in w.items()}==receipt['shapes']
    return w,receipt

def integer_forward(w,rows,architecture=None):
    p,h,k=dimensions(w);n=len(rows['ids'])
    x=np.stack([input_vector(w,ids,meta) for ids,meta in zip(rows['ids'],rows['meta'])])
    hidden=[];raw=[];query=[]
    for e in range(k):
        a=np.clip(trunc(x@w['hidden'][e].astype(np.int32),64),0,127)
        hidden.append(a);raw.append(a@w['value'][e].astype(np.int32))
        query.append(np.clip(trunc(a@w['query'][e].astype(np.int32),64),-127,127))
    hidden=np.stack(hidden,1);raw=np.stack(raw,1);query=np.stack(query,1)
    band=((rows['meta'][:,5].astype(int)-1)//8).clip(0,3);idx=np.arange(n)
    if k==8:
        gate=x@w['router'].astype(np.int32).T+w['router_bias']
        expert=2*np.arange(4)[None,:]+(gate>0).astype(int)
        band_raw=raw[idx[:,None],expert];route=expert[idx,band]
    else:band_raw=raw;route=band
    q=query[idx,route];moves=rows['moves'];mr=np.repeat(idx,np.diff(rows['offsets']))
    emb=sum(w[key][moves[field]].astype(np.int32) for key,field in [('from_table','from'),('to_table','to'),('special','kind')])
    a=band_raw[...,0]/256;c=band_raw[...,1]/256;ls=lambda z:-np.logaddexp(0,-z)
    probs=np.exp(np.stack((ls(c)+ls(a),ls(-c),ls(c)+ls(-a)),-1))
    return dict(hidden=hidden[idx,route],all_hidden=hidden,raw=raw[idx,route],query=q,
        raw_policy=(emb*q[mr]).sum(1),all_wdl=probs,route=route)
