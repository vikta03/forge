"""Research int8 exports with explicit architecture; not legacy FG2 binaries."""
import json
from pathlib import Path
import numpy as np
from common import atomic_json,digest

def export(model,path):
    path=Path(path)
    arrays={k:np.clip(np.rint(v.detach().cpu().numpy()),-128,127).astype(np.int8)
            for k,v in model.state_dict().items()}
    assert all(np.isfinite(v.detach().cpu().numpy()).all() for v in model.state_dict().values())
    np.savez_compressed(path,**arrays)
    receipt=dict(format='FORGE-capacity-research-int8-v1',architecture=model.architecture,
        placement_width=model.width,heads=4,shapes={k:list(v.shape) for k,v in arrays.items()},
        parameters=sum(v.size for v in arrays.values()),sha256=digest(path),bytes=path.stat().st_size,
        legacy_engine_compatible=False,search_tested=False,
        algebra='placement trunc(sum/2)+32 clip[0,127]; hidden/deep trunc(dot/64) clip[0,127]; value dot/256; query trunc(dot/64) clip[-127,127]; policy dot/256')
    atomic_json(path.with_suffix('.json'),receipt)
    return receipt

def read(path):
    path=Path(path);receipt=json.loads(path.with_suffix('.json').read_text())
    assert digest(path)==receipt['sha256']
    with np.load(path,allow_pickle=False) as z: w={k:z[k].astype(np.int32) for k in z.files}
    assert {k:list(v.shape) for k,v in w.items()}==receipt['shapes']
    return w,receipt

def trunc_int(x,d): return np.sign(x)*(np.abs(x)//d)

def integer_forward(w,rows,architecture):
    """Independent NumPy arithmetic, no PyTorch/model.forward reuse."""
    width=w['placement'].shape[1];n=len(rows['ids'])
    table=np.concatenate((w['placement'],np.zeros((1,width),np.int32)))
    accum=table[rows['ids']].sum(2,dtype=np.int32)
    views=np.clip(trunc_int(accum,2)+32,0,127).reshape(n,2*width)
    x=np.concatenate((views,np.zeros((n,320-2*width),np.int32),rows['meta'].astype(np.int32)),1)
    hidden=[];raw=[];query=[]
    for k in range(4):
        h=np.clip(trunc_int(x@w['hidden'][k],64),0,127)
        if architecture=='deep32':h=np.clip(trunc_int(h@w['deep'][k],64),0,127)
        hidden.append(h);raw.append(h@w['value'][k])
        query.append(np.clip(trunc_int(h@w['query'][k],64),-127,127))
    route=((rows['meta'][:,5].astype(int)-1)//8).clip(0,3);index=np.arange(n)
    h=np.stack(hidden,1);v=np.stack(raw,1);q=np.stack(query,1)[index,route]
    moves=rows['moves'];move_rows=np.repeat(index,np.diff(rows['offsets']))
    emb=w['from_table'][moves['from']]+w['to_table'][moves['to']]+w['special'][moves['kind']]
    policy=(emb*q[move_rows]).sum(1)
    logits=v.astype(float)/256;a=logits[...,0];c=logits[...,1]
    log_sig=lambda z:-np.logaddexp(0,-z)
    logwdl=np.stack((log_sig(c)+log_sig(a),log_sig(-c),log_sig(c)+log_sig(-a)),-1)
    return dict(hidden=h[index,route],all_hidden=h,raw=v[index,route],query=q,
                raw_policy=policy,all_wdl=np.exp(logwdl),route=route)
