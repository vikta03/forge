"""Exact quantized export, backward compatible FG2NET1 or four-head FG2NET2."""
import hashlib, json, struct, zlib
from pathlib import Path
import numpy as np
from model import SHAPES

HEAD_NAMES = ('hidden','value','query')
HEAD_BYTES = 328*32 + 32*2 + 32*16

def integer(x):
    a=x.detach().cpu().numpy()
    if not np.isfinite(a).all(): raise ValueError('Nonfinite export')
    return np.clip(np.rint(a),-128,127).astype(np.int8)

def export(model,path):
    arrays={name:integer(model.merged_placement() if name=='placement' else getattr(model,name)) for name in SHAPES}
    payload=b''.join((arrays[k][0] if model.head_count==4 and k in HEAD_NAMES else arrays[k]).tobytes() for k in SHAPES)
    if model.head_count==4:
        payload+=b''.join(arrays[k][i].tobytes() for i in range(1,4) for k in HEAD_NAMES)
    version=2 if model.head_count==4 else 1
    blob=f'FG2NET{version}\0'.encode()+struct.pack('<6I',version,1,128,len(payload),zlib.crc32(payload),model.head_count)
    blob+=payload
    path=Path(path);path.write_bytes(blob)
    receipt=dict(arm=model.arm,version=version,heads=model.head_count,sha256=hashlib.sha256(blob).hexdigest(),
                 bytes=len(blob),shared_factors_coalesced=model.arm=='shared')
    path.with_suffix('.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt

def read(path):
    blob=Path(path).read_bytes()
    version,kind,width,n,crc,heads=struct.unpack('<6I',blob[8:32])
    assert blob[:8]==f'FG2NET{version}\0'.encode() and kind==1 and width==128
    assert (version,heads) in ((1,1),(2,4)) and len(blob)==n+32 and zlib.crc32(blob[32:])==crc
    offset=32;arrays={}
    for name,shape in SHAPES.items():
        count=int(np.prod(shape));arrays[name]=np.frombuffer(blob,dtype=np.int8,count=count,offset=offset).reshape(shape).astype(np.int64)
        offset+=count
    for name in HEAD_NAMES:arrays[name]=arrays[name][None].repeat(heads,axis=0)
    for h in range(1,heads):
        for name in HEAD_NAMES:
            shape=SHAPES[name];count=int(np.prod(shape))
            arrays[name][h]=np.frombuffer(blob,dtype=np.int8,count=count,offset=offset).reshape(shape)
            offset+=count
    assert offset==len(blob)
    return arrays
