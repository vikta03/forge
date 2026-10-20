#!/usr/bin/env python3
"""Launch the recorded netB configuration on explicitly supplied inputs."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data",type=Path,required=True,help="directory with train.* and val.*")
    ap.add_argument("--initial",type=Path,required=True,help="compatible netB warm-start checkpoint")
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--device",default="cuda")
    ap.add_argument("--epochs",type=int,default=24)
    a=ap.parse_args(); root=Path(__file__).resolve().parents[1]; data=a.data.resolve()
    paths=[data/f"{split}.{ext}" for split in ("train","val")
           for ext in ("bincache","bincache.json","cells","cells.json","widebins")]
    missing=[str(p) for p in paths+[a.initial] if not p.is_file()]
    if missing: raise SystemExit("Required inputs missing:\n"+"\n".join(missing))
    env=dict(os.environ,FORGE_MINI="1",FORGE_PSQ_CROSSGATE="1",FORGE_MLP_HIDDEN="16",
             FORGE_MLP_PRODUCT="1",FORGE_WIDE_ARM="netB")
    args=[sys.executable,"-m","experiments.forge.train_section_lattice"]
    for split in ("train","val"):
        args += [f"--bincache-{split}",str(data/f"{split}.bincache"),
                 f"--cells-{split}",str(data/f"{split}.cells"),
                 f"--wide-{split}",str(data/f"{split}.widebins")]
    args += ["--psq-width","16","--king-buckets","32","--variant","factor8",
             "--train-rows","0","--val-rows","0","--epochs",str(a.epochs),
             "--batch","32768","--lr","0.003","--weight-decay","0.000001",
             "--smoothness","0.0001","--seed","20260814","--shuffle-block","524288",
             "--router-em","6,12","--router-split","--router-floor","0.04",
             "--router-stride","8","--initial",str(a.initial.resolve()),
             "--out",str(a.out.resolve()),"--device",a.device]
    subprocess.run(args,cwd=root,env=env,check=True)
if __name__=="__main__": main()
