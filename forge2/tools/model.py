"""Quantization-aware FORGE2, matching native integer algebra. GPL-3.0."""
import torch
from torch import nn
import torch.nn.functional as F

SHAPES = dict(placement=(32*768,128),contacts=(32256,32),local=(768,32),
              blocker=(12,32),hidden=(328,32),value=(32,2),query=(32,16),
              from_table=(768,16),to_table=(768,16),special=(8,16))

def trunc(x): return x+(torch.trunc(x)-x).detach()
def quant(x):
    bounded=x.clamp(-128,127)
    return bounded+(torch.round(bounded)-bounded).detach()

class Forge2(nn.Module):
    def __init__(self, kind='relation', seed=20261021):
        super().__init__()
        if kind not in ('placement','relation'):raise ValueError(kind)
        self.kind=kind
        # Both arms have identical initial shared parameters. The placement
        # control masks relational inputs and freezes unused tables.
        gen=torch.Generator().manual_seed(seed)
        for name,shape in SHAPES.items():
            tensor=torch.randn(shape,generator=gen)*4
            self.register_parameter(name,nn.Parameter(tensor,requires_grad=not(kind=='placement' and name in ('contacts','local','blocker'))))

    def forward(self,b):
        n=b['planes'].shape[0];occ=b['occupied'][:,None,:,None]
        codes=b['planes'];index=b['buckets'][:,:,None]*768+codes
        placement=(F.embedding(index,quant(self.placement))*occ).sum(2)
        relation=torch.zeros((n,2,32),device=placement.device)
        if self.kind=='relation':
            loc=F.embedding(codes,quant(self.local))*occ
            edge=F.embedding(b['edge_rows'],quant(self.contacts))
            edge=edge+F.embedding(b['blockers'],quant(self.blocker))*b['blocked'][...,None]
            edge=edge*b['edge_mask'][:,None,:,None]
            incoming=loc[...,:16].scatter_add(2,b['dst'][:,None,:,None].expand(-1,2,-1,16),edge[...,:16])
            outgoing=loc[...,16:].scatter_add(2,b['src'][:,None,:,None].expand(-1,2,-1,16),edge[...,16:])
            a=trunc(incoming/2).clamp(0,127);c=trunc(outgoing/2).clamp(0,127)
            relation=(torch.cat([a,trunc(a*c/128)],-1)*occ).sum(2)
        x=torch.cat([(trunc(placement/2)+32).clamp(0,127).flatten(1),
                     trunc(relation/8).clamp(0,127).flatten(1),b['meta']],1)
        hidden=trunc(x@quant(self.hidden)/64).clamp(0,127)
        logits=hidden@quant(self.value)/256
        A,C=logits.unbind(1)
        logwdl=torch.stack([F.logsigmoid(C)+F.logsigmoid(A),F.logsigmoid(-C),
                            F.logsigmoid(C)+F.logsigmoid(-A)],1)
        query=trunc(hidden@quant(self.query)/64).clamp(-127,127)
        ids=b['move_indices']
        move=F.embedding(ids[...,0],quant(self.from_table))+F.embedding(ids[...,1],quant(self.to_table))+F.embedding(ids[...,2],quant(self.special))
        raw_policy=(query[:,None,:]*move).sum(-1)
        return dict(hidden=hidden,logits=logits,logwdl=logwdl,raw_policy=raw_policy)

def loss(output,b,policy_weight=.25):
    value=-(b['wdl']*output['logwdl']).sum(1).mean()
    valid=b['policy'].sum(1)>0
    if valid.any():
        logits=(output['raw_policy'][valid]/256).masked_fill(~b['move_mask'][valid],-1e9)
        policy=-(b['policy'][valid]*logits.log_softmax(1)).sum(1).mean()
    else:policy=output['raw_policy'].sum()*0
    return value+policy_weight*policy,dict(value_ce=float(value.detach()),policy_ce=float(policy.detach()))
