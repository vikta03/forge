"""J78 fixtures and an independent board-identity repetition oracle. No searches."""
import json
from pathlib import Path
import chess

PLIES = (0, 1, 3, 4, 8, 64)

def cases(here):
    cycle = ['g1f3','g8f6','f3g1','f6g8']
    rows = [dict(id='knight-cycle',root_fen=chess.STARTING_FEN,moves=cycle*3,chess960=False),
            dict(id='null-boundary',root_fen=chess.STARTING_FEN,
                 moves=cycle*2+['0000','0000']+cycle*2,chess960=False)]
    for pinned in (True,False):
        fen = ('4r1k1/3p4/8/4P3/8/8/8/4K1N1 b - - 0 1' if pinned else
               '4r1k1/3p4/8/4P3/8/8/8/K5N1 b - - 0 1')
        rows.append(dict(id='ep-'+str(pinned),root_fen=fen,
                         moves=['d7d5']+['g1f3','g8h8','f3g1','h8g8']*3,chess960=False))
    rows += [dict(id='castling-rights',root_fen='4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1',
                  moves=['h1h2','e8e7','h2h1','e7e8']*3,chess960=False),
             dict(id='chess960-rights',root_fen='4k3/8/8/8/8/8/8/1R1K4 w B - 0 1',
                  moves=['b1b2','e8e7','b2b1','e7e8']*3,chess960=True),
             dict(id='promotion-reset',root_fen='4k3/1P6/8/8/8/8/8/4K1N1 w - - 0 1',
                  moves=['b7b8n','e8f7','g1f3','f7e8','f3g1','e8f7'],chess960=False),
             dict(id='check-evasion-cycle',root_fen='3k4/8/8/8/8/8/8/3RK3 b - - 0 1',
                  moves=['d8c8','d1c1','c8d8','c1d1']*3,chess960=False)]
    # Actual histories used in J77 and twelve independent ordinary games.
    for row in json.loads((here/'draw-positions.json').read_text()):
        rows.append(dict(id=row['id'],**row['context'],chess960=True))
    for row in json.loads((here/'timing-positions.json').read_text())[::2]:
        rows.append(dict(id='ordinary-'+row['id'],root_fen=chess.STARTING_FEN,
                         moves=row['moves'],chess960=False))
    return rows

def key(board):
    # Public python-chess legality/EP rules, independent of FORGE's hash keys.
    return ' '.join(board.fen(en_passant='legal').split()[:4])

def prepare(here, run):
    rows=cases(here)
    tsv=[]; expected={}; positives=0; states=0
    for row in rows:
        b=chess.Board(row['root_fen'],chess960=row['chess960'])
        assert b.is_valid(), (row['id'], b.fen())
        sequence=row.get('moves',[])
        tsv.append('\t'.join([row['id'],row['root_fen'],str(int(row['chess960'])),
                              ' '.join(sequence)]))
        history=[key(b)]; last_null=0
        for step in range(len(sequence)+1):
            usable={ply:[] for ply in PLIES}
            for move in list(b.legal_moves):
                uci=b.uci(move); b.push(move); child_key=key(b)
                child_index=step+1
                end=min(b.halfmove_clock,child_index-last_null)
                matches=[d for d in range(4,end+1,2) if history[child_index-d]==child_key]
                for ply in PLIES:
                    if len(matches)>=2 or (matches and min(matches)<ply+1):
                        usable[ply].append(uci)
                b.pop()
            for ply in PLIES:
                moves=sorted(usable[ply]); positives+=bool(moves)
                expected[(row['id'],step,ply)]=moves
            states+=1
            if step==len(sequence):break
            u=sequence[step]
            if u=='0000':
                assert not b.is_check(); b.push(chess.Move.null()); last_null=step+1
            else:b.push_uci(u)
            history.append(key(b))
    (run/'repetition.tsv').write_text('\n'.join(tsv)+'\n')
    (run/'repetition-fixtures.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert positives>=10, 'positive controls must actually exercise repetition'
    return expected,dict(histories=len(rows),states=states,queries=len(expected),positive_queries=positives)
