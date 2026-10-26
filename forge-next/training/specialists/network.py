"""FORGE four selected heads, initialized entirely from the fixed random seed."""
import sys
from common import J35
if str(J35) not in sys.path: sys.path.append(str(J35))
import torch
import numpy as np
from torch import nn
from torch.nn import functional as F
from model import Forge2, quant, trunc

class Specialist(Forge2):
    def __init__(self, seed=20261022, heads=4):
        if heads not in (1,4): raise ValueError('Unsupported head count')
        super().__init__('placement',seed)
        self.arm='specialists' if heads==4 else 'control'
        self.head_count=heads
        if heads==4:
            # Equal RANDOM initial stacks; no pretrained weights or optimizer.
            # Separate parameter slices receive gradients from separate routes.
            for name in ('hidden','value','query'):
                weight=getattr(self,name).detach()
                setattr(self,name,nn.Parameter(weight.unsqueeze(0).repeat(4,1,1)))

    def merged_placement(self): return self.placement
    def route(self,meta): return ((meta[:,5].long()-1)//8).clamp(0,3)

    def forward(self, b):
        n = len(b['ids'])
        w = quant(self.merged_placement())
        w = torch.cat((w, w.new_zeros((1,128))))
        p = F.embedding_bag(b['ids'].reshape(2*n,32), w, mode='sum', padding_idx=24576)
        x = torch.cat(((trunc(p/2)+32).clamp(0,127).reshape(n,256),
                       p.new_zeros((n,64)), b['meta']),1)
        if self.head_count == 1:
            h = trunc(x @ quant(self.hidden)/64).clamp(0,127)
            logits = h @ quant(self.value)/256
            query = trunc(h @ quant(self.query)/64).clamp(-127,127)
        else:
            # Trainer may compute all small heads; native inference selects ONE.
            # Avoid materializing a [batch,328,32] weight gather for every sample.
            route = self.route(b['meta'])
            all_h = trunc(torch.einsum('ni,kih->nkh',x,quant(self.hidden))/64).clamp(0,127)
            h = all_h[torch.arange(n,device=x.device),route]
            logits = torch.einsum('nh,nhv->nv',h,quant(self.value)[route])/256
            query = trunc(torch.einsum('nh,nhq->nq',h,quant(self.query)[route])/64).clamp(-127,127)
        a,c = logits.unbind(1)
        logwdl = torch.stack((F.logsigmoid(c)+F.logsigmoid(a),F.logsigmoid(-c),
                             F.logsigmoid(c)+F.logsigmoid(-a)),1)
        moves = b['move_indices']; rows = b['move_rows']
        emb = F.embedding(moves[:,0],quant(self.from_table)) + F.embedding(moves[:,1],quant(self.to_table)) + F.embedding(moves[:,2],quant(self.special))
        raw = (query[rows]*emb).sum(1)
        return dict(hidden=h,logits=logits,logwdl=logwdl,raw_policy=raw)

def loss_sums(out,b):
    n=len(b['ids']);rows=b['move_rows'];logits=out['raw_policy']/256
    maximum=logits.new_full((n,),-1e30)
    maximum.scatter_reduce_(0,rows,logits.detach(),reduce='amax',include_self=True)
    shifted=logits-maximum[rows]
    normalizer=logits.new_zeros(n).scatter_add_(0,rows,shifted.exp()).clamp_min(1e-30)
    logp=shifted-normalizer[rows].log()
    return -(b['wdl']*out['logwdl']).sum(),-(b['policy']*logp).sum()

def to_cpu_batch(rows,index,pin=False):
    from encode import select
    data=select(rows,index);n=len(index);counts=np.diff(data['offsets']).astype(np.int64)
    move_rows=np.repeat(np.arange(n),counts);moves=data['moves']
    arrays=dict(ids=data['ids'].astype(np.int64),meta=data['meta'].astype(np.float32),
        wdl=data['wdl'],move_rows=move_rows,
        move_indices=np.column_stack((moves['from'],moves['to'],moves['kind'])).astype(np.int64),
        policy=moves['prob'].astype(np.float32))
    valid=int((np.bincount(move_rows,weights=moves['prob'],minlength=n)>0).sum())
    tensors={k:torch.from_numpy(np.ascontiguousarray(v)) for k,v in arrays.items()}
    return ({k:t.pin_memory() for k,t in tensors.items()} if pin else tensors),valid

def configure(cfg):
    torch.set_num_threads(1);torch.set_num_interop_threads(1)
    torch.manual_seed(cfg['seed'])
    torch.backends.cuda.matmul.allow_tf32=False;torch.backends.cudnn.allow_tf32=False
    if not torch.cuda.is_available():raise RuntimeError('CUDA not available')
    total=torch.cuda.get_device_properties(0).total_memory
    torch.cuda.set_per_process_memory_fraction(min(.85,cfg['gpu_allocator_bytes']/total),0)

def optimizer(model,cfg):
    return torch.optim.AdamW([p for p in model.parameters() if p.requires_grad],
        lr=cfg['learning_rate'],weight_decay=cfg['weight_decay'],fused=True)

def microbatches(rows,indices,micro,pin=True):
    result=[];valid=0
    for start in range(0,len(indices),micro):
        batch,count=to_cpu_batch(rows,indices[start:start+micro],pin)
        result.append(batch);valid+=count
    return result,valid,len(indices)

def update(model,opt,batches,valid,n,cfg):
    opt.zero_grad(set_to_none=True)
    for batch in batches:
        v,p=loss_sums(model(batch),batch)
        loss=v/n+cfg['quiet_policy_weight']*p/max(1,valid)
        loss.backward()
    norm=torch.nn.utils.clip_grad_norm_(model.parameters(),cfg['gradient_clip'],error_if_nonfinite=True)
    opt.step();return norm
