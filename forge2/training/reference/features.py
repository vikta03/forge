"""Independent board-to-FORGE2 inputs. GPL-3.0; no learned chess bonuses."""
import chess
import numpy as np

DX = (1,1,0,-1,-1,-1,0,1,2,1,-1,-2,-2,-1,1,2)
DY = (0,1,1,1,0,-1,-1,-1,1,2,2,1,-1,-2,-2,-1)
PHASE = (0,0,1,1,2,4,0)

def code(pc, view): return pc.piece_type-1+6*(pc.color != view)
def square(sq, view): return sq if view else sq ^ 56
def plane(pc, sq, view): return code(pc,view)*64+square(sq,view)

def board_features(fen):
    b = chess.Board(fen, chess960=True)
    if not b.is_valid(): raise ValueError('Invalid board: '+fen)
    # Match FORGE's FEN loader and make-move rule: retain EP when a pawn
    # can geometrically capture, including a pinned pawn. No new hash policy.
    if not b.has_pseudo_legal_en_passant():b.ep_square=None
    board = b.piece_map(); occupied = np.zeros(64, np.float32)
    planes = np.zeros((2,64), np.int64); buckets = np.zeros(2,np.int64)
    for v,view in enumerate((b.turn,not b.turn)):
        k = square(b.king(view),view); buckets[v]=(k//8)*4+min(k%8,7-k%8)
        for s,p in board.items(): occupied[s]=1; planes[v,s]=plane(p,s,view)
    edges=[]
    # Enumerate pairs and blockers independently of the native ray walker.
    for s,p in sorted(board.items()):
        for t,target in sorted(board.items()):
            if s==t: continue
            dx=t%8-s%8;dy=t//8-s//8;ax=abs(dx);ay=abs(dy);pt=p.piece_type
            leap=(pt==chess.KNIGHT and ax*ay==2) or (pt==chess.KING and max(ax,ay)==1)
            leap |= pt==chess.PAWN and ax==1 and dy==(1 if p.color else -1)
            ray=(pt==chess.BISHOP and ax==ay) or (pt==chess.ROOK and (not ax or not ay))
            ray |= pt==chess.QUEEN and (ax==ay or not ax or not ay)
            if not (leap or ray): continue
            blockers=[]
            if not leap:
                sx=(dx>0)-(dx<0);sy=(dy>0)-(dy<0);x=s%8+sx;y=s//8+sy
                while (x,y)!=(t%8,t//8):
                    if y*8+x in board: blockers.append(board[y*8+x])
                    x+=sx;y+=sy
                if len(blockers)>1:continue
            rows=[];block=[]
            for view in (b.turn,not b.turn):
                a,c=square(s,view),square(t,view);dx=c%8-a%8;dy=c//8-a//8
                knight=abs(dx)*abs(dy)==2
                delta=(dx,dy) if knight else ((dx>0)-(dx<0),(dy>0)-(dy<0))
                direction=list(zip(DX,DY)).index(delta)
                row=((((code(p,view)*12+code(target,view))*16+direction)*7
                      +max(abs(dx),abs(dy))-1)*2+bool(blockers))
                rows.append(row);block.append(code(blockers[0],view) if blockers else -1)
            edges.append((s,t,rows,block))
    quiet=list(m for m in b.legal_moves if not b.is_capture(m))
    moves=[]
    for m in quiet:
        pc=board[m.from_square]
        kind=2+m.promotion if m.promotion else 3 if b.is_castling(m) else 0
        moves.append((plane(pc,m.from_square,b.turn),plane(pc,m.to_square,b.turn),kind))
    rights=lambda c:16*(int(b.has_kingside_castling_rights(c))+2*int(b.has_queenside_castling_rights(c)))
    meta=[min(b.halfmove_clock,255)//2,sum(PHASE[p.piece_type] for p in board.values()),
          rights(b.turn),rights(not b.turn),square(b.ep_square,b.turn)+1 if b.ep_square is not None else 0,
          len(board),int(b.is_check()),32]
    return dict(planes=planes,buckets=buckets,occupied=occupied,edges=edges,
                moves=moves,uci=[m.uci() for m in quiet],meta=np.array(meta,np.float32))

def collate(rows, device='cpu'):
    import torch
    feats=[r['_features'] if '_features' in r else board_features(r['fen']) for r in rows]
    n=len(rows);ne=max(1,max(len(x['edges']) for x in feats));nm=max(1,max(len(x['moves']) for x in feats))
    arrays=dict(planes=np.stack([x['planes'] for x in feats]),buckets=np.stack([x['buckets'] for x in feats]),
        occupied=np.stack([x['occupied'] for x in feats]),meta=np.stack([x['meta'] for x in feats]),
        edge_rows=np.zeros((n,2,ne),np.int64),blockers=np.zeros((n,2,ne),np.int64),
        blocked=np.zeros((n,2,ne),np.float32),edge_mask=np.zeros((n,ne),np.float32),
        src=np.zeros((n,ne),np.int64),dst=np.zeros((n,ne),np.int64),
        move_indices=np.zeros((n,nm,3),np.int64),move_mask=np.zeros((n,nm),bool),
        policy=np.zeros((n,nm),np.float32),wdl=np.array([r.get('wdl',[0,1,0]) for r in rows],np.float32))
    for i,(r,f) in enumerate(zip(rows,feats)):
        for j,(s,t,edge,block) in enumerate(f['edges']):
            arrays['src'][i,j]=s;arrays['dst'][i,j]=t;arrays['edge_mask'][i,j]=1
            arrays['edge_rows'][i,:,j]=edge;arrays['blockers'][i,:,j]=np.maximum(block,0)
            arrays['blocked'][i,:,j]=np.array(block)>=0
        count=len(f['moves'])
        if count:arrays['move_indices'][i,:count]=f['moves'];arrays['move_mask'][i,:count]=True
        target=r.get('quiet_policy',{})
        arrays['policy'][i,:count]=[target.get(u,0) for u in f['uci']]
        mass=arrays['policy'][i].sum()
        if mass>0: arrays['policy'][i]/=mass
    return {k:torch.as_tensor(v,device=device) for k,v in arrays.items()},feats
