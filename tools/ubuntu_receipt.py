#!/usr/bin/env python3
"""Record the actual CI binary's compiler, ABI and completed checks."""
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
binary=root/"bin/forge-engine"
protocol=json.loads((out/"protocol/verification.json").read_text())
assert protocol["passed"] and "all tests passed" in (out/"rules.log").read_text()
def read(args):return subprocess.check_output(args,text=True)
dynamic=read(["readelf","-d",str(binary)])
assert "RPATH" not in dynamic and "RUNPATH" not in dynamic
versions=read(["readelf","--version-info",str(binary)])
abi=sorted(set(re.findall(r"\b(?:GLIBCXX|GLIBC|CXXABI)_[0-9.]+",versions)))
receipt={"passed":True,"platform":platform.platform(),"libc":platform.libc_ver(),
         "compiler":read([os.environ.get("CXX","g++"),"--version"]).splitlines()[0],
         "source_commit":read(["git","rev-parse","HEAD"]).strip(),"required_abi":abi,
         "binary_sha256":hashlib.sha256(binary.read_bytes()).hexdigest(),
         "model_sha256":protocol["model_sha256"],"protocol":protocol,
         "rules":"Inherited rules/SEE/make-unmake/Chess960/perft and clock-cache checks passed",
         "no_strength_claim":True}
(out/"ubuntu-verification.json").write_text(json.dumps(receipt,indent=2)+"\n")
