#!/usr/bin/env python3
"""Package only explicitly listed runtime files and Git-tracked source."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument("--verification",type=Path,required=True)
    a=ap.parse_args()
    proof=json.loads(a.verification.read_text());assert proof["passed"]
    assert "preparation placeholder" not in (ROOT/"docs/evaluation.md").read_text()
    out=ROOT/"dist";out.mkdir(exist_ok=True)
    pkg=out/"forge-netB-1.0.0";pkg.mkdir()
    for rel in ("forge","bin/forge-engine","models/netB-best.forge","COPYING",
                "LICENSE.md","THIRD_PARTY.md","licenses/CC-BY-4.0.txt",
                "src/syzygy/LICENSE","docs/installation.md",
                "provenance/release-verification.json"):
        dest=pkg/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/rel,dest)
    shutil.copy2(a.verification,pkg/"ubuntu-verification.json")
    (pkg/"README.txt").write_text(
        "FORGE netB 1.0.0 — Remmy Victor (@vikta03)\n"
        "Choose the forge launcher in a UCI GUI. Keep this directory together.\n"
        "Linux x86-64, Haswell-compatible AVX2/FMA/BMI1/2 CPU.\n"
        "https://github.com/vikta03/forge\nSee docs/installation.md and licence files.\n")
    archive=out/"forge-netB-1.0.0-linux-x86_64-avx2.tar.gz"
    with tarfile.open(archive,"w:gz") as tar:tar.add(pkg,arcname=pkg.name)
    source=out/"forge-netB-1.0.0-source.tar.gz"
    subprocess.run(["git","archive","--format=tar.gz","--prefix=forge-netB-1.0.0-source/",
                    "-o",str(source),"HEAD"],cwd=ROOT,check=True)
    shutil.copy2(a.verification,out/"ubuntu-verification.json")
    shutil.copy2(ROOT/"provenance/release-verification.json",out/"release-verification.json")
    artifacts=sorted(p for p in out.iterdir() if p.is_file() and p.name!="SHA256SUMS")
    (out/"SHA256SUMS").write_text("".join(
        hashlib.sha256(p.read_bytes()).hexdigest()+"  "+p.name+"\n" for p in artifacts))
if __name__=="__main__":main()
