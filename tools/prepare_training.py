#!/usr/bin/env python3
"""Prepare one split from caller-supplied FENs and aligned soft W/D/L targets."""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--fens", type=Path, required=True)
    ap.add_argument("--targets", type=Path, required=True,
                    help="little-endian float16 [rows,3], W/D/L from side-to-move")
    ap.add_argument("--output-prefix", type=Path, required=True)
    a = ap.parse_args()
    import chess
    import numpy as np
    fens, targets, prefix = a.fens.resolve(), a.targets.resolve(), a.output_prefix.resolve()
    # Validation is streamed; malformed input must fail before the C++ parser.
    count = 0
    with fens.open() as lines:
        for count, line in enumerate(lines, 1):
            if not line.strip() or not chess.Board(line.strip()).is_valid():
                raise ValueError(f"Invalid FEN at row {count}")
    if not count or targets.stat().st_size != count * 6:
        raise ValueError("WDL target bytes must equal exactly 6 * FEN row count")
    labels = np.memmap(targets, dtype="<f2", mode="r", shape=(count,3))
    for start in range(0,count,100000):
        block = np.asarray(labels[start:start+100000], dtype=np.float32)
        if not (np.isfinite(block).all() and (block >= 0).all()
                and (block <= 1).all() and np.allclose(block.sum(1),1,atol=.002)):
            raise ValueError(f"Invalid WDL probabilities near row {start}")
    paths = [Path(str(prefix)+suffix) for suffix in
             (".sections",".sparse",".wide",".bincache",".cells",".widebins")]
    if any(p.exists() for p in paths): raise FileExistsError("Output prefix already exists")
    prefix.parent.mkdir(parents=True,exist_ok=True)
    # The base cache stores the full base feature set; wide columns are separate.
    env = dict(os.environ, FORGE_MINI="1", FORGE_PSQ_CROSSGATE="1",
               FORGE_MLP_HIDDEN="16", FORGE_MLP_PRODUCT="1", FORGE_WIDE_ARM="net0")
    def run(args): subprocess.run([str(x) for x in args],cwd=ROOT,env=env,check=True)
    run([ROOT/"bin/forge-extract",fens,prefix])
    sec,spr,wide,cache,cells,bins = paths
    run([sys.executable,"-m","experiments.forge.build_bin_cache",sec,targets,cache,spr])
    run([sys.executable,"-m","experiments.forge.build_cell_sidecar",
         "--records",sec,"--cache",cache,"--out",cells])
    run([sys.executable,"-m","experiments.wide_trace.prep_bins",sec,wide,bins])
    print(f"Prepared {count} input rows; paths retain the same ordered/filtering contract.")
if __name__ == "__main__": main()
