"""Small gates for full-frame traversal, compressed storage and training resume."""
import gzip
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
import numpy as np
from common import atomic_json
from encode import Encoder,concatenate,select,save_rows,load_rows
from checks import artificial_game,compact_from_rows
from stream import GameReader,absent

def full_reader(cfg,lib,fixtures,run):
    with tempfile.TemporaryDirectory(dir=run,prefix='full-reader-') as folder:
        folder=Path(folder)
        game=np.concatenate([np.asarray(artificial_game(fixtures[0]['fen'],2)) for _ in range(513)])
        game['clock']=np.arange(513)%100
        raw=game.tobytes();blob=gzip.compress(raw);archive=folder/'game.gz';archive.write_bytes(blob)
        manifest=folder/'corpus.sqlite';db=sqlite3.connect(manifest)
        db.executescript('CREATE TABLE meta(key TEXT,value TEXT); CREATE TABLE archives(id INTEGER,path TEXT,bytes INTEGER,mtime_ns INTEGER);')
        db.execute('INSERT INTO meta VALUES (?,?)',('snapshot',cfg['snapshot']))
        stat=archive.stat();db.execute('INSERT INTO archives VALUES (?,?,?,?)',(1,str(archive),len(blob),stat.st_mtime_ns));db.commit();db.close()
        local=dict(cfg,manifest=str(manifest));reader=GameReader(local,lib)
        rows,stats=reader.get((17,1,0,len(blob),hashlib.sha256(raw).digest(),513,'train'))
        assert stats['raw_frames']==513 and len(rows['ids'])==513
        assert rows['refs'][:,1].tolist()==list(range(513))
        assert np.all(rows['refs'][:,0]==17)
        reader.db.close()
        for f in reader.files.values():f.close()
        keys=rows['keys'];forbidden=np.sort(np.unique(keys[::7]))
        assert np.array_equal(absent(keys,forbidden),~np.isin(keys,forbidden))
        assert absent(keys,np.empty(0,'V16')).all()
        path=folder/'compressed';save_rows(path,rows,dict(test=True))
        restored=load_rows(path,verify=True)
        assert all(np.array_equal(rows[k],restored[k]) for k in rows)
        damaged=path/'moves.npy.zst';damaged.write_bytes(damaged.read_bytes()[:-7])
        try:load_rows(path,verify=True)
        except ValueError:pass
        else:raise AssertionError('Corrupted compressed array accepted')
    result=dict(all_frames_seen=513,chunk_boundary_crossed=True,compressed_roundtrip=True,
                corrupted_shard_rejected=True,validation_key_exclusion=True)
    atomic_json(Path(run)/'full-reader-checks.json',result)

def training_resume(cfg,fixtures,run):
    import torch
    import full_train
    from network import Placement
    from train import evaluate
    rows=compact_from_rows(fixtures)
    # refs identify separate games for exact sharded/full metric aggregation.
    rows['refs'][:,0]=np.arange(len(rows['ids']))
    with tempfile.TemporaryDirectory(dir=run,prefix='full-trainer-') as folder:
        folder=Path(folder);local=dict(cfg,max_epochs=2,min_epochs_before_stop=4,
            effective_batch=16,shuffle_shards=1,validate_every_presentations=10**9,
            validation_sample_records=32,checkpoint_every_seconds=1e9,
            train_records=len(rows['ids']),valid_records=len(rows['ids']))
        def prepare(name):
            root=folder/name;cache=root/'cache';cache.mkdir(parents=True)
            for split in ('valid','train'):
                for i,index in enumerate(np.array_split(np.arange(len(rows['ids'])),2)):
                    save_rows(cache/f'{split}-{i:05d}',select(rows,index),dict(test=True,split=split,snapshot=cfg['snapshot']))
            atomic_json(cache/'validation-ready.json',dict(rows=len(rows['ids'])))
            atomic_json(cache/'producer.json',dict(status='complete',splits={'train':{'rows':len(rows['ids'])}}))
            return root
        # Aggregate metrics must agree with evaluating the same rows together.
        reference=prepare('reference');model=Placement(cfg['seed']).cuda()
        sharded=full_train.full_validation(model,sorted((reference/'cache').glob('valid-*')),4096)
        direct=evaluate(model,rows)
        for key in ('value_ce','policy_ce','brier','expected_mse','game_macro_ce','game_macro_standard_error'):
            assert abs(sharded[key]-direct[key])<2e-6,(key,sharded[key],direct[key])
        del model;torch.cuda.empty_cache()
        a=full_train.train(local,reference)
        interrupted=prepare('resume');original=full_train.atomic_checkpoint
        def stop_after_one_pass(path,payload):
            original(path,payload)
            if Path(path).name=='last.pt' and payload['state']['epoch']==1:
                (interrupted/'STOP').touch()
        full_train.atomic_checkpoint=stop_after_one_pass
        try:paused=full_train.train(local,interrupted)
        finally:full_train.atomic_checkpoint=original
        assert paused['status']=='paused' and paused['epoch']==1
        (interrupted/'STOP').unlink()
        b=full_train.train(local,interrupted)
        assert a['seen']==b['seen']==2*len(rows['ids']) and a['epoch']==b['epoch']==2
        assert b['status']=='budget_complete'
        x=torch.load(reference/'models/last.pt',map_location='cpu',weights_only=True)
        y=torch.load(interrupted/'models/last.pt',map_location='cpu',weights_only=True)
        maximum=0.
        for name,weight in x['model'].items():
            maximum=max(maximum,float((weight-y['model'][name]).abs().max()))
            assert torch.allclose(weight,y['model'][name],atol=2e-5,rtol=2e-5),name
        del x,y;torch.cuda.empty_cache()
    result=dict(full_vs_sharded_validation=True,resumed_full_passes=2,
                presented_rows=a['seen'],max_parameter_difference=maximum,
                complete_coverage_and_resume=True)
    atomic_json(Path(run)/'full-trainer-checks.json',result)
