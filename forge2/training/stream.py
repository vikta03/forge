"""One compressed read per sampled game; bounded compact-shard producer."""
from collections import Counter, OrderedDict, deque
from concurrent.futures import ProcessPoolExecutor
import gzip
import hashlib
import io
import json
import multiprocessing as mp
import os
from pathlib import Path
import shutil
import sqlite3
import time
import numpy as np
from common import atomic_json,require_job,shards,digest
from encode import Encoder,concatenate,select,save_rows,load_rows,load_keys
from codec.raw import records

_reader=None

class GameReader:
    def __init__(self,cfg,lib):
        self.cfg=cfg;self.encoder=Encoder(lib);self.files=OrderedDict()
        self.db=sqlite3.connect(f"file:{cfg['manifest']}?mode=ro",uri=True)
        got=self.db.execute("SELECT value FROM meta WHERE key='snapshot'").fetchone()[0]
        if got!=cfg['snapshot']:raise ValueError('Different corpus snapshot')
        self.archives={r[0]:r[1:] for r in self.db.execute('SELECT id,path,bytes,mtime_ns FROM archives')}

    def get(self,row):
        gid,aid,offset,length,sha,n,split=row;cfg=self.cfg
        if not 18<=length<=cfg['maximum_compressed_game_bytes']:raise ValueError('Compressed game size')
        path,size,mtime=self.archives[aid]
        stat=Path(path).stat()
        if (stat.st_size,stat.st_mtime_ns)!=(size,mtime):raise ValueError('Changed archive: '+path)
        if aid not in self.files:
            self.files[aid]=open(path,'rb')
            if len(self.files)>16:self.files.popitem(last=False)[1].close()
        self.files.move_to_end(aid);f=self.files[aid];f.seek(offset);blob=f.read(length)
        if len(blob)!=length:raise ValueError('Short compressed read')
        with gzip.GzipFile(fileobj=io.BytesIO(blob)) as z:raw=z.read(cfg['maximum_game_bytes']+1)
        if len(raw)>cfg['maximum_game_bytes']:raise ValueError('Inflated game size bound')
        if hashlib.sha256(raw).digest()!=sha:raise ValueError('Game SHA256 mismatch')
        game=records(raw)
        if len(game)!=n:raise ValueError('Game frame count mismatch')
        # Every frame is considered. Chunking bounds the dense policy scratch
        # arrays without imposing the pilot's sampling cap.
        parts=[];stats=Counter()
        for start in range(0,n,cfg['frame_chunk_size']):
            part,counts=self.encoder.decode(game[start:start+cfg['frame_chunk_size']],gid)
            parts.append(part);stats.update(counts)
        encoded=concatenate(parts)
        stats.update(compressed_bytes=length,inflated_bytes=len(raw),games=1)
        return encoded,stats

def initialize(cfg,lib):
    global _reader
    _reader=GameReader(cfg,lib)

def decode_games(rows):
    parts=[];stats=Counter()
    for row in rows:
        data,counts=_reader.get(row);stats.update(counts)
        if len(data['ids']):parts.append(data)
    return (concatenate(parts) if parts else None),dict(stats)

def bounded_map(pool,tasks,window):
    tasks=iter(tasks);pending=deque()
    for _ in range(window):
        try:pending.append(pool.submit(decode_games,next(tasks)))
        except StopIteration:break
    while pending:
        yield pending.popleft().result()
        try:pending.append(pool.submit(decode_games,next(tasks)))
        except StopIteration:pass

def game_order(db,split,seed):
    # Numeric game IDs only: ~16 MiB for the complete train partition.
    ids=np.fromiter((r[0] for r in db.execute('SELECT id FROM games WHERE split=? ORDER BY id',(split,))),dtype=np.int64)
    np.random.default_rng(seed).shuffle(ids);return ids

def descriptors(db,ids,split):
    result=[]
    for start in range(0,len(ids),400):
        part=ids[start:start+400].tolist();marks=','.join('?' for _ in part)
        result.extend(db.execute('SELECT id,archive,offset,bytes,sha256,positions,split FROM games WHERE id IN ('+marks+')',part))
    if len(result)!=len(ids) or any(r[-1]!=split for r in result):raise ValueError('Partition mismatch')
    # Coalesce reads within each random block; training shuffles records later.
    return sorted(result,key=lambda r:(r[1],r[2]))

def absent(keys,sorted_forbidden):
    if not len(sorted_forbidden):return np.ones(len(keys),dtype=bool)
    at=np.searchsorted(sorted_forbidden,keys)
    possible=at<len(sorted_forbidden);keep=np.ones(len(keys),dtype=bool)
    keep[possible]=sorted_forbidden[at[possible]]!=keys[possible]
    return keep

def build_cache(cfg,run,lib):
    require_job();run=Path(run);cache=run/'cache';cache.mkdir(exist_ok=True)
    start=time.monotonic();receipt_path=cache/'producer.json'
    receipt=json.loads(receipt_path.read_text()) if receipt_path.exists() else dict(status='preparing',counts={},shards=0)
    stats=Counter(receipt.get('counts',{}));previous_seconds=receipt.get('active_seconds',0)
    def save(status):
        receipt.update(status=status,counts=dict(stats),active_seconds=previous_seconds+time.monotonic()-start)
        atomic_json(receipt_path,receipt)
    db=sqlite3.connect(f"file:{cfg['manifest']}?mode=ro",uri=True)
    if db.execute("SELECT value FROM meta WHERE key='snapshot'").fetchone()[0]!=cfg['snapshot']:
        raise ValueError('Corpus changed')
    try:
        # Only incomplete derived shard directories can be removed on resume.
        for p in cache.glob('*.partial'):shutil.rmtree(p)
        with ProcessPoolExecutor(cfg['workers'],mp_context=mp.get_context('spawn'),
              initializer=initialize,initargs=(cfg,str(lib))) as pool:
            for split in ('valid','train'):
                existing=shards(cache,split)
                receipts=[json.loads((p/'receipt.json').read_text()) for p in existing]
                for path,r in zip(existing,receipts):
                    if r['snapshot']!=cfg['snapshot']:raise ValueError('Cache provenance mismatch')
                    load_rows(path,verify=True)
                total=sum(r['rows'] for r in receipts)
                cursor=receipts[-1]['next_game_cursor'] if receipts else 0
                written_bytes=sum(json.loads(p.read_text())['bytes'] for p in cache.glob('*/receipt.json'))
                order=game_order(db,split,cfg['seed']+(1 if split=='valid' else 0))
                warm_keys=np.empty(0,'V16')
                if split=='valid':
                    # The warm-start model saw 8M pilot rows. Exclude those
                    # inputs from expanded validation, not merely from J38.
                    prior=[]
                    for old in shards(cfg['warm_training_cache'],'train'):
                        old_receipt=json.loads((old/'receipt.json').read_text())
                        if old_receipt['snapshot']!=cfg['snapshot']:raise ValueError('Pilot cache snapshot differs')
                        if digest(old/'keys.npy')!=old_receipt['files']['keys.npy']:raise ValueError('Pilot keys changed')
                        prior.append(np.load(old/'keys.npy',mmap_mode='r',allow_pickle=False))
                    if not prior:raise ValueError('Pilot training keys missing')
                    warm_keys=np.unique(np.concatenate(prior));del prior
                validation_keys=np.sort(np.concatenate([load_keys(p) for p in shards(cache,'valid')])) if split=='train' else np.empty(0,'V16')
                own_keys=np.sort(np.concatenate([load_keys(p) for p in existing])) if split=='valid' and existing else np.empty(0,'V16')
                while cursor<len(order):
                    if (run/'STOP').exists():save('paused');return
                    end=min(len(order),cursor+cfg['games_per_shard'])
                    rows=descriptors(db,order[cursor:end],split)
                    requested=sum(r[3] for r in rows)
                    reserved=receipt.get('compressed_bytes_reserved',stats['compressed_bytes'])
                    if reserved+requested>cfg['max_compressed_bytes']:
                        raise RuntimeError('Compressed-read ceiling; retain cache and review')
                    receipt['compressed_bytes_reserved']=reserved+requested;save('streaming')
                    tasks=[rows[i:i+cfg['games_per_task']] for i in range(0,len(rows),cfg['games_per_task'])]
                    parts=[]
                    for data,counts in bounded_map(pool,tasks,2*cfg['workers']):
                        stats.update(counts)
                        if data is not None:parts.append(data)
                    if not parts:raise ValueError('Whole game block had no eligible records')
                    data=concatenate(parts)
                    # Deduplicate model inputs within each shard and across validation.
                    _,unique=np.unique(data['keys'],return_index=True)
                    unique.sort();stats['within_shard_duplicates']+=len(data['ids'])-len(unique)
                    data=select(data,unique)
                    if split=='valid':
                        keep=absent(data['keys'],warm_keys)
                        stats['valid_pilot_overlap_removed']+=int((~keep).sum())
                        data=select(data,np.flatnonzero(keep))
                    forbidden=validation_keys if split=='train' else own_keys
                    if len(forbidden):
                        keep=absent(data['keys'],forbidden)
                        stats[split+'_heldout_overlap_removed']+=int((~keep).sum())
                        data=select(data,np.flatnonzero(keep))
                    if not len(data['ids']):raise ValueError('No distinct eligible records in block')
                    required=sum(v.nbytes for v in data.values())+16384
                    if written_bytes+required>cfg['cache_bytes']:raise RuntimeError('48-GiB compact-feature limit')
                    if shutil.disk_usage(cache).free<required+cfg['minimum_free_disk_bytes']:raise RuntimeError('Free-disk reserve reached')
                    path=cache/f'{split}-{len(receipts):05d}'
                    r=save_rows(path,data,dict(snapshot=cfg['snapshot'],split=split,
                        start_game_cursor=cursor,next_game_cursor=end,seed=cfg['seed'],
                        raw_histories_saved=False,source_games=[int(x[0]) for x in rows]))
                    receipts.append(r);written_bytes+=r['bytes'];total+=r['rows'];cursor=end
                    if split=='valid':own_keys=np.sort(np.r_[own_keys,data['keys']])
                    receipt.update(split=split,records=total,shards=len(receipts),compact_bytes=written_bytes)
                    save('streaming')
                receipt.setdefault('splits',{})[split]=dict(rows=total,games=len(order),shards=len(receipts),complete=True)
                if split=='valid':atomic_json(cache/'validation-ready.json',dict(rows=total,snapshot=cfg['snapshot']))
        save('complete')
    except BaseException as e:
        receipt['error']=f'{type(e).__name__}: {e}';save('failed');raise
    finally:db.close()

if __name__=='__main__':
    import sys
    from common import config
    build_cache(config(),Path(sys.argv[1]),Path(sys.argv[2]))
