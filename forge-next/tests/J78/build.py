"""J78 build helpers. Called only inside the approved bounded job."""
from pathlib import Path
import json
import os
import hashlib
import shutil
import subprocess
import time

def command(argv,log,timeout=240):
    start=time.monotonic()
    with Path(log).open('w') as out:
        out.write(json.dumps([str(x) for x in argv])+'\n');out.flush()
        p=subprocess.run([str(x) for x in argv],stdout=out,stderr=subprocess.STDOUT,
                         timeout=timeout,check=False)
    if p.returncode:raise RuntimeError(str(log)+': exit '+str(p.returncode))
    return dict(seconds=time.monotonic()-start,exit_code=p.returncode,log=str(log))

def flags(root):
    return ['-std=c++20','-DNDEBUG','-pthread','-march=haswell','-mtune=generic',
            '-DFORGE_CAPACITY_P=192','-DFORGE_HEAD_WIDTH=32','-DFORGE_HEADS=4',
            '-DFORGE_CAPACITY_DEEP=0','-DFORGE_NEXT_PROBE','-DFORGE_NEXT_OPT=2',
            '-DFORGE_CACHE_AUDIT','-iquote',str(root/'src')]

def instrument(root,target):
    text=(root/'src/search.cpp').read_text()
    for old,new in [
        ('            // --- extensions ---','            audit_before_extensions(ss, tte, posKey, move, ttMove);\n            // --- extensions ---'),
        ('                    const Value singularBeta','                    audit_singular(ss);\n                    const Value singularBeta'),
        ('            newDepth += extension;','            newDepth += extension;\n            audit_before_move(ss, extension);'),
        ('        pos.undo_null_move();','        pos.undo_null_move();\n        audit_child_return("null");'),
        ('                pos.undo_move(pcMove);','                pos.undo_move(pcMove);\n                audit_child_return("probcut");')]:
        assert text.count(old)==1,old
        text=text.replace(old,new)
    target.write_text(text)

def build_all(here,run):
    assert os.environ.get('FORGE_J78_SUPERVISED')=='1'
    root=here/'candidate'
    cfg=json.loads((here/'config.json').read_text())
    if cfg.get('reuse_build_run'):
        prior=Path(cfg['reuse_build_run']);old=prior/'job-source'
        paths=[p.relative_to(here) for p in (here/'candidate/src').rglob('*') if p.is_file()]
        paths += [Path(x) for x in ('contract_tests.cpp','j47_contracts.cpp','j48_contracts.cpp')]
        for rel in paths:
            assert (here/rel).read_bytes()==(old/rel).read_bytes(),str(rel)
        for name in ('candidate-engine','contracts'):
            expected=cfg['reuse_build_sha'][name]
            assert hashlib.sha256((prior/name).read_bytes()).hexdigest()==expected,name
            shutil.copy2(prior/name,run/name)
        return dict(reused_verified_builds=str(prior),source_files_compared=len(paths),
                    binary_hashes=cfg['reuse_build_sha'],prior_builds=json.loads((prior/'builds.json').read_text()))
    sources=sorted((root/'src').glob('*.cpp'))
    result={}
    result['engine']=command(['g++',*flags(root),'-O3','-flto=1','-Wall','-Wextra','-Wpedantic',
        '-frandom-seed=forge2-placement-policy','-ffile-prefix-map='+str(root)+'=.',
        *sources,root/'src/syzygy/tbprobe.cpp','-o',run/'candidate-engine'],run/'engine-build.log')
    search=run/'contract-search.cpp';instrument(root,search)
    result['contracts']=command(['g++',*flags(root),'-O2','-fsanitize=undefined',
        '-fsanitize-trap=undefined','-fno-sanitize-recover=all',
        '-DTERMINAL_GUARD_PRESENT=1','-DAUDIT_SEARCH_SOURCE="'+str(search)+'"',
        here/'contract_tests.cpp',*[p for p in sources if p.name not in ('main.cpp','search.cpp')],
        root/'src/syzygy/tbprobe.cpp','-o',run/'contracts'],run/'contracts-build.log')
    return result
