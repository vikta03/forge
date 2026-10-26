"""Portable entry to the preserved trainer. Requires an external encoded corpus."""
from pathlib import Path
import argparse, hashlib, json, os, re, sys

def main():
    root=Path(__file__).resolve().parents[1]
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('family',choices=['specialists','capacity-v1','capacity-v2','reader64'])
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--cache',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if os.environ.get('FORGE_TRAINING_ALLOWED')!='1':
        p.error('Set FORGE_TRAINING_ALLOWED=1 after arranging your own resource limits.')
    cache=a.cache.resolve();cfg=json.loads(a.config.read_text());run=a.output.resolve()
    producer=json.loads((cache/'producer.json').read_text())
    if producer['status']!='complete' or not (cache/'validation-ready.json').exists():
        p.error('Cache must be complete before training.')
    for split,key in [('train','train_records'),('valid','valid_records')]:
        if producer['splits'][split]['rows']!=cfg[key]:p.error(f'{split} cache count differs from config')
    run.mkdir(parents=True,exist_ok=True)
    marker=run/'cache-source.txt';configuration=run/'configuration.json'
    if marker.exists() and marker.read_text().strip()!=str(cache):p.error('Resume cache changed')
    if configuration.exists() and json.loads(configuration.read_text())!=cfg:p.error('Resume configuration changed')
    receipts={d.name:hashlib.sha256((d/'receipt.json').read_bytes()).hexdigest()
              for d in sorted(cache.iterdir()) if d.is_dir()
              and re.fullmatch(r'(?:train|valid)-[0-9]{5}',d.name)}
    if not receipts:p.error('Cache contains no shard receipts')
    manifest=run/'cache-manifest.json'
    inventory={'cache':str(cache),'receipts':receipts}
    if manifest.exists() and json.loads(manifest.read_text())!=inventory:p.error('Resume shard receipts changed')
    manifest.write_text(json.dumps(inventory,indent=2)+'\n')
    os.environ['FORGE_CACHE_MANIFEST']=str(manifest)
    marker.write_text(str(cache)+'\n');configuration.write_text(json.dumps(cfg,indent=2)+'\n')
    sys.path[:0]=[str(root/'training'/a.family),str(root/'training/reference')]
    from network import configure
    from full_train import train
    configure(cfg)
    print(json.dumps(train(cfg,run),indent=2))

if __name__=='__main__':main()
