"""Cached-data loading and validation reused from the verified J38 trainer."""
from concurrent.futures import ThreadPoolExecutor
import json
import math
import os
from pathlib import Path
import time
import numpy as np
import torch
from common import atomic_json,require_job,shards,cache_root
from encode import concatenate,load_rows
from network import Specialist,optimizer,microbatches,update,to_cpu_batch,loss_sums

def atomic_checkpoint(path,obj):
    path=Path(path);tmp=path.with_name(path.name+'.tmp')
    with tmp.open('wb') as f:torch.save(obj,f);f.flush();os.fsync(f.fileno())
    os.replace(tmp,path)

def producer_status(run):
    path=cache_root(run)/'producer.json'
    return json.loads(path.read_text()) if path.exists() else {'status':'starting'}

def wait_for(run,predicate):
    # This is a worker's bounded backpressure wait, not a conversational poll.
    started=time.monotonic()
    while True:
        if (Path(run)/'STOP').exists():raise InterruptedError('Requested graceful stop')
        status=producer_status(run)
        if status['status']=='failed':raise RuntimeError('Data producer failed: '+status.get('error',''))
        result=predicate(status)
        if result:return result,time.monotonic()-started
        time.sleep(.15)

def load_group(paths):return concatenate([load_rows(p) for p in paths])

@torch.no_grad()
def evaluate(model,rows,batch=4096):
    totals=dict(value_ce=0.,policy_ce=0.,brier=0.,expected_mse=0.,expected_mae=0.,wdl_kl=0.,policy_rows=0,rows=0)
    slices={};game_errors={}
    for start in range(0,len(rows['ids']),batch):
        b,valid=to_cpu_batch(rows,np.arange(start,min(start+batch,len(rows['ids']))),True)
        b={k:v.to('cuda',non_blocking=True) for k,v in b.items()};o=model(b);v,p=loss_sums(o,b)
        totals['rows']+=len(b['ids']);totals['policy_rows']+=valid
        totals['value_ce']+=float(v);totals['policy_ce']+=float(p)
        pred=o['logwdl'].exp();totals['brier']+=float(((pred-b['wdl'])**2).sum())
        delta=(pred[:,0]+.5*pred[:,1])-(b['wdl'][:,0]+.5*b['wdl'][:,1])
        totals['expected_mse']+=float(delta.square().sum());totals['expected_mae']+=float(delta.abs().sum())
        totals['wdl_kl']+=float((b['wdl']*(b['wdl'].clamp_min(1e-30).log()-o['logwdl'])).sum())
        error=(-(b['wdl']*o['logwdl']).sum(1)).cpu().numpy()
        meta=rows['meta'][start:start+len(error)];refs=rows['refs'][start:start+len(error)]
        masks={**{f'head_{h}':np.minimum(3,(meta[:,5].astype(int)-1)//8)==h for h in range(4)},'pieces_2_to_7':meta[:,5]<=7,'pieces_8_to_16':(meta[:,5]>=8)&(meta[:,5]<=16),
            'pieces_17_plus':meta[:,5]>=17,'clock_0_to_19':meta[:,0]<10,
            'clock_20_to_59':(meta[:,0]>=10)&(meta[:,0]<30),'clock_60_to_99':meta[:,0]>=30,
            'phase_0_to_4':meta[:,1]<=4,'phase_5_to_12':(meta[:,1]>4)&(meta[:,1]<=12),
            'phase_13_plus':meta[:,1]>12}
        for name,mask in masks.items():
            s=slices.setdefault(name,[0.,0]);s[0]+=float(error[mask].sum());s[1]+=int(mask.sum())
        absolute=delta.abs().cpu().numpy()
        for gid,e,a in zip(refs[:,0].tolist(),error.tolist(),absolute.tolist()):
            s=game_errors.setdefault(gid,[0.,0,0.]);s[0]+=e;s[1]+=1;s[2]+=a
    for name in ('value_ce','brier','expected_mse','expected_mae','wdl_kl'):totals[name]/=totals['rows']
    totals['policy_ce']/=max(1,totals['policy_rows'])
    if not all(math.isfinite(float(x)) for x in totals.values()):raise ValueError('Nonfinite validation')
    totals['slices']={k:dict(rows=n,value_ce=s/n if n else None) for k,(s,n) in slices.items()}
    game_means=np.array([s/n for s,n,a in game_errors.values()])
    totals['game_macro_ce']=float(game_means.mean());totals['games']=len(game_means)
    totals['game_macro_standard_error']=float(game_means.std(ddof=1)/np.sqrt(len(game_means))) if len(game_means)>1 else None
    totals['game_statistics']={str(g):v for g,v in game_errors.items()}
    return totals
