"""Lean raw -> placement/quiet-policy encoding; never expand history tensors."""
import ctypes
from compression import zstd
import io
import hashlib
from pathlib import Path
import sys
import numpy as np
from common import J35
if str(J35) not in sys.path:sys.path.append(str(J35))
from codec.raw import MOVE_BASE, wdl

MOVE_DTYPE=np.dtype([('from','<u2'),('to','<u2'),('kind','u1'),('prob','<f4')])
REVERSE=np.array([int(f'{v:08b}'[::-1],2) for v in range(256)],np.uint8)

class Encoder:
    def __init__(self,lib):
        self.lib=ctypes.CDLL(str(lib));self.fn=self.lib.pack_positions
        self.fn.argtypes=[ctypes.c_int]+[ctypes.c_void_p]*8;self.fn.restype=None

    def decode(self,arr,gid):
        n=len(arr)
        if not n:raise ValueError('Empty game sample')
        if np.any(arr['stm']>1):raise ValueError('Invalid side to move')
        bb=REVERSE[arr['planes'][:,:12,:]].copy().view('<u8').reshape(n,12)
        old=REVERSE[arr['planes'][:,19,:]].copy().view('<u8').reshape(n)
        rights=arr['rights'].copy()
        if int(arr['format'][0])==1:
            if np.any(rights>1):raise ValueError('Invalid format-1 rights')
            for j in range(4):
                files=np.where(arr['stm']==arr.initial_stm,arr.initial_rooks[j],arr.initial_rooks[j^2])
                rights[:,j]=(rights[:,j]*(1<<files)).astype('u1')
        clocks=arr['clock'].copy()
        ids=np.zeros((n,2,32),'<u2');meta=np.zeros((n,8),'u1')
        board=np.zeros((n,64),'i1');status=np.zeros(n,'i1')
        args=[bb,old,rights,clocks,ids,meta,board,status]
        self.fn(n,*[x.ctypes.data for x in args])
        if np.any(status<0):raise ValueError(f'Malformed board in game {gid}: {status[status<0].tolist()}')
        target,valid=wdl(arr['best_q'],arr['best_d'])
        keep=(status==0)&valid&~arr['planes'][:,12,:].any(1)
        policy=arr['policy']
        if not np.isfinite(policy).all() or np.any((policy<0)&(policy!=-1)):
            raise ValueError('Nonfinite policy or illegal sentinel')
        legal=policy>=0;count=legal.sum(1);mass=np.maximum(policy,0).sum(1)
        if np.any((count<1)|(count>256)) or np.any(abs(mass-1)>1e-4):
            raise ValueError('Invalid teacher policy mass/count')
        selected=np.flatnonzero(keep)
        r,p=np.nonzero(legal[keep]);moves=MOVE_BASE[p].copy();boards=board[keep]
        pc=boards[r,moves[:,0]];dest=boards[r,moves[:,1]]
        if np.any((pc<0)|(pc>=6)):raise ValueError('Legal move does not start on own piece')
        castle=(pc==5)&(dest==3)
        ep=(pc==0)&(dest<0)&(moves[:,0]%8!=moves[:,1]%8)
        quiet=(dest<6)&~ep
        if np.any((dest>=0)&(dest<6)&~castle):raise ValueError('Own piece capture in legal list')
        r,p,moves,pc,castle=(v[quiet] for v in (r,p,moves,pc,castle))
        promo=moves[:,2].copy()
        promo[(promo==0)&(pc==0)&(moves[:,1]>=56)]=1
        special=np.array([0,4,7,6,5],np.uint8)[promo];special[castle]=3
        q=MOVE_DTYPE
        flat=np.empty(len(r),q)
        # Board codes are int8; multiply only after widening, otherwise piece
        # indices 2..5 overflow before NumPy adds the int64 square coordinates.
        piece_base=pc.astype(np.int32)*64
        flat['from']=piece_base+moves[:,0];flat['to']=piece_base+moves[:,1]
        if np.any(flat['from']>=384) or np.any(flat['to']>=384):
            raise ValueError('Quiet move piece-square index outside own-piece range')
        flat['kind']=special
        probabilities=policy[selected[r],p].copy()
        quiet_mass=np.bincount(r,weights=probabilities,minlength=len(selected))
        flat['prob']=probabilities/np.maximum(quiet_mass[r],1e-30)
        offsets=np.r_[0,np.cumsum(np.bincount(r,minlength=len(selected)))].astype('<u4')
        keys=np.empty(len(selected),'V16')
        for i,j in enumerate(selected):
            keys[i]=hashlib.blake2b(ids[j].tobytes()+meta[j].tobytes(),digest_size=16).digest()
        return dict(ids=ids[keep],meta=meta[keep],wdl=target[keep],keys=keys,
            refs=np.column_stack((np.full(len(selected),gid),arr.frame_ids[keep])).astype('<u4'),
            offsets=offsets,moves=flat),dict(raw_frames=n,accepted=len(selected),
            in_check=int((status==1).sum()),chess960=int((status==2).sum()),
            rule50=int((status==3).sum()),invalid_value=int((~valid).sum()),
            repetition=int(arr['planes'][:,12,:].any(1).sum()))

def select(rows,index):
    """Select/reorder records while retaining ALL legal quiet actions."""
    index=np.asarray(index,dtype=np.int64)
    starts=rows['offsets'][index].astype(np.int64)
    counts=rows['offsets'][index+1].astype(np.int64)-starts
    offsets=np.r_[0,np.cumsum(counts)].astype('<u4')
    flat=np.repeat(starts,counts)+np.arange(int(counts.sum()))-np.repeat(offsets[:-1],counts)
    out={k:rows[k][index] for k in ['ids','meta','wdl','keys','refs']}
    out.update(offsets=offsets,moves=rows['moves'][flat]);return out

def concatenate(parts):
    if not parts:raise ValueError('No records to combine')
    out={k:np.concatenate([p[k] for p in parts]) for k in ['ids','meta','wdl','keys','refs','moves']}
    counts=np.concatenate([np.diff(p['offsets']) for p in parts])
    out['offsets']=np.r_[0,np.cumsum(counts)].astype('<u4');return out

def save_rows(path,rows,receipt):
    from common import atomic_json,digest
    import os
    path=Path(path);tmp=path.with_name(path.name+'.partial')
    tmp.mkdir(parents=True,exist_ok=False)
    for key,array in rows.items():
        # Lossless, separately framed arrays; never materialize expanded raw data.
        with (tmp/(key+'.npy.zst')).open('wb') as f:
            with zstd.open(f,'wb',level=1) as encoded:
                np.save(encoded,array,allow_pickle=False)
            f.flush();os.fsync(f.fileno())
    receipt={**receipt,'rows':len(rows['ids']),
        'format':'npy-zstd-1',
        'uncompressed_array_bytes':sum(a.nbytes for a in rows.values()),
        'files':{p.name:digest(p) for p in sorted(tmp.glob('*.npy.zst'))}}
    receipt['bytes']=sum(p.stat().st_size for p in tmp.glob('*.npy.zst'))
    atomic_json(tmp/'receipt.json',receipt);os.replace(tmp,path);return receipt

_verified_shards={}
_cache_manifest=None

def load_rows(path,verify=False):
    from common import digest
    import json
    path=Path(path);receipt=json.loads((path/'receipt.json').read_text())
    # The shared J38 cache stays read-only. Authenticate each shard lazily on
    # first consumption; later loads reject metadata changes without rehashing.
    import os
    global _cache_manifest
    manifest=os.environ.get('FORGE_CACHE_MANIFEST')
    if manifest:
        if _cache_manifest is None:_cache_manifest=json.loads(Path(manifest).read_text())
        if path.parent.resolve()==Path(_cache_manifest['cache']).resolve():
            expected=_cache_manifest['receipts'].get(path.name)
            if expected is None or digest(path/'receipt.json')!=expected:
                raise ValueError('Parent cache receipt changed: '+str(path))
            signature=tuple((name,(path/name).stat().st_size,(path/name).stat().st_mtime_ns)
                            for name in sorted(receipt['files']))
            previous=_verified_shards.get(str(path))
            if previous is not None and signature!=previous:
                raise ValueError('Parent cache changed during training: '+str(path))
            verify=verify or previous is None
            _verified_shards[str(path)]=signature
    if verify:
        for name,sha in receipt['files'].items():
            if digest(path/name)!=sha:raise ValueError('Changed feature shard: '+str(path/name))
    if receipt.get('format')!='npy-zstd-1':raise ValueError('Unsupported compact cache format')
    rows={}
    for p in path.glob('*.npy.zst'):
        with zstd.open(p,'rb') as f:rows[p.name[:-8]]=np.load(f,allow_pickle=False)
    if set(rows)!={'ids','meta','wdl','keys','refs','offsets','moves'}:
        raise ValueError('Incomplete compact shard')
    return rows

def load_keys(path):
    with zstd.open(Path(path)/'keys.npy.zst','rb') as f:return np.load(f,allow_pickle=False)
