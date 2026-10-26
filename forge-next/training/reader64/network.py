"""Two scratch-trained, integer-algebra architectures; single-head deployment."""
import numpy as np
import torch
from torch import nn
from torch.nn import functional as F

def trunc(x): return x + (torch.trunc(x) - x).detach()
def quant(x):
    b = x.clamp(-128, 127)
    return b + (torch.round(b) - b).detach()

ARCHITECTURES = dict(control=(160,32,4), wide192=(192,32,4), hidden48=(160,48,4),
                     routed8=(160,32,8), routed8_hidden48=(160,48,8), reader64=(192,64,2))

class Specialist(nn.Module):
    def __init__(self,seed=20261025,architecture='control',overlap=.25):
        super().__init__()
        self.architecture=architecture
        self.width,self.hidden_width,self.head_count=ARCHITECTURES[architecture]
        self.overlap=overlap
        self.router_temperature=1024.
        p,h,k=self.width,self.hidden_width,self.head_count
        def random(name,shape,scale=4):
            gen=torch.Generator().manual_seed(seed+sum((i+1)*ord(c) for i,c in enumerate(name)))
            return torch.randn(shape,generator=gen)*scale
        # Shared random draws for the common architecture, never trained weights.
        placement=torch.cat((random('placement',(24576,128)),random('extra_placement',(24576,32))),1)
        if p>160:placement=torch.cat((placement,random('new_placement',(24576,p-160))),1)
        self.placement=nn.Parameter(placement)
        base=random('hidden',(328,32))
        old=torch.cat((base[:128],random('extra_our',(32,32)),base[128:256],random('extra_other',(32,32)),base[320:]),0)
        ih=torch.cat((old[:160],random('new_our',(p-160,32)),old[160:320],random('new_other',(p-160,32)),old[320:]),0)
        if h>32:ih=torch.cat((ih,random('new_hidden',(2*p+8,h-32))),1)
        heads=ih[None].repeat(k,1,1)
        if k==8:
            # Distinct scratch experts avoid the zero-gradient identical-pair trap.
            heads+=random('expert_symmetry_break',heads.shape,.5)
        self.hidden=nn.Parameter(heads)
        for name,width in (('value',2),('query',16)):
            w=random(name,(32,width))
            if h>32:w=torch.cat((w,random('new_'+name,(h-32,width))),0)
            self.register_parameter(name,nn.Parameter(w[None].repeat(k,1,1)))
        for name,shape in dict(from_table=(768,16),to_table=(768,16),special=(8,16)).items():
            self.register_parameter(name,nn.Parameter(random(name,shape)))
        if k==8:
            gate=random('router',(4,p),1.)
            self.router=nn.Parameter(torch.cat((gate,-gate,torch.zeros((4,8))),1))
            self.router_bias=nn.Parameter(torch.zeros(4))

    def route(self,meta):
        bands=2 if self.head_count==2 else 4
        return ((meta[:,5].long()-1)//(32//bands)).clamp(0,bands-1)

    def forward(self,b):
        n=len(b['ids']);p=self.width;k=self.head_count;w=quant(self.placement)
        w=torch.cat((w,w.new_zeros((1,p))))
        sums=F.embedding_bag(b['ids'].reshape(2*n,32),w,mode='sum',padding_idx=24576)
        views=(trunc(sums/2)+32).clamp(0,127).reshape(n,2*p)
        x=torch.cat((views,b['meta']),1)
        all_h=trunc(torch.einsum('ni,kih->nkh',x,quant(self.hidden))/64).clamp(0,127)
        raw=torch.einsum('nkh,khv->nkv',all_h,quant(self.value))
        all_q=trunc(torch.einsum('nkh,khq->nkq',all_h,quant(self.query))/64).clamp(-127,127)
        band=self.route(b['meta']);row=torch.arange(n,device=x.device)
        route=band;out={}
        if k==8:
            bias=self.router_bias.clamp(-10000000,10000000)
            bias=bias+(bias.round()-bias).detach()
            gate=x@quant(self.router).T+bias
            soft=(gate/self.router_temperature).sigmoid()
            hard=(gate>0).to(x.dtype)
            # Hard in the forward pass (including training), soft only for gradients.
            # Parentheses retain the exact 0/1 forward values despite rounding.
            choice=hard+(soft-soft.detach())
            mix=torch.stack((1-choice,choice),-1)
            band_raw=(raw.reshape(n,4,2,2)*mix[...,None]).sum(2)
            band_q=(all_q.reshape(n,4,2,16)*mix[...,None]).sum(2)
            route=2*band+(gate[row,band]>0).long()
            out.update(gate=gate,route_soft=soft)
        else:band_raw=raw;band_q=all_q
        a,c=(band_raw/256).unbind(-1)
        all_logwdl=torch.stack((F.logsigmoid(c)+F.logsigmoid(a),F.logsigmoid(-c),F.logsigmoid(c)+F.logsigmoid(-a)),-1)
        query=band_q[row,band]
        moves=b['move_indices'];rows=b['move_rows']
        emb=F.embedding(moves[:,0],quant(self.from_table))+F.embedding(moves[:,1],quant(self.to_table))+F.embedding(moves[:,2],quant(self.special))
        out.update(hidden=all_h[row,route],all_hidden=all_h,logits=band_raw[row,band]/256,
            logwdl=all_logwdl[row,band],all_logwdl=all_logwdl,raw_policy=(query[rows]*emb).sum(1),
            query=query,route=route,band=band)
        return out

def route_weights(meta,overlap,bands=4):
    step=32//bands
    route=((meta[:,5].long()-1)//step).clamp(0,bands-1)
    weights=meta.new_zeros((len(meta),bands));weights.scatter_(1,route[:,None],1.)
    for border in range(step,32,step):
        lo=border//step-1
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
        v=(ce*route_weights(b['meta'],model.overlap,2 if model.head_count==2 else 4)).sum()
        loss=v/n+cfg['quiet_policy_weight']*p/max(1,valid)
        if 'route_soft' in out:
            mask=F.one_hot(out['band'],4).to(out['route_soft'].dtype)
            count=mask.sum(0);active=(count>0).to(count.dtype)
            fraction=(out['route_soft']*mask).sum(0)/count.clamp_min(1)
            balance=((fraction-.5).square()*active).sum()/active.sum().clamp_min(1)
            loss=loss+cfg['router_balance_weight']*balance*len(b['ids'])/n
        loss.backward()
    norm=torch.nn.utils.clip_grad_norm_(model.parameters(),cfg['gradient_clip'],error_if_nonfinite=True)
    opt.step();return norm
