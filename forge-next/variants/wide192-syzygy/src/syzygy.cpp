#include "syzygy.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "movegen.h"
#include "syzygy/tbprobe.h"

namespace boring::Syzygy {
std::string Path;
bool RootProbe=true;
int ProbeLimit=7;
Stats stats;
namespace {
bool loaded=false;
constexpr Value KnownWin=VALUE_MATE_IN_MAX_PLY-1;
unsigned ep(const Position& pos) { return pos.ep_square()==SQ_NONE ? 0 : unsigned(pos.ep_square()); }
Value score(int outcome,int ply) { return outcome==0 ? VALUE_DRAW : outcome*(KnownWin-ply); }
Move decode(TbMove raw, const MoveList<LEGAL>& legal) {
    constexpr PieceType promotions[]={NO_PIECE_TYPE,QUEEN,ROOK,BISHOP,KNIGHT};
    const unsigned pr=TB_MOVE_PROMOTES(raw);
    if (pr>4) return MOVE_NONE;
    for (const auto& item:legal) {
        Move m=item;
        if (from_sq(m)!=TB_MOVE_FROM(raw) || to_sq(m)!=TB_MOVE_TO(raw)) continue;
        if (pr ? type_of(m)!=PROMOTION || promotion_type(m)!=promotions[pr]
               : type_of(m)==PROMOTION) continue;
        return m;
    }
    return MOVE_NONE;
}
}
void configure(const std::string& path) {
    // Caller stops search and clears TT before changing this process-wide state.
    // Fathom tb_init replaces/unmaps a previous path itself. tb_free is final
    // destruction in this vendored version; its storage cannot be re-inited.
    loaded=false; Path.clear(); reset();
    const bool ok=tb_init(path.c_str());
    if (path.empty() || path=="<empty>") return;
    if (!ok || TB_LARGEST==0) {
        tb_init("");
        throw std::runtime_error("SyzygyPath contains no usable tables; probing disabled");
    }
    loaded=true; Path=path;
    std::cout << "info string syzygy loaded maximum_cardinality " << TB_LARGEST
              << " (material coverage may be incomplete)" << std::endl;
}
bool eligible(const Position& pos) {
    return loaded && ProbeLimit>0 && !pos.can_castle(ANY_CASTLING)
        && popcount(pos.pieces())<=std::min<unsigned>(TB_LARGEST,ProbeLimit);
}
void reset() { stats=Stats{}; }
uint64_t hits() { return stats.rootHits+stats.mainHits+stats.qHits; }
void report() {
    if (!loaded) return;
    std::cout << "info string syzygy stats root_attempts " << stats.rootAttempts
              << " root_hits " << stats.rootHits << " attempts " << stats.attempts
              << " main_hits " << stats.mainHits << " q_hits " << stats.qHits
              << " failed " << stats.failed << std::endl;
    if (!stats.firstFen.empty())
        std::cout << "info string syzygy first_leaf wdl " << stats.firstWdl
                  << " fen " << stats.firstFen << std::endl;
}
bool interior(const Position& pos,int ply,bool quiescence,Value& value) {
    // WDL is exact at a zero halfmove clock. Never disguise clock-independent
    // WDL as a rule-50-aware answer at an arbitrary nonzero clock.
    if (!eligible(pos) || pos.rule50_count()!=0) return false;
    // Checkmate is a rules terminal, stronger than a generic tablebase loss.
    // Preserve the actual mate distance when a capture just reset the clock.
    if (pos.checkers() && !has_legal_move(pos)) {
        value=mated_in(ply);
        return true;
    }
    ++stats.attempts;
    const unsigned w=tb_probe_wdl(pos.pieces(WHITE),pos.pieces(BLACK),pos.pieces(KING),
        pos.pieces(QUEEN),pos.pieces(ROOK),pos.pieces(BISHOP),pos.pieces(KNIGHT),
        pos.pieces(PAWN),0,0,ep(pos),pos.side_to_move()==WHITE);
    if (w==TB_RESULT_FAILED) { ++stats.failed; return false; }
    if (quiescence) ++stats.qHits; else ++stats.mainHits;
    if (stats.firstFen.empty()) { stats.firstFen=pos.fen(); stats.firstWdl=int(w)-2; }
    value=score(w==TB_WIN ? 1 : w==TB_LOSS ? -1 : 0,ply);
    return true;
}
bool root(Position& pos,const std::vector<Move>& allowed,RootResult& result) {
    if (!RootProbe || !eligible(pos) || allowed.empty()) return false;
    ++stats.rootAttempts;
    TbRootMoves results{};
    // Always request the conservative ranking used after repetition: shortest
    // distance to the next zeroing move, even when there was no repetition.
    // This prevents a series of arbitrary equal-win choices making no progress.
    if (!tb_probe_root_dtz(pos.pieces(WHITE),pos.pieces(BLACK),pos.pieces(KING),
            pos.pieces(QUEEN),pos.pieces(ROOK),pos.pieces(BISHOP),pos.pieces(KNIGHT),
            pos.pieces(PAWN),pos.rule50_count(),0,ep(pos),pos.side_to_move()==WHITE,
            true,true,&results)) { ++stats.failed; return false; }
    const MoveList<LEGAL> legal(pos);
    if (results.size!=legal.size()) { ++stats.failed; return false; }
    int bestRank=-2000;
    for (unsigned i=0;i<results.size;++i) {
        const auto& row=results.moves[i];
        Move m=decode(row.move,legal);
        if (m==MOVE_NONE) { ++stats.failed; return false; }
        if (std::find(allowed.begin(),allowed.end(),m)==allowed.end()) continue;
        int rank=row.tbRank;
        bool mate=false;
        if (pos.is_draw(0)) rank=0;
        else {
            StateInfo state;
            pos.do_move(m,state);
            mate=pos.checkers() && !has_legal_move(pos);
            if (pos.is_draw(1)) rank=0; // actual game history can enable a claim
            pos.undo_move(m);
        }
        if (rank>bestRank) {
            bestRank=rank; result.move=m; result.rank=rank;
            result.outcome=rank>900 ? 1 : rank<-900 ? -1 : 0;
            result.value=mate ? mate_in(1) : score(result.outcome,0);
        }
    }
    // Fathom documents a rare one-ply DTZ rounding ambiguity at this boundary.
    // Fall back to ordinary search rather than certify it as win/draw/loss.
    if (result.move==MOVE_NONE || std::abs(bestRank)==900) { ++stats.failed; return false; }
    ++stats.rootHits;
    return true;
}
}
