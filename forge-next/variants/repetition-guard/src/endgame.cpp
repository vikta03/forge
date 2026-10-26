// endgame.cpp — the KPK bitbase and the mate drivers.

#include "endgame.h"

#include <algorithm>
#include <bitset>
#include <vector>

#include "bitboard.h"
#include "eval_terms.h"
#include "position.h"
#include "syzygy/tbprobe.h"

namespace boring {

namespace {

using namespace Eval;

// Drive the losing king to the edge, and then to a corner. Values are in
// centipawns and are added on top of the winning side's material, so the search
// always prefers the line that shrinks the box fastest.
constexpr int PUSH_TO_EDGE[SQUARE_NB] = {
    100, 90, 80, 70, 70, 80, 90, 100,
     90, 70, 60, 50, 50, 60, 70,  90,
     80, 60, 40, 30, 30, 40, 60,  80,
     70, 50, 30, 20, 20, 30, 50,  70,
     70, 50, 30, 20, 20, 30, 50,  70,
     80, 60, 40, 30, 30, 40, 60,  80,
     90, 70, 60, 50, 50, 60, 70,  90,
    100, 90, 80, 70, 70, 80, 90, 100
};

// For king, bishop and knight: the losing king must be forced into a corner of
// the *bishop's* colour. This table is written for the a1/h8 (dark) corners and
// is mirrored when the bishop is light-squared.
constexpr int PUSH_TO_CORNER[SQUARE_NB] = {
    200, 190, 180, 170, 160, 150, 140, 130,
    190, 180, 170, 160, 150, 140, 130, 140,
    180, 170, 155, 140, 140, 125, 140, 150,
    170, 160, 140, 120, 110, 140, 150, 160,
    160, 150, 140, 110, 120, 140, 160, 170,
    150, 140, 125, 140, 140, 155, 170, 180,
    140, 130, 140, 150, 160, 170, 180, 190,
    130, 140, 150, 160, 170, 180, 190, 200
};

// Bringing the winning king closer is progress; in a few endings driving the
// losing king away is what matters instead.
constexpr int PUSH_CLOSE[8] = {0, 0, 100, 80, 60, 40, 20, 10};

// True when the two squares are of different colours.
bool opposite_colours(Square s1, Square s2) {
    return bool(DARK_SQUARES & s1) != bool(DARK_SQUARES & s2);
}

// ------------------------------------------------------------------- KPK
//
// King and pawn against king, solved exhaustively. Only White-to-promote
// positions with the pawn on files A-D are stored; every other case is reduced
// to one of those by mirroring, which is what makes 24 KB enough.

namespace KPK {

// bit  0- 5  white king square
// bit  6-11  black king square
// bit    12  side to move
// bit 13-14  pawn file, A-D
// bit 15-17  RANK_7 minus the pawn's rank
constexpr unsigned MAX_INDEX = 2 * 24 * 64 * 64;

std::bitset<MAX_INDEX> DB;

constexpr unsigned index(Color stm, Square bksq, Square wksq, Square psq) {
    return unsigned(wksq) | (unsigned(bksq) << 6) | (unsigned(stm) << 12)
         | (unsigned(file_of(psq)) << 13) | (unsigned(RANK_7 - rank_of(psq)) << 15);
}

enum Result { INVALID = 0, UNKNOWN = 1, DRAW = 2, WIN = 4 };

Result& operator|=(Result& r, Result v) { return r = Result(int(r) | int(v)); }

struct KPKPosition {
    KPKPosition() = default;
    explicit KPKPosition(unsigned idx);

    operator Result() const { return result; }
    Result classify(const std::vector<KPKPosition>& db);

    Color  stm    = WHITE;
    Square ksq[COLOR_NB] = {SQ_A1, SQ_A1};
    Square psq    = SQ_A1;
    Result result = INVALID;
};

KPKPosition::KPKPosition(unsigned idx) {
    ksq[WHITE] = Square((idx >> 0) & 0x3F);
    ksq[BLACK] = Square((idx >> 6) & 0x3F);
    stm        = Color((idx >> 12) & 0x01);
    psq        = make_square(File((idx >> 13) & 0x3), Rank(RANK_7 - ((idx >> 15) & 0x7)));

    // Impossible placements: kings touching, or something standing on the pawn,
    // or Black to move while already in check from the pawn.
    if (distance(ksq[WHITE], ksq[BLACK]) <= 1 || ksq[WHITE] == psq || ksq[BLACK] == psq
        || (stm == WHITE && (pawn_attacks_bb(WHITE, psq) & ksq[BLACK])))
        result = INVALID;

    // White to move with the pawn on the seventh, promoting safely: won.
    else if (stm == WHITE && rank_of(psq) == RANK_7 && ksq[WHITE] != psq + NORTH
             && (distance(ksq[BLACK], psq + NORTH) > 1
                 || (attacks_bb<KING>(ksq[WHITE]) & (psq + NORTH))))
        result = WIN;

    // Black to move, stalemated or able to take the pawn safely: drawn.
    else if (stm == BLACK
             && (!(attacks_bb<KING>(ksq[BLACK])
                   & ~(attacks_bb<KING>(ksq[WHITE]) | pawn_attacks_bb(WHITE, psq)))
                 || (attacks_bb<KING>(ksq[BLACK]) & ~attacks_bb<KING>(ksq[WHITE]) & psq)))
        result = DRAW;

    else
        result = UNKNOWN;
}

// Retrograde step: a position is won for White if any move reaches a won
// position, and drawn for Black if any move reaches a drawn one. Anything still
// referring to an unknown position stays unknown for another pass.
Result KPKPosition::classify(const std::vector<KPKPosition>& db) {
    const Result Good = (stm == WHITE ? WIN : DRAW);
    const Result Bad  = (stm == WHITE ? DRAW : WIN);

    Result   r = INVALID;
    Bitboard b = attacks_bb<KING>(ksq[stm]);

    while (b) {
        const Square s = pop_lsb(b);
        r |= (stm == WHITE ? Result(db[index(BLACK, ksq[BLACK], s, psq)])
                           : Result(db[index(WHITE, s, ksq[WHITE], psq)]));
    }

    if (stm == WHITE) {
        if (rank_of(psq) < RANK_7)
            r |= Result(db[index(BLACK, ksq[BLACK], ksq[WHITE], psq + NORTH)]);

        if (rank_of(psq) == RANK_2 && psq + NORTH != ksq[WHITE] && psq + NORTH != ksq[BLACK])
            r |= Result(db[index(BLACK, ksq[BLACK], ksq[WHITE], psq + NORTH + NORTH)]);
    }

    return result = (r & Good) ? Good : (r & UNKNOWN) ? UNKNOWN : Bad;
}

void init() {
    std::vector<KPKPosition> db(MAX_INDEX);

    for (unsigned idx = 0; idx < MAX_INDEX; ++idx)
        db[idx] = KPKPosition(idx);

    // Iterate until nothing changes. Each pass propagates one more ply of
    // knowledge backwards from the terminal positions.
    bool repeat = true;
    while (repeat) {
        repeat = false;
        for (unsigned idx = 0; idx < MAX_INDEX; ++idx)
            repeat |= (db[idx] == UNKNOWN && db[idx].classify(db) != UNKNOWN);
    }

    for (unsigned idx = 0; idx < MAX_INDEX; ++idx)
        if (db[idx] == WIN)
            DB.set(idx);
}

// White has the pawn; caller must have mirrored the position accordingly.
bool probe(Square wksq, Square psq, Square bksq, Color stm) {
    assert(file_of(psq) <= FILE_D);
    return DB[index(stm, bksq, wksq, psq)];
}

}  // namespace KPK

// ------------------------------------------------------------ mate drivers

// The winning side has enough to mate and the loser has nothing but his king.
// Score the material, then reward squeezing the box.
Value mate_with_material(const Position& pos, Color strong) {
    const Color  weak      = ~strong;
    const Square winnerKsq = pos.king_square(strong);
    const Square loserKsq  = pos.king_square(weak);

    Value result = pos.non_pawn_material(strong)
                 + pos.count<PAWN>(strong) * SIMPLE_PIECE_VALUE[PAWN]
                 + PUSH_TO_EDGE[loserKsq] + PUSH_CLOSE[distance(winnerKsq, loserKsq)];

    // Material that is definitely enough to force mate.
    if (pos.count<QUEEN>(strong) || pos.count<ROOK>(strong)
        || (pos.count<KNIGHT>(strong) && pos.count<BISHOP>(strong))
        || (pos.count<BISHOP>(strong) >= 2
            && opposite_colours(lsb(pos.pieces(strong, BISHOP)),
                                msb(pos.pieces(strong, BISHOP)))))
        result = std::min(Value(result + VALUE_KNOWN_WIN), VALUE_MATE_IN_MAX_PLY - 1);

    return result;
}

// King, bishop and knight against a bare king. The mate only exists in the two
// corners matching the bishop's colour, so the usual push-to-edge table would
// send the king to the wrong one and the fifty-move rule would arrive first.
Value mate_KBNK(const Position& pos, Color strong) {
    const Color  weak      = ~strong;
    const Square winnerKsq = pos.king_square(strong);
    Square       loserKsq  = pos.king_square(weak);
    const Square bishopSq  = lsb(pos.pieces(strong, BISHOP));

    // PUSH_TO_CORNER is written for the dark corners; mirror the losing king
    // horizontally when the bishop is light-squared so the same table applies.
    if (!opposite_colours(bishopSq, SQ_A1))
        loserKsq = flip_file(loserKsq);

    return VALUE_KNOWN_WIN + PUSH_CLOSE[distance(winnerKsq, loserKsq)]
         + PUSH_TO_CORNER[loserKsq];
}


// ------------------------------------------------------- Syzygy tablebases

bool SyzygyReady = false;

// The tablebase supplies the verdict; this supplies a gradient to convert
// along. Where the drivers above already apply, their score is reused so a
// bishop-and-knight win still heads for a corner of the bishop's colour.
Value tablebase_win_score(const Position& pos, Color strong) {
    if (pos.count(~strong) == 1 && !pos.count<PAWN>(strong)) {
        if (pos.count<BISHOP>(strong) == 1 && pos.count<KNIGHT>(strong) == 1
            && pos.non_pawn_material(strong)
                 == SIMPLE_PIECE_VALUE[BISHOP] + SIMPLE_PIECE_VALUE[KNIGHT])
            return mate_KBNK(pos, strong);
        if (pos.non_pawn_material(strong) >= SIMPLE_PIECE_VALUE[ROOK])
            return mate_with_material(pos, strong);
    }
    const Square winnerKsq = pos.king_square(strong);
    const Square loserKsq  = pos.king_square(~strong);
    return Value(VALUE_KNOWN_WIN + PUSH_TO_EDGE[loserKsq]
               + PUSH_CLOSE[distance(winnerKsq, loserKsq)]);
}

// True when the tables answered. Fathom's tb_probe_wdl() wrapper rejects any
// non-zero halfmove clock, which would refuse seven of every eight endgame
// positions, so the clock-independent implementation is called directly and
// the fifty-move horizon is handled the same way the drivers handle it.
bool probe_syzygy(const Position& pos, Value& value) {
    if (!SyzygyReady || popcount(pos.pieces()) > int(TB_LARGEST))
        return false;
    // Fathom cannot describe a position that may still castle.
    if (pos.can_castle(ANY_CASTLING))
        return false;

    const Square ep = pos.ep_square();
    const unsigned wdl = tb_probe_wdl_impl(
        pos.pieces(WHITE), pos.pieces(BLACK), pos.pieces(KING),
        pos.pieces(QUEEN), pos.pieces(ROOK), pos.pieces(BISHOP),
        pos.pieces(KNIGHT), pos.pieces(PAWN),
        ep == SQ_NONE ? 0u : unsigned(ep),
        pos.side_to_move() == WHITE);
    if (wdl == TB_RESULT_FAILED)
        return false;

    const Color us        = pos.side_to_move();
    const int   remaining = std::clamp(100 - pos.rule50_count(), 0, 100);
    switch (wdl) {
    case TB_WIN:
        value = Value(tablebase_win_score(pos, us) * remaining / 100);
        break;
    case TB_LOSS:
        value = Value(-(tablebase_win_score(pos, ~us) * remaining / 100));
        break;
    default:
        // A cursed win or a blessed loss is drawn under the fifty-move rule.
        value = VALUE_DRAW;
        break;
    }
    return true;
}

}  // namespace

bool Endgames::Enabled = true;

void Endgames::init() { KPK::init(); }

int Endgames::init_syzygy(const char* path) {
    if (SyzygyReady) {
        tb_free();
        SyzygyReady = false;
    }
    if (path == nullptr || *path == '\0')
        return 0;
    SyzygyReady = tb_init(path) && TB_LARGEST > 0;
    if (!SyzygyReady)
        tb_free();
    return SyzygyReady ? int(TB_LARGEST) : 0;
}

bool Endgames::insufficient_material(const Position& pos) {
    if (pos.pieces(PAWN) || pos.pieces(ROOK) || pos.pieces(QUEEN))
        return false;

    for (Color c : {WHITE, BLACK}) {
        const int minors = pos.count<KNIGHT>(c) + pos.count<BISHOP>(c);

        // A lone minor cannot mate. Two knights cannot force mate either, and
        // two bishops can only if they are on opposite colours.
        if (minors > 1) {
            if (pos.count<BISHOP>(c) >= 2
                && opposite_colours(lsb(pos.pieces(c, BISHOP)), msb(pos.pieces(c, BISHOP))))
                return false;
            if (pos.count<KNIGHT>(c) && pos.count<BISHOP>(c))
                return false;
            if (pos.count<KNIGHT>(c) > 2)
                return false;
        }
    }

    return true;
}

bool Endgames::probe(const Position& pos, Value& value) {
    // Reporting nothing leaves trace.knownEndgame false, so eval.cpp scores
    // the position with the network rather than returning this file's exact
    // answer. That is the whole point of the switch.
    if (!Enabled)
        return false;

    // Exact where the tables reach. Everything below answers the rest,
    // and answers everything when no tables are loaded.
    if (probe_syzygy(pos, value))
        return true;

    const int pieceCount = popcount(pos.pieces());

    if (pieceCount > 4)
        return false;

    if (insufficient_material(pos)) {
        value = VALUE_DRAW;
        return true;
    }

    // --- king and pawn against king ---
    if (pieceCount == 3 && pos.pieces(PAWN)) {
        const Color strong = pos.pieces(WHITE, PAWN) ? WHITE : BLACK;

        Square wksq = pos.king_square(strong);
        Square bksq = pos.king_square(~strong);
        Square psq  = lsb(pos.pieces(strong, PAWN));
        Color  stm  = pos.side_to_move();

        // The bitbase only stores White-to-promote with the pawn on the left
        // half of the board; fold every other case onto that.
        if (strong == BLACK) {
            wksq = flip_rank(wksq);
            bksq = flip_rank(bksq);
            psq  = flip_rank(psq);
            stm  = ~stm;
        }
        if (file_of(psq) > FILE_D) {
            wksq = flip_file(wksq);
            bksq = flip_file(bksq);
            psq  = flip_file(psq);
        }

        Value v;
        if (!KPK::probe(wksq, psq, bksq, stm))
            v = VALUE_DRAW;
        else
            // Won. Scale by how far the pawn has come so the search still
            // prefers pushing it.
            v = Value(VALUE_KNOWN_WIN + SIMPLE_PIECE_VALUE[PAWN] + 20 * int(rank_of(psq)));

        // Convert from "good for the strong side" to "good for the mover".
        value = (v == VALUE_DRAW)                      ? VALUE_DRAW
              : (pos.side_to_move() == strong)         ? v
                                                       : -v;
        return true;
    }

    // --- bare king against mating material ---
    for (Color strong : {WHITE, BLACK}) {
        const Color weak = ~strong;

        if (pos.count(weak) != 1 || pos.count<PAWN>(strong))
            continue;

        Value v;

        if (pos.count<BISHOP>(strong) == 1 && pos.count<KNIGHT>(strong) == 1
            && pos.non_pawn_material(strong)
                 == SIMPLE_PIECE_VALUE[BISHOP] + SIMPLE_PIECE_VALUE[KNIGHT])
            v = mate_KBNK(pos, strong);
        else if (pos.non_pawn_material(strong) >= SIMPLE_PIECE_VALUE[ROOK])
            v = mate_with_material(pos, strong);
        else
            continue;  // not enough to force mate; let the normal eval handle it

        // No pawn or capture remains to reset the clock. As the available
        // quiet plies disappear, a heuristic mate driver must approach the draw
        // it becomes at clock 100. Search still returns an actual checkmate
        // before testing the fifty-move draw, so a mate in the remaining plies
        // keeps its decisive search score.
        const int remaining = std::clamp(100 - pos.rule50_count(), 0, 100);
        v = Value(v * remaining / 100);

        value = (pos.side_to_move() == strong) ? v : -v;
        return true;
    }

    return false;
}

}  // namespace boring
