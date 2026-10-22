"""Same J35 placement algebra; pooled embeddings and ragged quiet actions."""
import sys
import numpy as np
import torch
import torch.nn.functional as F
from common import J35
if str(J35) not in sys.path:sys.path.append(str(J35))
from model import Forge2,quant,trunc

class Placement(Forge2):
    def __init__(self,seed=20261021):super().__init__('placement',seed)

    def forward(self,b):
        n=len(b['ids']);weights=quant(self.placement)
        weights=torch.cat((weights,weights.new_zeros((1,128))))
        # Sum directly: no materialized [batch, 2, 64, 128] activation.
        placement=F.embedding_bag(b['ids'].reshape(2*n,32),weights,mode='sum',padding_idx=24576)
        x=torch.cat(((trunc(placement/2)+32).clamp(0,127).reshape(n,256),
                     placement.new_zeros((n,64)),b['meta']),1)
        hidden=trunc(x@quant(self.hidden)/64).clamp(0,127)
        logits=hidden@quant(self.value)/256;A,C=logits.unbind(1)
        logwdl=torch.stack((F.logsigmoid(C)+F.logsigmoid(A),F.logsigmoid(-C),
                           F.logsigmoid(C)+F.logsigmoid(-A)),1)
        query=trunc(hidden@quant(self.query)/64).clamp(-127,127)
        m=b['move_indices'];rows=b['move_rows']
        emb=F.embedding(m[:,0],quant(self.from_table))+F.embedding(m[:,1],quant(self.to_table))+F.embedding(m[:,2],quant(self.special))
        raw=(query[rows]*emb).sum(1)
        return dict(hidden=hidden,logits=logits,logwdl=logwdl,raw_policy=raw)

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
