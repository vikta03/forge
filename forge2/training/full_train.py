"""Full-corpus placement training; pilot warm start, full-validation selection."""
from concurrent.futures import ThreadPoolExecutor
import json
import math
from pathlib import Path
import time
import numpy as np
import torch
from common import atomic_json,digest,require_job,shards
from encode import concatenate,load_rows,select
from network import Placement,optimizer,microbatches,update
from train import atomic_checkpoint,evaluate,load_group,producer_status,wait_for

def validation_sample(paths,cfg):
    sizes=[json.loads((p/'receipt.json').read_text())['rows'] for p in paths]
    total=sum(sizes);parts=[]
    for i,(p,n) in enumerate(zip(paths,sizes)):
        count=min(n,max(1,round(cfg['validation_sample_records']*n/total)))
        rows=load_rows(p)
        index=np.random.default_rng(cfg['seed']+701+i).choice(n,count,replace=False)
        parts.append(select(rows,index))
    return concatenate(parts)

def full_validation(model,paths,micro):
    """Aggregate all validation shards without holding them all in RAM."""
    sums={k:0. for k in ('value_ce','policy_ce','brier','expected_mse')}
    count=policy_rows=games=0;game_sum=game_squares=0.;slices={}
    for path in paths:
        result=evaluate(model,load_rows(path),micro)
        n=result['rows'];g=result['games'];count+=n;policy_rows+=result['policy_rows'];games+=g
        for k in ('value_ce','brier','expected_mse'):sums[k]+=result[k]*n
        sums['policy_ce']+=result['policy_ce']*result['policy_rows']
        mean=result['game_macro_ce'];se=result['game_macro_standard_error'] or 0.
        game_sum+=g*mean;game_squares+=g*mean*mean+(g-1)*g*se*se
        for key,value in result['slices'].items():
            bucket=slices.setdefault(key,[0.,0]);m=value['rows']
            if m:bucket[0]+=value['value_ce']*m;bucket[1]+=m
    if not count:raise ValueError('No validation records')
    for k in ('value_ce','brier','expected_mse'):sums[k]/=count
    sums['policy_ce']/=max(1,policy_rows)
    variance=max(0.,(game_squares-game_sum*game_sum/max(1,games))/max(1,games-1))
    return dict(sums,rows=count,policy_rows=policy_rows,games=games,
        game_macro_ce=game_sum/max(1,games),game_macro_standard_error=math.sqrt(variance/max(1,games)),
        slices={k:dict(rows=n,value_ce=s/n if n else None) for k,(s,n) in slices.items()})

def learning_rate(cfg,epoch,epoch_seen,total,seen):
    if seen<cfg['warmup_presentations']:
        return cfg['learning_rate']*(.1+.9*seen/cfg['warmup_presentations'])
    # The first pass discovers the retained count. Start annealing only once
    # that count is known, rather than guessing a full-data epoch length.
    progress=0. if epoch==0 else min(1.,(epoch-1+epoch_seen/total)/(cfg['max_epochs']-1))
    return cfg['minimum_learning_rate']+.5*(cfg['learning_rate']-cfg['minimum_learning_rate'])*(1+math.cos(math.pi*progress))

def train(cfg,run):
    require_job();run=Path(run);out=run/'models';out.mkdir(exist_ok=True)
    checkpoint=out/'last.pt';start=time.monotonic()
    (valid_paths,_)=wait_for(run,lambda s:shards(run/'cache','valid')
        if (run/'cache/validation-ready.json').exists() else None)
    quick=validation_sample(valid_paths,cfg)
    model=Placement(cfg['seed']).cuda();micro=4096
    ck=torch.load(checkpoint,map_location='cpu',weights_only=True) if checkpoint.exists() else None
    if ck:
        if ck['config']!=cfg:raise ValueError('Resume configuration differs')
        model.load_state_dict(ck['model']);micro=ck['microbatch']
    else:
        if digest(Path(cfg['warm_start']))!=cfg['warm_start_sha256']:raise ValueError('Pilot checkpoint changed')
        warm=torch.load(cfg['warm_start'],map_location='cpu',weights_only=True)
        if warm['kind']!='placement' or warm['snapshot']!=cfg['snapshot']:raise ValueError('Incompatible warm start')
        model.load_state_dict(warm['model']);del warm
    opt=optimizer(model,cfg)
    state=ck['state'] if ck else dict(epoch=0,group=0,row=0,epoch_seen=0,seen=0,steps=0,
        best_ce=1e30,plateau_reference=1e30,stale_epochs=0,
        next_validation=cfg['validate_every_presentations'],input_wait_seconds=0.,training_seconds=0.,
        warm_start_prior_presentations=cfg['warm_start_prior_presentations'],status='running')
    if ck:
        opt.load_state_dict(ck['optimizer'])
        torch.cuda.set_rng_state(ck['cuda_rng'])
    if ck and state['status'] in ('budget_complete','validation_plateau'):
        del opt,model;torch.cuda.empty_cache();return state
    state['status']='running';last_save=time.monotonic();copy_stream=torch.cuda.Stream()
    def prepare_gpu(data,index):
        cpu,nvalid,n=microbatches(data,index,micro)
        with torch.cuda.stream(copy_stream):
            batches=[{k:v.to('cuda',non_blocking=True) for k,v in b.items()} for b in cpu]
            ready=torch.cuda.Event();ready.record(copy_stream)
        return batches,nvalid,n,cpu,ready
    def save(best=False):
        nonlocal last_save
        payload=dict(kind='placement',model=model.state_dict(),optimizer=opt.state_dict(),
            state=dict(state),config=cfg,microbatch=micro,torch_version=str(torch.__version__),
            cuda_rng=torch.cuda.get_rng_state(),snapshot=cfg['snapshot'])
        atomic_checkpoint(checkpoint,payload)
        if best:atomic_checkpoint(out/'best.pt',payload)
        atomic_json(out/'status.json',dict(state,session_seconds=time.monotonic()-start,
            microbatch=micro,effective_batch=cfg['effective_batch']))
        last_save=time.monotonic()
    def validation(full=False,epoch_end=False):
        metrics=full_validation(model,valid_paths,micro) if full else evaluate(model,quick,micro)
        improved=False
        if full:
            ce=metrics['value_ce'];improved=ce<state['best_ce']
            if improved:state['best_ce']=ce;state['best_at_presentations']=state['seen']
            if ce<state['plateau_reference']-cfg['plateau_min_delta']:
                state['plateau_reference']=ce;state['stale_epochs']=0
            elif epoch_end:state['stale_epochs']+=1
            state['last_full_validation']=metrics
        else:state['last_quick_validation']=metrics
        record=dict(presentations=state['seen'],completed_epochs=state['epoch'],metrics=metrics,
            validation_scope='all_retained_validation' if full else 'fixed_monitor_sample',best=improved,
            input_wait_seconds=state['input_wait_seconds'],training_seconds=state['training_seconds'],
            producer=producer_status(run))
        if state['training_seconds']:
            record['compute_presentations_per_second']=state['seen']/state['training_seconds']
        with (out/'history.jsonl').open('a') as f:f.write(json.dumps(record)+'\n')
        print(json.dumps(record),flush=True);save(improved)
    if not ck:validation(full=True)
    try:
        with ThreadPoolExecutor(max_workers=1) as host,ThreadPoolExecutor(max_workers=1) as loader:
            while state['epoch']<cfg['max_epochs']:
                epoch=state['epoch'];group=state['group'];prefetch=None;prefetch_members=None
                if epoch:
                    status,wait=wait_for(run,lambda s:s if s['status']=='complete' else None)
                    state['input_wait_seconds']+=wait
                    total=status['splits']['train']['rows']
                    paths=shards(run/'cache','train')
                    paths=[paths[i] for i in np.random.default_rng(cfg['seed']+epoch).permutation(len(paths))]
                else:total=None
                while True:
                    if (run/'STOP').exists():raise InterruptedError('Requested graceful stop')
                    at=group*cfg['shuffle_shards']
                    if epoch==0:
                        def ready(s):
                            p=shards(run/'cache','train')
                            return p if len(p)>=at+cfg['shuffle_shards'] or s['status']=='complete' else None
                        paths,wait=wait_for(run,ready);state['input_wait_seconds']+=wait
                    if at>=len(paths):break
                    members=paths[at:at+cfg['shuffle_shards']];waited=time.monotonic()
                    data=prefetch.result() if prefetch_members==members else load_group(members)
                    prefetch=None;prefetch_members=None
                    state['input_wait_seconds']+=time.monotonic()-waited
                    next_members=paths[at+cfg['shuffle_shards']:at+2*cfg['shuffle_shards']]
                    if next_members:prefetch=loader.submit(load_group,next_members);prefetch_members=next_members
                    order=np.random.default_rng(cfg['seed']+1000003*epoch+7919*group).permutation(len(data['ids']))
                    position=state['row'];batch=cfg['effective_batch']
                    if position<len(order):future=host.submit(prepare_gpu,data,order[position:position+batch])
                    while position<len(order):
                        if (run/'STOP').exists():raise InterruptedError('Requested graceful stop')
                        waited=time.monotonic();batches,nvalid,n,cpu,ready=future.result()
                        state['input_wait_seconds']+=time.monotonic()-waited
                        if position+n<len(order):future=host.submit(prepare_gpu,data,order[position+n:position+n+batch])
                        torch.cuda.current_stream().wait_event(ready)
                        for b in batches:
                            for t in b.values():t.record_stream(torch.cuda.current_stream())
                        lr=learning_rate(cfg,epoch,state['epoch_seen'],total or cfg['train_records'],state['seen'])
                        for g in opt.param_groups:g['lr']=lr
                        began=time.monotonic();update(model,opt,batches,nvalid,n,cfg)
                        torch.cuda.synchronize();state['training_seconds']+=time.monotonic()-began
                        position+=n;state.update(row=position,seen=state['seen']+n,steps=state['steps']+1,
                            epoch_seen=state['epoch_seen']+n,learning_rate=lr)
                        if state['seen']>=state['next_validation']:
                            state['next_validation']+=cfg['validate_every_presentations'];validation()
                        elif time.monotonic()-last_save>=cfg['checkpoint_every_seconds']:save()
                    group+=1;state.update(group=group,row=0);del data
                status=producer_status(run)
                total=status['splits']['train']['rows']
                if status['status']!='complete' or state['epoch_seen']!=total:
                    raise RuntimeError('Full-pass coverage differs from producer receipt')
                state['retained_train_records']=total
                state.update(epoch=epoch+1,group=0,row=0,epoch_seen=0)
                validation(full=True,epoch_end=True)
                if state['epoch']>=cfg['min_epochs_before_stop'] and state['stale_epochs']>=cfg['plateau_patience_epochs']:
                    state['status']='validation_plateau';save();return state
            state['status']='budget_complete';save();return state
    except InterruptedError:
        state['status']='paused';save();return state
    except BaseException:
        atomic_json(out/'failure.json',dict(last_committed_checkpoint=str(checkpoint),state=state));raise
    finally:del opt,model;torch.cuda.empty_cache()
