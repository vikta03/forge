"""Release gates for the new reader/trainer, executed only inside J38."""
import json
from pathlib import Path
import sys
import tempfile
import numpy as np
from common import J35,J35_RUN,BINARY,atomic_json,require_job
if str(J35) not in sys.path:sys.path.append(str(J35))
from encode import Encoder,MOVE_DTYPE,concatenate,select,save_rows,load_rows
from codec.raw import records,dtype,MOVE_BASE,decode as control_decode
from codec.fast_raw import decode

def compact_from_rows(rows):
    from features import board_features
    parts=[]
    for i,row in enumerate(rows):
        f=board_features(row['fen']);occupied=f['occupied']>0
        ids=np.full((1,2,32),24576,'<u2')
        for v in range(2):
            values=f['planes'][v,occupied]+768*f['buckets'][v]
            ids[0,v,:len(values)]=np.sort(values)
        moves=np.empty(len(f['moves']),MOVE_DTYPE)
        # Normalize once in float64, then store float32. This independent
        # oracle must not introduce two float32 normalizations before comparing
        # the producer's direct conditional distribution.
        probabilities=np.array([row.get('quiet_policy',{}).get(u,0.) for u in f['uci']],np.float64)
        mass=probabilities.sum()
        if mass>0:probabilities/=mass
        for j,((a,b,c),u) in enumerate(zip(f['moves'],f['uci'])):
            moves[j]=(a,b,c,probabilities[j])
        parts.append(dict(ids=ids,meta=f['meta'][None,:].astype('u1'),
            wdl=np.array([row.get('wdl',[0,1,0])],'<f4'),keys=np.zeros(1,'V16'),
            refs=np.array([[i,0]],'<u4'),offsets=np.array([0,len(moves)],'<u4'),moves=moves))
    return concatenate(parts)

def compare_game(encoder,game,gid):
    import chess
    from data import board_from,action
    a=decode(game);b=control_decode(game)
    for key in ('features','moves','legal','policy','state_wdl','state_valid'):
        if not np.array_equal(a[key],b[key]):raise ValueError('Frozen raw decoders disagree: '+key)
    got,stats=encoder.decode(game,gid);expected=[];frames=[]
    for i in range(len(game)):
        board=board_from(a['features'][i])
        if not board.is_valid():raise ValueError('Python-chess rejected raw fixture')
        legal={action(board,m):m for m in board.legal_moves}
        keys=[tuple(map(int,m)) for m in a['moves'][i][a['legal'][i]]]
        if set(legal)!=set(keys):raise ValueError('Raw legal-action set differs from independent generator')
        keep=(a['state_valid'][i] and not board.is_check() and board.halfmove_clock<100
              and not board.has_chess960_castling_rights() and not a['features'][i,:,12].any())
        if not keep:continue
        if not board.has_pseudo_legal_en_passant():board.ep_square=None
        raw_policy=game['policy'][i]
        policy={legal[k].uci():float(p) for k,p in zip(keys,raw_policy[raw_policy>=0])
                if not board.is_capture(legal[k])}
        expected.append(dict(fen=board.fen(en_passant='fen'),wdl=a['state_wdl'][i].tolist(),quiet_policy=policy))
        frames.append(int(game.frame_ids[i]))
    if frames!=got['refs'][:,1].tolist():raise ValueError('Eligibility masks differ')
    if expected:
        want=compact_from_rows(expected)
        for k in ('ids','meta','wdl'):
            if not np.array_equal(got[k],want[k]):raise ValueError('Compact encoding differs: '+k)
        for i in range(len(expected)):
            def moves(x):
                return {(int(m['from']),int(m['to']),int(m['kind'])):float(m['prob'])
                        for m in x['moves'][x['offsets'][i]:x['offsets'][i+1]]}
            gm,wm=moves(got),moves(want)
            if gm.keys()!=wm.keys() or any(abs(gm[k]-wm[k])>2e-7 for k in gm):
                raise ValueError('Conditional quiet-policy differs: '+json.dumps(dict(
                    fen=expected[i]['fen'],actual_only=sorted(gm.keys()-wm.keys()),
                    expected_only=sorted(wm.keys()-gm.keys()),
                    max_shared_error=max((abs(gm[k]-wm[k]) for k in gm.keys()&wm.keys()),default=0))))
    return stats

def artificial_game(fen,fmt):
    """Hand-built raw frame from independent legal move generation."""
    import chess
    from data import action
    board=chess.Board(fen,chess960=True)
    if not board.is_valid():raise ValueError('Bad artificial fixture')
    arr=np.zeros(1,dtype(6));arr['version']=6;arr['format']=fmt
    arr['stm']=not board.turn;arr['clock']=board.halfmove_clock;arr['best_d']=.5
    arr['result_d']=.5;arr['played_d']=.5
    planes=np.zeros((104,64),np.uint8);flip=0 if board.turn else 56
    for sq,pc in board.piece_map().items():
        planes[(pc.piece_type-1)+6*(pc.color!=board.turn),sq^flip]=1
    planes[19]=planes[6]
    if board.ep_square is not None:
        f=(board.ep_square^flip)%8
        if planes[6,32+f]:planes[19,32+f]=0;planes[19,48+f]=1
    arr['planes'][0]=np.packbits(planes,axis=1,bitorder='big')
    for side,color in enumerate((board.turn,not board.turn)):
        k=board.king(color)%8
        for sq in chess.scan_forward(board.castling_rights&board.occupied_co[color]):
            j=2*side+int(sq%8>k);arr['rights'][0,j]=(1 if fmt==1 else 1<<(sq%8))
    lookup={(int(m[0]),int(m[1]),int(m[2])):i for i,m in enumerate(MOVE_BASE)}
    indices=[]
    for move in board.legal_moves:
        a,b,p,c=action(board,move);indices.append(lookup[(a,b,0 if p==1 else p)])
    if not indices:raise ValueError('Fixture has no legal moves')
    arr['policy']=-1;arr['policy'][0,indices]=1/len(indices)
    arr['best_idx']=indices[0];arr['played_idx']=indices[-1]
    return records(arr.tobytes())

def verify_reader(cfg,lib,run):
    require_job()
    import chess
    from verify_worker import FENS,SPECIAL
    from stream import GameReader
    encoder=Encoder(lib);summary=dict(artificial_frames=0,raw_frames=0,raw_games=0)
    cases=FENS+SPECIAL
    cases += [chess.Board(f,chess960=True).mirror().fen(en_passant='fen') for f in cases]
    for clock in (1,2,49,50,99,100,255):
        b=chess.Board(FENS[0]);b.halfmove_clock=clock;cases.append(b.fen())
    cases.append('4k3/8/8/8/8/8/4r3/4K3 w - - 0 1')
    for fen in cases:
        for fmt in (1,2):
            compare_game(encoder,artificial_game(fen,fmt),0);summary['artificial_frames']+=1
    repeated=np.asarray(artificial_game(FENS[0],2)).copy()
    repeated['planes'][:,12,:]=255
    compare_game(encoder,records(repeated.tobytes()),0)
    invalid=np.asarray(artificial_game(FENS[0],2)).copy();invalid['best_q']=float('nan')
    compare_game(encoder,records(invalid.tobytes()),0)
    broken=np.asarray(artificial_game(FENS[0],2)).copy();broken['policy'][0,0]=-2
    try:encoder.decode(records(broken.tobytes()),0)
    except ValueError:pass
    else:raise ValueError('Malformed policy was accepted')
    summary['repetition_invalid_value_bad_policy_checks']=True
    reader=GameReader(cfg,lib)
    # Reuse already enumerated game IDs, not a new corpus/index scan.
    receipt=json.loads((J35_RUN/'data/receipt.json').read_text())
    ids=[r['id'] for r in receipt['games'][:12]]+[r['id'] for r in receipt['games'][-4:]]
    import gzip,io,hashlib
    for gid in ids:
        row=reader.db.execute('SELECT id,archive,offset,bytes,sha256,positions,split FROM games WHERE id=?',(gid,)).fetchone()
        if row[-1] not in ('train','valid'):raise ValueError('Reserved test split')
        path,size,mtime=reader.archives[row[1]];st=Path(path).stat()
        if (st.st_size,st.st_mtime_ns)!=(size,mtime):raise ValueError('Changed archive')
        with open(path,'rb') as f:f.seek(row[2]);blob=f.read(row[3])
        with gzip.GzipFile(fileobj=io.BytesIO(blob)) as f:raw=f.read(cfg['maximum_game_bytes']+1)
        if len(raw)>cfg['maximum_game_bytes'] or hashlib.sha256(raw).digest()!=row[4]:raise ValueError('Raw fixture identity')
        game=records(raw);frames=np.linspace(0,len(game)-1,min(16,len(game)),dtype=int)
        compare_game(encoder,game[frames],gid);summary['raw_frames']+=len(frames);summary['raw_games']+=1
    fixtures=[json.loads(l) for l in (J35_RUN/'data/train.jsonl').read_text().splitlines()[:32]]
    rows=compact_from_rows(fixtures)
    with tempfile.TemporaryDirectory(dir=run,prefix='encoding-check-') as tmp:
        path=Path(tmp)/'shard';save_rows(path,rows,{'test':True})
        restored=load_rows(path,verify=True)
        for k in rows:
            if not np.array_equal(rows[k],restored[k]):raise ValueError('Feature shard roundtrip')
        rng=np.random.default_rng(cfg['seed']);order=rng.permutation(len(rows['ids']))
        for k,v in select(rows,order).items():
            pieces=concatenate([select(rows,order[:13]),select(rows,order[13:])])
            if not np.array_equal(v,pieces[k]):raise ValueError('Batch/group resume ordering')
    reader.db.close()
    summary['shard_roundtrip_and_cursor_checks']=True
    atomic_json(Path(run)/'reader-checks.json',summary);return fixtures

def verify_training_math(cfg,fixtures,run):
    import torch
    from model import Forge2,loss
    from features import collate
    from network import Placement,to_cpu_batch,loss_sums,optimizer,update
    frozen=Forge2('placement',cfg['seed']).cuda();pooled=Placement(cfg['seed']).cuda()
    base,_=collate(fixtures,'cuda');compact=compact_from_rows(fixtures)
    b,valid=to_cpu_batch(compact,np.arange(len(fixtures)))
    b={k:v.cuda() for k,v in b.items()};a=frozen(base);c=pooled(b)
    for key in ('hidden','logits','logwdl'):
        if not torch.equal(a[key],c[key]):raise ValueError('Pooled forward parity: '+key)
    expected=a['raw_policy'][base['move_mask']]
    if not torch.equal(expected,c['raw_policy']):raise ValueError('Ragged policy parity')
    lf,_=loss(a,base,cfg['quiet_policy_weight']);v,p=loss_sums(c,b)
    lp=v/len(fixtures)+cfg['quiet_policy_weight']*p/max(1,valid)
    if not torch.allclose(lf,lp,atol=2e-6,rtol=2e-6):raise ValueError('Ragged loss differs')
    lf.backward();lp.backward();maximum=0.
    for (name,x),(_,y) in zip(frozen.named_parameters(),pooled.named_parameters()):
        if not x.requires_grad:continue
        maximum=max(maximum,float((x.grad-y.grad).abs().max()))
        if not torch.allclose(x.grad,y.grad,atol=2e-5,rtol=2e-4):raise ValueError('Gradient parity: '+name)
    receipt=dict(positions=len(fixtures),forward_integer_exact=True,loss=float(lp),
        max_gradient_difference=maximum,gradients_close=True)
    # A real optimizer/checkpoint reload, including a pending unconsumed batch.
    opt=optimizer(pooled,cfg);update(pooled,opt,[b],valid,len(fixtures),cfg)
    with tempfile.TemporaryDirectory(dir=run,prefix='resume-check-') as tmp:
        path=Path(tmp)/'checkpoint.pt'
        torch.save(dict(model=pooled.state_dict(),optimizer=opt.state_dict(),
            state=dict(epoch=2,group=3,row=4096,seen=16004096)),path)
        saved=torch.load(path,map_location='cpu',weights_only=True)
        restored=Placement(cfg['seed']).cuda();restored.load_state_dict(saved['model'])
        other=optimizer(restored,cfg);other.load_state_dict(saved['optimizer'])
        update(pooled,opt,[b],valid,len(fixtures),cfg)
        update(restored,other,[b],valid,len(fixtures),cfg)
        for (name,x),(_,y) in zip(pooled.named_parameters(),restored.named_parameters()):
            if not torch.allclose(x,y,atol=2e-5,rtol=2e-5):raise ValueError('Optimizer resume differs: '+name)
        if saved['state']!=dict(epoch=2,group=3,row=4096,seen=16004096):raise ValueError('Checkpoint coverage differs')
        receipt['optimizer_resume_next_update_close']=True
        del restored,other,saved
    atomic_json(Path(run)/'training-math-checks.json',receipt)
    del opt,frozen,pooled,base,b,a,c,lf,lp,v,p;torch.cuda.empty_cache()

def verify_export(cfg,checkpoint,run,tag='best'):
    """No games: model loading, static parity and two bounded 4096-node searches."""
    import torch
    import chess
    from network import Placement
    from network import to_cpu_batch
    from features import collate
    from export_model import export
    from engine import Engine
    from verify_worker import FENS,SPECIAL,cp_from_raw
    from model import Forge2
    run=Path(run);ck=torch.load(checkpoint,map_location='cpu',weights_only=True)
    model=Forge2('placement',cfg['seed']);model.load_state_dict(ck['model'])
    pooled=Placement(cfg['seed']);pooled.load_state_dict(ck['model'])
    path=run/'models'/(tag+'.fg2');receipt=export(model,path)
    if export(model,run/'models'/(tag+'-roundtrip.fg2'))['sha256']!=receipt['sha256']:
        raise ValueError('Export roundtrip differs')
    engine=Engine(BINARY,run/(tag+'-native.log'))
    try:
        engine.load(path);engine.ready()  # Standalone engine disables endgame shortcuts at startup.
        fens=FENS+SPECIAL
        for f in fens:
            b,feats=collate([dict(fen=f)])
            with torch.no_grad():pred=model(b)
            cb,_=to_cpu_batch(compact_from_rows([dict(fen=f)]),np.array([0]))
            with torch.no_grad():other=pooled(cb)
            for key in ('hidden','logits'):
                if not torch.equal(pred[key],other[key]):raise ValueError('Trained pooled algebra differs')
            if not torch.equal(pred['raw_policy'][b['move_mask']],other['raw_policy']):
                raise ValueError('Trained ragged policy differs')
            got=engine.inspect(f);raw=[int(x) for x in (pred['logits'][0]*256).tolist()]
            quiet={u:int(v) for u,v in zip(feats[0]['uci'],pred['raw_policy'][0].tolist())}
            if got['hidden']!=[int(x) for x in pred['hidden'][0].tolist()] or got['raw_value_logits']!=raw or got['quiet_policy']!=quiet:
                raise ValueError('Native checkpoint parity')
            if abs(got['score']-cp_from_raw(raw))>1:raise ValueError('Native CP parity')
        engine.send('setoption name ProbeAudit value true');engine.ready()
        searches=[]
        for f in FENS[:2]:
            r=engine.search(f,4096)
            if chess.Move.from_uci(r['bestmove']) not in chess.Board(f).legal_moves:raise ValueError('Illegal searched move')
            if not r['probe'] or not r['probe']['audits'] or r['probe']['synthetic']:raise ValueError('Loaded audit path missing')
            searches.append(r)
        receipt.update(native_positions=len(fens),searches=searches,strength_test=False)
        atomic_json(run/(tag+'-verification.json'),receipt)
    finally:engine.close()
    return receipt
