"""Two scratch-trained, integer-algebra architectures; single-head deployment."""
import numpy as np
import torch
from torch import nn
from torch.nn import functional as F

def trunc(x): return x + (torch.trunc(x) - x).detach()
def quant(x):
    b = x.clamp(-128, 127)
    return b + (torch.round(b) - b).detach()

class Specialist(nn.Module):
    def __init__(self, seed=20261024, architecture='wide160', overlap=.25):
        super().__init__()
        if architecture not in ('wide160', 'deep32', 'baseline'): raise ValueError(architecture)
        self.architecture = architecture
        self.width = 160 if architecture == 'wide160' else 128
        self.overlap = overlap
        self.head_count = 4
        def random(name, shape, scale=4):
            gen = torch.Generator().manual_seed(seed + sum((i+1)*ord(c) for i,c in enumerate(name)))
            return torch.randn(shape, generator=gen) * scale
        # Same original 128 features in both random initializations; no trained parent.
        p = random('placement', (24576,128))
        if self.width == 160: p = torch.cat((p,random('extra_placement',(24576,32))),1)
        self.placement = nn.Parameter(p)
        h = random('hidden', (328,32))
        if self.width == 160:
            h = torch.cat((h[:128],random('extra_our',(32,32)),h[128:256],
                           random('extra_other',(32,32)),h[320:]),0)
        self.hidden = nn.Parameter(h[None].repeat(4,1,1))
        if architecture == 'deep32':
            # Untrained identity-centered initialization keeps early gradients alive.
            d = 64*torch.eye(32) + random('deep',(32,32),.5)
            self.deep = nn.Parameter(d[None].repeat(4,1,1))
        for name,shape in dict(value=(32,2),query=(32,16),from_table=(768,16),
                               to_table=(768,16),special=(8,16)).items():
            weight = random(name,shape)
            if name in ('value','query'): weight=weight[None].repeat(4,1,1)
            self.register_parameter(name,nn.Parameter(weight))

    def route(self,meta): return ((meta[:,5].long()-1)//8).clamp(0,3)

    def forward(self,b):
        n=len(b['ids']);w=quant(self.placement)
        w=torch.cat((w,w.new_zeros((1,self.width))))
        p=F.embedding_bag(b['ids'].reshape(2*n,32),w,mode='sum',padding_idx=24576)
        views=(trunc(p/2)+32).clamp(0,127).reshape(n,2*self.width)
        x=torch.cat((views,p.new_zeros((n,320-2*self.width)),b['meta']),1)
        all_h=trunc(torch.einsum('ni,kih->nkh',x,quant(self.hidden))/64).clamp(0,127)
        if self.architecture=='deep32':
            all_h=trunc(torch.einsum('nki,kih->nkh',all_h,quant(self.deep))/64).clamp(0,127)
        raw=torch.einsum('nkh,khv->nkv',all_h,quant(self.value))
        a,c=(raw/256).unbind(-1)
        all_logwdl=torch.stack((F.logsigmoid(c)+F.logsigmoid(a),F.logsigmoid(-c),
                                F.logsigmoid(c)+F.logsigmoid(-a)),-1)
        route=self.route(b['meta']);row=torch.arange(n,device=p.device)
        h=all_h[row,route]
        query=trunc(torch.einsum('nh,nhq->nq',h,quant(self.query)[route])/64).clamp(-127,127)
        moves=b['move_indices'];rows=b['move_rows']
        emb=F.embedding(moves[:,0],quant(self.from_table))+F.embedding(moves[:,1],quant(self.to_table))+F.embedding(moves[:,2],quant(self.special))
        raw_policy=(query[rows]*emb).sum(1)
        return dict(hidden=h,all_hidden=all_h,logits=raw[row,route]/256,
                    logwdl=all_logwdl[row,route],all_logwdl=all_logwdl,
                    raw_policy=raw_policy,query=query,route=route)

def route_weights(meta,overlap):
    route=((meta[:,5].long()-1)//8).clamp(0,3)
    weights=meta.new_zeros((len(meta),4));weights.scatter_(1,route[:,None],1.)
    for border in (8,16,24):
        lo=border//8-1
        weights[:,lo+1]+=(meta[:,5]==border)*overlap
        weights[:,lo]+=(meta[:,5]==border+1)*overlap
    return weights/weights.sum(1,keepdim=True)

def loss_sums(out,b):
    """Metrics always use the deployed route, never an ensemble/overlap loss."""
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
    torch.set_num_threads(1);torch.set_num_interop_threads(1);torch.manual_seed(cfg['seed'])
    torch.backends.cuda.matmul.allow_tf32=False;torch.backends.cudnn.allow_tf32=False
    if not torch.cuda.is_available():raise RuntimeError('CUDA unavailable outside sandbox')
    total=torch.cuda.get_device_properties(0).total_memory
    torch.cuda.set_per_process_memory_fraction(min(.85,cfg['gpu_allocator_bytes']/total),0)

def optimizer(model,cfg):
    return torch.optim.AdamW(model.parameters(),lr=cfg['learning_rate'],weight_decay=cfg['weight_decay'],fused=True)

def microbatches(rows,indices,micro,pin=True):
    result=[];valid=0
    for start in range(0,len(indices),micro):
        batch,count=to_cpu_batch(rows,indices[start:start+micro],pin)
        result.append(batch);valid+=count
    return result,valid,len(indices)

def update(model,opt,batches,valid,n,cfg):
    opt.zero_grad(set_to_none=True)
    for b in batches:
        out=model(b);_,p=loss_sums(out,b)
        ce=-(b['wdl'][:,None,:]*out['all_logwdl']).sum(-1)
        v=(ce*route_weights(b['meta'],model.overlap)).sum()
        (v/n+cfg['quiet_policy_weight']*p/max(1,valid)).backward()
    norm=torch.nn.utils.clip_grad_norm_(model.parameters(),cfg['gradient_clip'],error_if_nonfinite=True)
    opt.step();return norm
