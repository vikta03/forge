#!/usr/bin/env python3
"""Fetch the exact release model, verify it, then atomically install it."""
import hashlib
import json
from pathlib import Path
from urllib.request import urlopen

root=Path(__file__).resolve().parents[1]
for name, info in json.loads((root/"models.json").read_text()).items():
    if not name.endswith(".forge"): continue
    target=root/"models"/name
    if target.exists():
        if hashlib.sha256(target.read_bytes()).hexdigest()!=info["sha256"]:
            raise SystemExit(f"Existing model has a different hash: {target}; refusing to overwrite")
        print(f"{name}: verified"); continue
    with urlopen(info["url"],timeout=60) as response:
        payload=response.read(info["bytes"]+1)
    if len(payload)!=info["bytes"] or hashlib.sha256(payload).hexdigest()!=info["sha256"]:
        raise SystemExit(f"{name}: download checksum failed")
    target.parent.mkdir(parents=True,exist_ok=True)
    temporary=target.with_suffix(".download")
    with temporary.open("xb") as out: out.write(payload)
    temporary.replace(target)
    print(f"{name}: installed and verified")
