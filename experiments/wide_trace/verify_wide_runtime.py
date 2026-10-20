#!/usr/bin/env python3
"""Run the standard factor-runtime parity verifier with a wide sidecar."""
from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--arm", required=True, choices=("net0", "netA", "netB", "netC"))
    parser.add_argument("--wide", type=Path, required=True)
    own, remaining = parser.parse_known_args()

    os.environ["FORGE_MINI"] = "1"
    os.environ["FORGE_PSQ_CROSSGATE"] = "1"
    os.environ["FORGE_MLP_HIDDEN"] = "16"
    os.environ["FORGE_MLP_PRODUCT"] = "1"
    os.environ["FORGE_WIDE_ARM"] = own.arm

    from experiments.forge import verify_factor_runtime as verifier

    base = verifier.CachedCorpus

    class WideCorpus(base):
        def __init__(self, path, limit=0, with_psq=False, cells=None, agg=None, **kwargs):
            super().__init__(path, limit, with_psq=with_psq, cells=cells,
                             agg=agg, wide=own.wide, **kwargs)

    verifier.CachedCorpus = WideCorpus
    sys.argv = [sys.argv[0], *remaining]
    verifier.main()


if __name__ == "__main__":
    main()
