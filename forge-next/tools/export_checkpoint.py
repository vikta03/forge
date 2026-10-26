"""Export a trusted local training checkpoint using its original integer algebra."""
from pathlib import Path
import argparse, json, sys

def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('family',choices=['specialists','capacity-v1','capacity-v2','reader64'])
    p.add_argument('checkpoint',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
    sys.path[:0]=[str(root/'training'/a.family),str(root/'training/reference')]
    import torch
    from network import Specialist
    from serialization import export
    ck=torch.load(a.checkpoint,map_location='cpu',weights_only=True);cfg=ck['config']
    model=Specialist(cfg['seed']) if a.family=='specialists' else Specialist(cfg['seed'],cfg['architecture'],cfg['boundary_overlap'])
    model.load_state_dict(ck['model'],strict=True)
    if a.family=='specialists':receipt=export(model,a.output)
    else:
        import native
        intermediate=a.output.with_suffix('.int8.npz')
        export(model,intermediate)
        if a.family=='capacity-v1':receipt=native.export(intermediate,a.output,cfg['architecture'])
        else:
            from serialization import read
            weights,_=read(intermediate)
            receipt=native.write_model(weights,a.output,'trained-export')
            receipt['trained']=True
            a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt,indent=2))

if __name__=='__main__':main()
