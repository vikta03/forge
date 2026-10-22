"""Export integer parameters with a versioned header and CRC. GPL-3.0."""
import hashlib
import json
from pathlib import Path
import struct
import zlib
import numpy as np
from model import SHAPES

MAGIC=b'FG2NET1\0'
WEIGHT_BYTES=sum(int(np.prod(s)) for s in SHAPES.values())

def export(model,path):
    path=Path(path)
    parts=[]
    for name,shape in SHAPES.items():
        values=getattr(model,name).detach().cpu().numpy()
        if values.shape!=shape or not np.isfinite(values).all():raise ValueError('Invalid parameters: '+name)
        parts.append(np.clip(np.rint(values),-128,127).astype(np.int8).tobytes())
    payload=b''.join(parts)
    if len(payload)!=WEIGHT_BYTES:raise ValueError('Parameter byte count')
    header=MAGIC+struct.pack('<6I',1,1 if model.kind=='placement' else 2,128,len(payload),zlib.crc32(payload),1)
    path.write_bytes(header+payload)
    receipt=dict(format='FG2NET1',kind=model.kind,bytes=len(header+payload),parameter_bytes=len(payload),
                 sha256=hashlib.sha256(header+payload).hexdigest(),shapes=SHAPES)
    path.with_suffix(path.suffix+'.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt

if __name__=='__main__':
    import argparse,torch
    from model import Forge2
    ap=argparse.ArgumentParser();ap.add_argument('checkpoint',type=Path);ap.add_argument('output',type=Path);a=ap.parse_args()
    ck=torch.load(a.checkpoint,map_location='cpu',weights_only=True)
    m=Forge2(ck['kind']);m.load_state_dict(ck['model']);export(m,a.output)
