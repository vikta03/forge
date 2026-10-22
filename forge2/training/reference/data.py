"""Bounded sampling from the existing game-split Lc0 manifest. GPL-3.0."""
import collections
import gzip
import hashlib
import io
import json
from pathlib import Path
import random
import sqlite3
import numpy as np
import chess
from codec.raw import records,decode as control_decode
from codec.fast_raw import decode

def board_from(x):
    stm=not bool(x[0,108]);flip=0 if stm else 56
    b=chess.Board(None,chess960=True);b.turn=stm;b.halfmove_clock=round(float(x[0,109])*100)
    for c in range(12):
        for sq in np.flatnonzero(x[:,c]):
            b.set_piece_at(int(sq)^flip,chess.Piece(c%6+1,stm if c<6 else not stm))
    b.castling_rights=sum(chess.BB_SQUARES[int(sq)^flip] for sq in np.flatnonzero(x[:,104:108].sum(1)))
    ep=np.flatnonzero(x[:,110]);b.ep_square=(int(ep[0])^flip) if len(ep) else None
    return b

def action(b,m):
    flip=0 if b.turn else 56
    promotion={chess.KNIGHT:1,chess.QUEEN:2,chess.ROOK:3,chess.BISHOP:4}.get(m.promotion,0)
    castle=(1 if m.to_square>m.from_square else 2) if b.is_castling(m) else 0
    return (m.from_square^flip,m.to_square^flip,promotion,castle)

def sample(manifest,out,train_games=128,valid_games=32,frames_per_game=8,seed=20261021,max_read_bytes=256*1024**2):
    out=Path(out);out.mkdir(parents=True,exist_ok=False)
    db=sqlite3.connect(f'file:{Path(manifest).resolve()}?mode=ro',uri=True)
    rng=random.Random(seed);seen_games=set();seen_inputs=set();counts=collections.Counter()
    receipt=dict(seed=seed,snapshot=db.execute("SELECT value FROM meta WHERE key='snapshot'").fetchone()[0],
                 source_manifest=str(Path(manifest).resolve()),games=[],skipped={},compressed_bytes_read=0,
                 target='best_q/best_d teacher WDL, side to move',reserved_test_untouched=True,
                 decoder_control_checks=0)
    # Separate endpoint lookups use the primary key; a combined min/max can
    # force SQLite to scan the entire games table.
    lo=db.execute('SELECT id FROM games ORDER BY id LIMIT 1').fetchone()[0]
    hi=db.execute('SELECT id FROM games ORDER BY id DESC LIMIT 1').fetchone()[0]
    def save():
        receipt['skipped']=dict(counts)
        (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    try:
        for split,wanted in [('train',train_games),('valid',valid_games)]:
            selected=0;attempts=0;written=0
            with (out/(split+'.jsonl')).open('w') as output:
                while selected<wanted:
                    attempts+=1
                    if attempts>max(10000,wanted*2000):raise RuntimeError('Insufficient distinct games within selection bound')
                    gid=rng.randint(lo,hi)
                    row=db.execute('SELECT archive,offset,bytes,sha256,split,positions FROM games WHERE id=?',(gid,)).fetchone()
                    if not row or row[4]!=split or bytes(row[3]).hex() in seen_games:continue
                    aid,offset,length,sha,_,n=row
                    path,size,mtime=db.execute('SELECT path,bytes,mtime_ns FROM archives WHERE id=?',(aid,)).fetchone()
                    st=Path(path).stat()
                    if (st.st_size,st.st_mtime_ns)!=(size,mtime):raise RuntimeError('Archive metadata differs from index')
                    if not 18<=length<=16*1024**2:raise RuntimeError('Compressed game size bound')
                    if receipt['compressed_bytes_read']+length>max_read_bytes:raise RuntimeError('Compressed-input byte ceiling')
                    with open(path,'rb') as stream:stream.seek(offset);compressed=stream.read(length)
                    receipt['compressed_bytes_read']+=len(compressed)
                    with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:raw=stream.read(64*1024**2+1)
                    if len(raw)>64*1024**2:raise RuntimeError('Uncompressed game size bound')
                    if hashlib.sha256(raw).digest()!=sha:raise RuntimeError('Game SHA differs from index')
                    game=records(raw)
                    if len(game)!=n:raise RuntimeError('Frame count differs from index')
                    gamehash=bytes(sha).hex();seen_games.add(gamehash);selected+=1
                    local=random.Random(seed ^ int(gamehash[:16],16))
                    frames=sorted(local.sample(range(n),min(n,frames_per_game)))
                    selected_frames=game[frames];d=decode(selected_frames);before=written
                    if selected==1:
                        control=control_decode(selected_frames)
                        for key in ('features','moves','legal','policy','state_wdl','state_valid'):
                            if not np.array_equal(d[key],control[key]):
                                raise RuntimeError('Frozen decoder disagreement: '+key)
                        receipt['decoder_control_checks']+=len(frames)
                    for j,frame in enumerate(frames):
                        if not d['state_valid'][j]:counts[split+'/invalid_teacher_wdl']+=1;continue
                        x=d['features'][j];b=board_from(x)
                        if not b.is_valid():raise RuntimeError('Decoded illegal board')
                        legal={action(b,m):m for m in b.legal_moves}
                        keys=[tuple(map(int,k)) for k in d['moves'][j][d['legal'][j]]]
                        if set(keys)!=set(legal):raise RuntimeError('Raw legal actions disagree with python-chess')
                        if b.has_chess960_castling_rights():counts[split+'/nonstandard_castling']+=1;continue
                        if b.is_check():counts[split+'/in_check']+=1;continue
                        if x[:,12].any():counts[split+'/current_repetition']+=1;continue
                        if b.halfmove_clock>=100:counts[split+'/claimable_fifty_move']+=1;continue
                        if not b.has_pseudo_legal_en_passant():b.ep_square=None
                        fen=b.fen(en_passant='fen');identity=' '.join(fen.split()[:5])
                        key=hashlib.sha256(identity.encode()).hexdigest()
                        if key in seen_inputs:counts[split+'/duplicate_board_clock']+=1;continue
                        seen_inputs.add(key)
                        policy={legal[k].uci():float(p) for k,p in zip(keys,d['policy'][j][d['legal'][j]]) if not b.is_capture(legal[k])}
                        row=dict(fen=fen,wdl=d['state_wdl'][j].tolist(),quiet_policy=policy,game=gamehash,
                                 game_id=gid,frame=frame,split=split,archive=aid,input_sha256=key,
                                 quiet_policy_mass=sum(policy.values()))
                        output.write(json.dumps(row,separators=(',',':'))+'\n');written+=1
                    receipt['games'].append(dict(id=gid,sha256=gamehash,archive=aid,split=split,
                                                frames_sampled=len(frames),rows_written=written-before))
                    save()
            receipt[split+'_rows']=written
            receipt[split+'_sha256']=hashlib.sha256((out/(split+'.jsonl')).read_bytes()).hexdigest()
        if receipt['train_rows']<64 or receipt['valid_rows']<16:raise RuntimeError('Too few accepted rows')
        receipt['status']='complete'
    except BaseException as e:
        receipt['status']='failed';receipt['error']=str(e);raise
    finally:save();db.close()
    return receipt
