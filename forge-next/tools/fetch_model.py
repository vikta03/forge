"""Download one frozen runtime model, verify SHA256, then install atomically."""
from pathlib import Path
import argparse, hashlib, json, os, tempfile, urllib.request

def main():
    root=Path(__file__).resolve().parents[1]
    models=json.loads((root/'models.json').read_text())
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('variant',nargs='?',default='wide192',choices=sorted(models))
    args=parser.parse_args();spec=models[args.variant]
    target=root/spec['local_model'];target.parent.mkdir(exist_ok=True)
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest()==spec['sha256']:
        print('Already verified:',target);return
    temporary=None
    try:
        with urllib.request.urlopen(spec['url'],timeout=60) as response, tempfile.NamedTemporaryFile(dir=target.parent,delete=False) as out:
            temporary=Path(out.name);count=0;digest=hashlib.sha256()
            while block:=response.read(1024*1024):
                count+=len(block)
                if count>spec['bytes']:raise ValueError('Download exceeds expected size')
                digest.update(block);out.write(block)
            out.flush();os.fsync(out.fileno())
        if count!=spec['bytes'] or digest.hexdigest()!=spec['sha256']:raise ValueError('Model size/SHA256 mismatch')
        os.replace(temporary,target);temporary=None
        print('Verified:',target)
    finally:
        if temporary is not None:temporary.unlink(missing_ok=True)

if __name__=='__main__':main()
