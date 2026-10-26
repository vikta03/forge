
#ifdef FORGE_NEXT_PROBE
#include "forge_next_probe.h"
#endif
// position.cpp — board state, move execution, and the queries the search and
// evaluation ask of a position.
//
// Chess960 is supported. Castling geometry is per-position rather than fixed,
// and castling moves are encoded internally as "king takes own rook", because
// in 960 the king's destination alone is ambiguous — a king on b1 with a rook
// on c1 castles to g1, but b1c1 is also a perfectly ordinary king move. The
// UCI layer converts to king-destination notation for standard chess.

#include "position.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>

#include "eval_terms.h"
#include "movegen.h"

namespace boring {

namespace Search { extern bool SpecialMoveSEE; }

const std::string StartFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace Zobrist {
Key PSQ[PIECE_NB][SQUARE_NB];
Key EN_PASSANT[FILE_NB];
Key CASTLING[CASTLING_RIGHTS_NB];
Key SIDE;
}  // namespace Zobrist

namespace {

constexpr std::string_view PIECE_TO_CHAR = " PNBRQK  pnbrqk";

// Reversible-move key differences. The move-delta idea and alternating-history
// filter follow Stockfish / Marcel van Kervinck's repetition algorithm (GPLv3;
// see REPETITION_PROVENANCE.md). This implementation uses a simple open-addressed
// table, FORGE's normalized repetition identity, and full move legality checks.
constexpr size_t REP_SIZE = 8192;
std::array<Key, REP_SIZE> repetitionKeys{};
std::array<uint16_t, REP_SIZE> repetitionMoves{};

size_t repetition_slot(Key key) { return (key ^ (key >> 32)) & (REP_SIZE - 1); }

Move reversible_move(Key key) {
    for (size_t slot = repetition_slot(key); repetitionMoves[slot] != MOVE_NONE;
         slot = (slot + 1) & (REP_SIZE - 1))
        if (repetitionKeys[slot] == key)
            return Move(repetitionMoves[slot]);
    return MOVE_NONE;
}

// A 64-bit PRNG, seeded so the Zobrist keys are identical on every run. That
// matters: a reproducible key set makes bench node counts reproducible too.
class PRNG {
    uint64_t s;

   public:
    explicit PRNG(uint64_t seed) : s(seed) { assert(seed); }

    uint64_t rand() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }
};

}  // namespace

void Position::init() {
    PRNG rng(1070372ULL);

    for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING, B_PAWN, B_KNIGHT,
                     B_BISHOP, B_ROOK, B_QUEEN, B_KING})
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            Zobrist::PSQ[pc][s] = rng.rand();

    for (File f = FILE_A; f <= FILE_H; ++f)
        Zobrist::EN_PASSANT[f] = rng.rand();

    // Give every combination of rights its own key by xoring independent
    // per-right keys, so incremental updates compose correctly.
    Key rightKey[4];
    for (int i = 0; i < 4; ++i)
        rightKey[i] = rng.rand();
    for (int cr = 0; cr < CASTLING_RIGHTS_NB; ++cr) {
        Zobrist::CASTLING[cr] = 0;
        for (int i = 0; i < 4; ++i)
            if (cr & (1 << i))
                Zobrist::CASTLING[cr] ^= rightKey[i];
    }

    Zobrist::SIDE = rng.rand();

    repetitionKeys.fill(0);
    repetitionMoves.fill(MOVE_NONE);
    for (Piece pc : {W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
                     B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING})
        for (Square from = SQ_A1; from <= SQ_H8; ++from)
            for (Square to = Square(int(from) + 1); to <= SQ_H8; ++to)
                if (attacks_bb(type_of(pc), from, 0) & to) {
                    const Key key = Zobrist::PSQ[pc][from] ^ Zobrist::PSQ[pc][to]
                                  ^ Zobrist::SIDE;
                    size_t slot = repetition_slot(key);
                    while (repetitionMoves[slot] != MOVE_NONE)
                        slot = (slot + 1) & (REP_SIZE - 1);
                    repetitionKeys[slot] = key;
                    repetitionMoves[slot] = make_move(from, to);
                }
}

// ------------------------------------------------------------------ setup

Position& Position::set(const std::string& fenStr, StateInfo* si, bool isChess960) {
    // FEN fields:
    //  1. piece placement, rank 8 first, '/' between ranks, digits = empty runs
    //  2. side to move
    //  3. castling availability
    //  4. en passant target square
    //  5. halfmove clock
    //  6. fullmove number
    // Fields 5 and 6 are optional; plenty of real-world FENs omit them.

    unsigned char      token;
    Square             sq = SQ_A8;
    std::istringstream ss(fenStr);

    std::memset(this, 0, sizeof(Position));
    st       = si;
    *st      = StateInfo{};
    chess960 = isChess960;

    ss >> std::noskipws;

    // 1. Pieces
    while ((ss >> token) && !isspace(token)) {
        if (isdigit(token))
            sq += (token - '0') * EAST;
        else if (token == '/')
            sq += 2 * SOUTH;
        else {
            const size_t idx = PIECE_TO_CHAR.find(token);
            if (idx != std::string_view::npos) {
                put_piece(Piece(idx), sq);
                ++sq;
            }
        }
    }

    // 2. Side to move
    ss >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;

    // 3. Castling rights. Two notations are accepted, because both are in use:
    //
    //   X-FEN       KQkq      — the *outermost* rook on that side of the king
    //   Shredder    AHah      — the rook's file, named explicitly
    //
    // Standard chess is just the case where the outermost rooks happen to be on
    // a1/h1, so the same code handles both games.
    while ((ss >> token) && !isspace(token)) {
        const Color c    = std::islower(token) ? BLACK : WHITE;
        const Piece rook = make_piece(c, ROOK);
        const char  t    = char(std::toupper(token));

        Square rsq;

        if (t == 'K')
            for (rsq = relative_square(c, SQ_H1); piece_on(rsq) != rook; --rsq) {}
        else if (t == 'Q')
            for (rsq = relative_square(c, SQ_A1); piece_on(rsq) != rook; ++rsq) {}
        else if (t >= 'A' && t <= 'H')
            rsq = make_square(File(t - 'A'), relative_rank(c, RANK_1));
        else
            continue;  // '-' or junk

        set_castling_right(c, rsq);
    }

    // 4. Geometrically available EP, retained for compatibility with the
    // trained input. compute_repetition_key() separately checks king safety.
    st->epSquare = SQ_NONE;
    unsigned char col, row;
    if (((ss >> col) && (col >= 'a' && col <= 'h')) && ((ss >> row) && (row == '3' || row == '6'))) {
        st->epSquare = make_square(File(col - 'a'), Rank(row - '1'));

        const Color them = ~sideToMove;
        if (!(attackers_to(st->epSquare) & pieces(sideToMove, PAWN))
            || !(pieces(them, PAWN) & (st->epSquare + pawn_push(them)))
            || (pieces() & (square_bb(st->epSquare) | (st->epSquare + pawn_push(sideToMove)))))
            st->epSquare = SQ_NONE;
    }

    // 5-6. Clocks. Absent fields simply leave the defaults in place.
    int halfmove = 0, fullmove = 1;
    ss >> std::skipws >> halfmove >> fullmove;
    if (ss.fail()) {
        halfmove = 0;
        fullmove = 1;
    }
    st->rule50 = halfmove;

    // gamePly counts plies from the start of the game, even-numbered for White.
    gamePly = std::max(2 * (fullmove - 1), 0) + (sideToMove == BLACK);

    set_state();

#ifdef FORGE_NEXT_PROBE
    ForgeNext::reset(*this);
#endif

    assert(pos_is_ok());
    return *this;
}

// Recompute everything derived from the piece placement. Only called from set();
// do_move() maintains all of this incrementally.
void Position::set_state() const {
    st->key = st->materialKey = st->pawnKey = st->minorPieceKey = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->nonPawnMaterial[WHITE] = st->nonPawnMaterial[BLACK] = VALUE_ZERO;
    st->psq                    = Score();
    st->phase                  = 0;
    st->checkersBB             = attackers_to(king_square(sideToMove)) & pieces(~sideToMove);
    st->repetition             = 0;
    st->pliesFromNull          = 0;
    st->previous               = nullptr;
    st->capturedPiece          = NO_PIECE;

    set_check_info();

    for (Bitboard b = pieces(); b;) {
        const Square s  = pop_lsb(b);
        const Piece  pc = piece_on(s);

        st->key ^= Zobrist::PSQ[pc][s];
        st->psq += Eval::PSQ[pc][s];
        st->phase += Eval::PHASE_VALUE[type_of(pc)];

        if (type_of(pc) == PAWN)
            st->pawnKey ^= Zobrist::PSQ[pc][s];
        else {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::PSQ[pc][s];
            if (type_of(pc) == KNIGHT || type_of(pc) == BISHOP)
                st->minorPieceKey ^= Zobrist::PSQ[pc][s];
            if (type_of(pc) != KING)
                st->nonPawnMaterial[color_of(pc)]
                    += Eval::SIMPLE_PIECE_VALUE[type_of(pc)];
        }
    }

    // Material key: for each piece kind, fold in one key per copy present.
    for (Piece pc : {W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING, B_PAWN, B_KNIGHT,
                     B_BISHOP, B_ROOK, B_QUEEN, B_KING})
        for (int cnt = 0; cnt < pieceCount[pc]; ++cnt)
            st->materialKey ^= Zobrist::PSQ[pc][cnt];

    if (st->epSquare != SQ_NONE)
        st->key ^= Zobrist::EN_PASSANT[file_of(st->epSquare)];

    if (sideToMove == BLACK)
        st->key ^= Zobrist::SIDE;

    st->key ^= Zobrist::CASTLING[st->castlingRights];
    st->repetitionKey = compute_repetition_key();
}

// Record one castling right and everything derived from it. Called once per
// right while parsing the FEN, since in Chess960 all of this depends on where
// the king and that rook actually stand.

void Position::set_castling_right(Color c, Square rfrom) {
    const Square         kfrom = king_square(c);
    const CastlingRights cr    = c & (kfrom < rfrom ? KING_SIDE : QUEEN_SIDE);

    st->castlingRights |= cr;
    castlingRightsMask[kfrom] |= cr;
    castlingRightsMask[rfrom] |= cr;
    castlingRookSquare[cr] = rfrom;

    const Square kto = castling_king_to(c, cr & KING_SIDE);
    const Square rto = castling_rook_to(c, cr & KING_SIDE);

    // Every square the king or the rook has to pass through or land on, minus
    // the two squares they already occupy. BETWEEN_BB includes its second
    // argument, so each term is the half-open segment from origin to
    // destination; excluding the origins is what allows the king and rook to
    // stand in each other's way, which they routinely do in 960.
    castlingPath[cr] = (BETWEEN_BB[rfrom][rto] | BETWEEN_BB[kfrom][kto])
                     & ~(square_bb(kfrom) | square_bb(rfrom));
}

std::string Position::fen() const {
    std::ostringstream ss;

    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f) {
            int emptyCnt = 0;
            while (f <= FILE_H && empty(make_square(f, r))) {
                ++emptyCnt;
                ++f;
            }
            if (emptyCnt)
                ss << emptyCnt;
            if (f <= FILE_H)
                ss << PIECE_TO_CHAR[piece_on(make_square(f, r))];
        }
        if (r > RANK_1)
            ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    // Shredder notation names the rook's file; standard chess uses KQkq. Emit
    // whichever matches the game being played, so the FEN round-trips.
    if (can_castle(WHITE_OO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OO))) : 'K');
    if (can_castle(WHITE_OOO))
        ss << (chess960 ? char('A' + file_of(castling_rook_square(WHITE_OOO))) : 'Q');
    if (can_castle(BLACK_OO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OO))) : 'k');
    if (can_castle(BLACK_OOO))
        ss << (chess960 ? char('a' + file_of(castling_rook_square(BLACK_OOO))) : 'q');
    if (!st->castlingRights)
        ss << '-';

    ss << ' ';
    if (st->epSquare == SQ_NONE)
        ss << '-';
    else
        ss << char('a' + file_of(st->epSquare)) << char('1' + rank_of(st->epSquare));

    ss << ' ' << st->rule50 << ' ' << (1 + (gamePly - (sideToMove == BLACK)) / 2);

    return ss.str();
}

// ------------------------------------------------------- attacks and pins

Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
    return (pawn_attacks_bb(BLACK, s) & pieces(WHITE, PAWN))
         | (pawn_attacks_bb(WHITE, s) & pieces(BLACK, PAWN))
         | (attacks_bb<KNIGHT>(s) & pieces(KNIGHT))
         | (attacks_bb<ROOK>(s, occupied) & pieces(ROOK, QUEEN))
         | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP, QUEEN))
         | (attacks_bb<KING>(s) & pieces(KING));
}

// Pieces that stand between a slider in `sliders` and the square `s`. A blocker
// of our own colour is pinned; one of the enemy's is a discovered-attack
// candidate. Both are returned; `pinners` receives only the sliders whose
// blocker belongs to the side occupying `s`.
Bitboard Position::slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const {
    Bitboard blockers = 0;
    pinners           = 0;

    // Sliders that would hit s on an otherwise empty board.
    Bitboard snipers = ((attacks_bb<ROOK>(s, 0) & pieces(QUEEN, ROOK))
                        | (attacks_bb<BISHOP>(s, 0) & pieces(QUEEN, BISHOP)))
                     & sliders;
    const Bitboard occupancy = pieces() ^ snipers;

    while (snipers) {
        const Square   sniperSq = pop_lsb(snipers);
        const Bitboard b        = BETWEEN_BB[s][sniperSq] & occupancy;

        if (b && !more_than_one(b)) {
            blockers |= b;
            if (b & pieces(color_of(piece_on(s))))
                pinners |= sniperSq;
        }
    }
    return blockers;
}

void Position::set_check_info() const {
    st->blockersForKing[WHITE] =
        slider_blockers(pieces(BLACK), king_square(WHITE), st->pinners[BLACK]);
    st->blockersForKing[BLACK] =
        slider_blockers(pieces(WHITE), king_square(BLACK), st->pinners[WHITE]);

    // Squares from which the side to move would check the enemy king.
    const Square ksq = king_square(~sideToMove);

    st->checkSquares[PAWN]   = pawn_attacks_bb(~sideToMove, ksq);
    st->checkSquares[KNIGHT] = attacks_bb<KNIGHT>(ksq);
    st->checkSquares[BISHOP] = attacks_bb<BISHOP>(ksq, pieces());
    st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st->checkSquares[QUEEN]  = st->checkSquares[BISHOP] | st->checkSquares[ROOK];
    st->checkSquares[KING]   = 0;
}

// ------------------------------------------------------- move properties

// Assumes `m` is pseudo-legal. Only the cases where the mover's own king can be
// left in check need real work.
bool Position::legal(Move m) const {
    assert(is_ok(m));

    const Color  us   = sideToMove;
    const Square from = from_sq(m);
    const Square to   = to_sq(m);

    assert(color_of(moved_piece(m)) == us);
    assert(piece_on(king_square(us)) == make_piece(us, KING));

    // En passant removes a pawn from a square the move never touches, so the
    // usual pin test does not cover it. Check the resulting occupancy directly:
    // this is the classic case of a rook on the fifth rank skewering two pawns.
    if (type_of(m) == EN_PASSANT) {
        const Square   ksq      = king_square(us);
        const Square   capsq    = to - pawn_push(us);
        const Bitboard occupied = (pieces() ^ from ^ capsq) | to;

        return !(attacks_bb<ROOK>(ksq, occupied) & pieces(~us, QUEEN, ROOK))
            && !(attacks_bb<BISHOP>(ksq, occupied) & pieces(~us, QUEEN, BISHOP));
    }

    // Castling. `to` is our own rook, so derive the king's real destination and
    // walk the squares he crosses; castling is only ever generated when we are
    // not already in check, so the starting square needs no test.
    if (type_of(m) == CASTLING) {
        const Square    kto  = castling_king_to(us, to > from);
        const Direction step = (kto > from) ? WEST : EAST;

        for (Square s = kto; s != from; s += step)
            if (attackers_to(s) & pieces(~us))
                return false;

        // Chess960 only: the castling rook itself can be the piece shielding
        // our king from a slider, and moving it would expose him. The classic
        // case is an enemy queen on a1 with our rook on b1 and our king on c1,
        // where the king does not move at all but the rook steps out of the way.
        // In standard chess the rook is never on such a line.
        return !chess960 || !(blockers_for_king(us) & to);
    }

    // A king move is legal iff the destination is unattacked once the king has
    // stepped off its square — the king itself must not block the attacking ray.
    if (type_of(piece_on(from)) == KING)
        return !(attackers_to(to, pieces() ^ from) & pieces(~us));

    // Anything else: legal unless it is pinned and leaves the pin ray.
    return !(blockers_for_king(us) & from) || aligned(from, to, king_square(us));
}

// Full validation of a move that came from an untrusted source: a transposition
// table hit, a killer, or a countermove. Those are 16-bit integers that may have
// been produced for a completely different position.
bool Position::pseudo_legal(Move m) const {
    const Color  us   = sideToMove;
    const Square from = from_sq(m);
    const Square to   = to_sq(m);
    const Piece  pc   = moved_piece(m);

    if (!is_ok(m))
        return false;

    // Non-normal moves are rare enough that regenerating is cheaper than
    // duplicating their rules here.
    if (type_of(m) != NORMAL)
        return MoveList<LEGAL>(*this).contains(m);

    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    if (pieces(us) & to)
        return false;

    if (type_of(pc) == PAWN) {
        // A NORMAL pawn move can never reach the last rank; that would have to
        // be encoded as a promotion.
        if ((RANK_8BB | RANK_1BB) & to)
            return false;

        const Direction up = pawn_push(us);

        const bool isCapture  = bool(pawn_attacks_bb(us, from) & pieces(~us) & to);
        const bool single     = (from + up == to) && empty(to);
        const bool doublePush = (from + up + up == to) && relative_rank(us, from) == RANK_2
                             && empty(to) && empty(from + up);

        if (!isCapture && !single && !doublePush)
            return false;
    } else if (!(attacks_bb(type_of(pc), from, pieces()) & to))
        return false;

    // Being in check restricts things enough that regenerating is simplest and
    // is not on any hot path.
    if (checkers()) {
        if (type_of(pc) != KING) {
            // Double check can only be answered by a king move.
            if (more_than_one(checkers()))
                return false;
            // Otherwise capture the checker or interpose.
            if (!(BETWEEN_BB[king_square(us)][lsb(checkers())] & to))
                return false;
        } else if (attackers_to(to, pieces() ^ from) & pieces(~us))
            return false;
    }

    return true;
}

bool Position::gives_check(Move m) const {
    assert(is_ok(m));
    assert(color_of(moved_piece(m)) == sideToMove);

    const Square from = from_sq(m);
    const Square to   = to_sq(m);
    const Square ksq  = king_square(~sideToMove);

    // Direct check: the piece lands on a square that attacks the enemy king.
    if (check_squares(type_of(piece_on(from))) & to)
        return true;

    // Discovered check: the mover was shielding the enemy king and steps off
    // the line. Staying on the line (a rook sliding along it) is not discovery.
    if ((blockers_for_king(~sideToMove) & from) && !aligned(from, to, ksq))
        return true;

    switch (type_of(m)) {
    case NORMAL:
        return false;

    case PROMOTION:
        // The new piece attacks from `to` on a board where the pawn has left.
        return bool(attacks_bb(promotion_type(m), to, pieces() ^ from) & ksq);

    case EN_PASSANT: {
        // Two pawns vanish from the fifth rank at once, which can open a line
        // that neither the direct nor the discovered test above would see.
        const Square   capsq    = make_square(file_of(to), rank_of(from));
        const Bitboard occupied = (pieces() ^ from ^ capsq) | to;

        return bool((attacks_bb<ROOK>(ksq, occupied) & pieces(sideToMove, QUEEN, ROOK))
                    | (attacks_bb<BISHOP>(ksq, occupied) & pieces(sideToMove, QUEEN, BISHOP)));
    }

    default: {
        // CASTLING — the king cannot give check, so only the rook matters. The
        // check_squares table is no use here: it was built on the current
        // occupancy, where our own king still stands on `from`. When he is the
        // piece screening the enemy king from the rook's destination, that
        // table reports "no check" for a move that removes the screen — and the
        // discovered-check test above misses it too, because king and rook both
        // stay on the line. Recompute on the occupancy the move actually leaves
        // behind: king and rook vacate `from` and `to`, and arrive at kto/rto.
        const bool     kingSide = to > from;
        const Square   rto      = castling_rook_to(sideToMove, kingSide);
        const Square   kto      = castling_king_to(sideToMove, kingSide);
        const Bitboard occupied = (pieces() ^ from ^ to) | rto | kto;

        return bool(attacks_bb<ROOK>(rto, occupied) & ksq);
    }
    }
}

// ------------------------------------------------------ doing and undoing

// `to` arrives as the rook's square (king-takes-own-rook encoding) and leaves
// as the king's real destination, which is what do_move() then uses for the
// Zobrist and piece-square updates.
template<bool Do>
void Position::do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto) {
    const bool kingSide = to > from;

    rfrom = to;
    rto   = castling_rook_to(us, kingSide);
    to    = castling_king_to(us, kingSide);

    // Both pieces have to come off before either goes down. In Chess960 the
    // origins and destinations overlap in every combination — the king and rook
    // can swap squares, or the king can end up where he started — and placing
    // one piece before lifting the other would corrupt the board.
    remove_piece(Do ? from : to);
    remove_piece(Do ? rfrom : rto);

    put_piece(make_piece(us, KING), Do ? to : from);
    put_piece(make_piece(us, ROOK), Do ? rto : rfrom);
}

// Approximate key of the position after m, for prefetching only.
Key Position::key_after(Move m) const {
    const Square from = from_sq(m), to = to_sq(m);
    const Piece  pc   = piece_on(from);
    const bool   ep   = type_of(m) == EN_PASSANT;
    const Square capsq = ep ? Square(to - pawn_push(sideToMove)) : to;
    const Piece  captured = ep ? make_piece(~sideToMove, PAWN) : piece_on(capsq);

    Key k = st->key ^ Zobrist::SIDE;
    if (captured != NO_PIECE)
        k ^= Zobrist::PSQ[captured][capsq];
    return k ^ Zobrist::PSQ[pc][to] ^ Zobrist::PSQ[pc][from];
}

void Position::do_move(Move m, StateInfo& newSt, bool givesCheck) {
    assert(is_ok(m));
    assert(&newSt != st);

#ifdef FORGE_NEXT_PROBE
    ForgeNext::mark_move(*this, m);
#endif

    ++nodes;
    Key k = st->key ^ Zobrist::SIDE;

    // Carry the irreversible state forward, then adjust it below.
    newSt.pawnKey                = st->pawnKey;
    newSt.minorPieceKey          = st->minorPieceKey;
    newSt.nonPawnKey[WHITE]      = st->nonPawnKey[WHITE];
    newSt.nonPawnKey[BLACK]      = st->nonPawnKey[BLACK];
    newSt.materialKey            = st->materialKey;
    newSt.nonPawnMaterial[WHITE] = st->nonPawnMaterial[WHITE];
    newSt.nonPawnMaterial[BLACK] = st->nonPawnMaterial[BLACK];
    newSt.castlingRights         = st->castlingRights;
    newSt.rule50                 = st->rule50;
    newSt.pliesFromNull          = st->pliesFromNull;
    newSt.epSquare               = st->epSquare;
    newSt.psq                    = st->psq;
    newSt.phase                  = st->phase;

    newSt.previous = st;
    st             = &newSt;

    ++gamePly;
    ++st->rule50;
    ++st->pliesFromNull;

    const Color  us   = sideToMove;
    const Color  them = ~us;
    const Square from = from_sq(m);
    Square       to   = to_sq(m);

    const Piece pc       = piece_on(from);
    Piece       captured = (type_of(m) == EN_PASSANT) ? make_piece(them, PAWN) : piece_on(to);

    assert(color_of(pc) == us);
    assert(captured == NO_PIECE || color_of(captured) == them || type_of(m) == CASTLING);

    if (type_of(m) == CASTLING) {
        Square      rfrom, rto;
        const Piece rook = make_piece(us, ROOK);

        do_castling<true>(us, from, to, rfrom, rto);

        k ^= Zobrist::PSQ[rook][rfrom] ^ Zobrist::PSQ[rook][rto];
        st->nonPawnKey[us] ^= Zobrist::PSQ[rook][rfrom]
                                ^ Zobrist::PSQ[rook][rto];
        st->psq += Eval::PSQ[rook][rto] - Eval::PSQ[rook][rfrom];

        captured = NO_PIECE;  // the "captured" rook is our own; nothing is taken
    }

    if (captured) {
        Square capsq = to;

        if (type_of(captured) == PAWN) {
            if (type_of(m) == EN_PASSANT)
                capsq -= pawn_push(us);
            st->pawnKey ^= Zobrist::PSQ[captured][capsq];
        } else {
            st->nonPawnKey[them] ^= Zobrist::PSQ[captured][capsq];
            if (type_of(captured) == KNIGHT || type_of(captured) == BISHOP)
                st->minorPieceKey ^= Zobrist::PSQ[captured][capsq];
            st->nonPawnMaterial[them] -= Eval::SIMPLE_PIECE_VALUE[type_of(captured)];
        }

        remove_piece(capsq);

        k ^= Zobrist::PSQ[captured][capsq];
        st->psq -= Eval::PSQ[captured][capsq];
        st->phase -= Eval::PHASE_VALUE[type_of(captured)];
        st->materialKey ^= Zobrist::PSQ[captured][pieceCount[captured]];
        st->rule50 = 0;
    }

    k ^= Zobrist::PSQ[pc][from] ^ Zobrist::PSQ[pc][to];
    if (type_of(pc) != PAWN) {
        st->nonPawnKey[us] ^= Zobrist::PSQ[pc][from] ^ Zobrist::PSQ[pc][to];
        if (type_of(pc) == KNIGHT || type_of(pc) == BISHOP)
            st->minorPieceKey ^= Zobrist::PSQ[pc][from] ^ Zobrist::PSQ[pc][to];
    }

    // Any move clears the previous en passant square.
    if (st->epSquare != SQ_NONE) {
        k ^= Zobrist::EN_PASSANT[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    // Moving a king or a rook, or capturing a rook on its home square, can
    // revoke castling rights.
    if (st->castlingRights && (castlingRightsMask[from] | castlingRightsMask[to])) {
        k ^= Zobrist::CASTLING[st->castlingRights];
        st->castlingRights &= ~(castlingRightsMask[from] | castlingRightsMask[to]);
        k ^= Zobrist::CASTLING[st->castlingRights];
    }

    if (type_of(m) != CASTLING)
        move_piece(from, to);

    st->psq += Eval::PSQ[pc][to] - Eval::PSQ[pc][from];

    if (type_of(pc) == PAWN) {
        // A double push only creates an en passant square if an enemy pawn is
        // actually placed to take. Recording it unconditionally would give two
        // otherwise identical positions different hash keys.
        if ((int(to) ^ int(from)) == 16
            && (pawn_attacks_bb(us, to - pawn_push(us)) & pieces(them, PAWN))) {
            st->epSquare = to - pawn_push(us);
            k ^= Zobrist::EN_PASSANT[file_of(st->epSquare)];
        } else if (type_of(m) == PROMOTION) {
            const PieceType promo      = promotion_type(m);
            const Piece     promoPiece = make_piece(us, promo);

            remove_piece(to);
            put_piece(promoPiece, to);

            k ^= Zobrist::PSQ[pc][to] ^ Zobrist::PSQ[promoPiece][to];
            st->pawnKey ^= Zobrist::PSQ[pc][to];
            st->materialKey ^= Zobrist::PSQ[promoPiece][pieceCount[promoPiece] - 1]
                             ^ Zobrist::PSQ[pc][pieceCount[pc]];

            st->psq += Eval::PSQ[promoPiece][to] - Eval::PSQ[pc][to];
            st->phase += Eval::PHASE_VALUE[promo];
            st->nonPawnMaterial[us] += Eval::SIMPLE_PIECE_VALUE[promo];
            st->nonPawnKey[us] ^= Zobrist::PSQ[promoPiece][to];
            if (promo == KNIGHT || promo == BISHOP)
                st->minorPieceKey ^= Zobrist::PSQ[promoPiece][to];
        }

        st->pawnKey ^= Zobrist::PSQ[pc][from] ^ Zobrist::PSQ[pc][to];
        st->rule50 = 0;
    }

    st->capturedPiece = captured;
    st->key           = k;

    sideToMove = ~sideToMove;
    set_check_info();
    st->checkersBB =
        givesCheck ? attackers_to(king_square(sideToMove)) & pieces(~sideToMove) : Bitboard(0);
    st->repetitionKey = compute_repetition_key();

    // Repetition distance. A positive value means this is the first repeat of a
    // position seen `repetition` plies ago; negative means that earlier
    // occurrence was itself a repeat, i.e. we are at a threefold.
    st->repetition  = 0;
    const int end   = std::min(st->rule50, st->pliesFromNull);
    if (end >= 4) {
        StateInfo* stp = st->previous->previous;
        for (int i = 4; i <= end; i += 2) {
            stp = stp->previous->previous;
            if (stp->repetitionKey == st->repetitionKey) {
                st->repetition = stp->repetition ? -i : i;
                break;
            }
        }
    }

    assert(pos_is_ok());
}

void Position::undo_move(Move m) {
    assert(is_ok(m));

    sideToMove = ~sideToMove;

    const Color  us   = sideToMove;
    const Square from = from_sq(m);
    Square       to   = to_sq(m);

    if (type_of(m) == PROMOTION) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    }

    if (type_of(m) == CASTLING) {
        Square rfrom, rto;
        do_castling<false>(us, from, to, rfrom, rto);
    } else {
        move_piece(to, from);

        if (st->capturedPiece) {
            Square capsq = to;
            if (type_of(m) == EN_PASSANT)
                capsq -= pawn_push(us);
            put_piece(st->capturedPiece, capsq);
        }
    }

    st = st->previous;
    --gamePly;

#ifdef FORGE_NEXT_PROBE
    ForgeNext::mark_move(*this, m);
#endif

    assert(pos_is_ok());
}

void Position::do_null_move(StateInfo& newSt) {
    assert(!checkers());
    assert(&newSt != st);

    newSt          = *st;
    newSt.previous = st;
    st             = &newSt;

    if (st->epSquare != SQ_NONE) {
        st->key ^= Zobrist::EN_PASSANT[file_of(st->epSquare)];
        st->epSquare = SQ_NONE;
    }

    st->key ^= Zobrist::SIDE;
    ++st->rule50;
    st->pliesFromNull = 0;

    sideToMove = ~sideToMove;
    set_check_info();

    // A null move is irreversible for repetition purposes.
    st->repetition = 0;

    // The null move cleared EP and flipped the side, so the keys now agree.
    st->repetitionKey = st->key;

    assert(pos_is_ok());
}

void Position::undo_null_move() {
    assert(!checkers());
    st         = st->previous;
    sideToMove = ~sideToMove;
}

// ------------------------------------------------ static exchange evaluation

// Is the material outcome of the capture sequence starting with `m` at least
// `threshold`? Answered without playing any moves: the swap list is walked with
// running bounds, and x-rays are added as pieces are removed from `occupied`.
bool Position::see_ge(Move m, int threshold) const {
    assert(is_ok(m));

    const MoveType moveType = type_of(m);

    const Square from = from_sq(m);
    const Square to   = to_sq(m);
    int swap;
    Bitboard occupied;

    if (moveType == NORMAL) {
        // This is the original hot path verbatim. SpecialMoveSEE adds no extra
        // arithmetic or global-option loads to ordinary captures.
        swap = Eval::SIMPLE_PIECE_VALUE[type_of(piece_on(to))] - threshold;
        if (swap < 0)
            return false;
        swap = Eval::SIMPLE_PIECE_VALUE[type_of(piece_on(from))] - swap;
        if (swap <= 0)
            return true;
        occupied = (pieces() ^ from) | to;
    } else {
        if (!Search::SpecialMoveSEE || moveType == CASTLING)
            return VALUE_ZERO >= threshold;

        const PieceType victim = moveType == EN_PASSANT ? PAWN : type_of(piece_on(to));
        const PieceType occupant =
            moveType == PROMOTION ? promotion_type(m) : type_of(piece_on(from));
        int gain = Eval::SIMPLE_PIECE_VALUE[victim];
        if (moveType == PROMOTION)
            gain += Eval::SIMPLE_PIECE_VALUE[occupant]
                  - Eval::SIMPLE_PIECE_VALUE[PAWN];
        swap = gain - threshold;
        if (swap < 0)
            return false;
        swap = Eval::SIMPLE_PIECE_VALUE[occupant] - swap;
        if (swap <= 0)
            return true;
        occupied = pieces() ^ from;
        if (moveType == EN_PASSANT)
            occupied ^= to - pawn_push(sideToMove);
        occupied |= to;
    }
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);
    Bitboard stmAttackers, bb;
    int      res = 1;

    while (true) {
        stm = ~stm;
        attackers &= occupied;

        if (!(stmAttackers = attackers & pieces(stm)))
            break;

        // A piece pinned against its own king cannot join the exchange, as long
        // as the pinner is still on the board.
        if (pinners(~stm) & occupied) {
            stmAttackers &= ~blockers_for_king(stm);
            if (!stmAttackers)
                break;
        }

        res ^= 1;

        // Always recapture with the least valuable attacker, adding any x-ray
        // that opens up behind it.
        if ((bb = stmAttackers & pieces(PAWN))) {
            if ((swap = Eval::SIMPLE_PIECE_VALUE[PAWN] - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(KNIGHT))) {
            if ((swap = Eval::SIMPLE_PIECE_VALUE[KNIGHT] - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
        } else if ((bb = stmAttackers & pieces(BISHOP))) {
            if ((swap = Eval::SIMPLE_PIECE_VALUE[BISHOP] - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN);
        } else if ((bb = stmAttackers & pieces(ROOK))) {
            if ((swap = Eval::SIMPLE_PIECE_VALUE[ROOK] - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN);
        } else if ((bb = stmAttackers & pieces(QUEEN))) {
            if ((swap = Eval::SIMPLE_PIECE_VALUE[QUEEN] - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
            attackers |= (attacks_bb<BISHOP>(to, occupied) & pieces(BISHOP, QUEEN))
                       | (attacks_bb<ROOK>(to, occupied) & pieces(ROOK, QUEEN));
        } else
            // Only the king is left. Capturing into a defended square is not
            // allowed, so if the opponent still has an attacker we lose the tempo.
            return (attackers & ~pieces(stm)) ? bool(res ^ 1) : bool(res);
    }

    return bool(res);
}

// ---------------------------------------------------------------- draws

// Twofold repetition counts as a draw inside the search tree, threefold from
// before the root. `st->repetition` holds the ply distance back to the earlier
// occurrence, so comparing it against `ply` distinguishes the two cases.
bool Position::is_draw(int ply) const {
    // Rules-only dead positions, independent of the neural evaluator and of
    // the disabled endgame adviser/tablebases. Two knights are NOT declared
    // dead: a cooperative mating sequence can exist.
    if (!pieces(PAWN, ROOK, QUEEN)) {
        const Bitboard minors = pieces(KNIGHT, BISHOP);
        if (!more_than_one(minors))
            return true;
        constexpr Bitboard dark = 0xAA55AA55AA55AA55ULL;
        if (!pieces(KNIGHT) && (!(minors & dark) || !(minors & ~dark)))
            return true;
    }

    if (st->rule50 > 99 && (!checkers() || MoveList<LEGAL>(*this).size()))
        return true;

    return st->repetition && st->repetition < ply;
}

bool Position::has_repeated() const {
    const StateInfo* stc = st;
    int              end = std::min(st->rule50, st->pliesFromNull);

    while (end-- >= 4) {
        if (stc->repetition)
            return true;
        stc = stc->previous;
    }
    return false;
}

Move Position::repetition_move(int ply) const {
    const int end = std::min(st->rule50, st->pliesFromNull);
    if (end < 3)
        return MOVE_NONE;

    const Key current = st->repetitionKey;
    const StateInfo* old = st->previous;
    Key other = current ^ old->repetitionKey ^ Zobrist::SIDE;
    for (int distance = 3; distance <= end; distance += 2) {
        old = old->previous;
        other ^= old->repetitionKey ^ old->previous->repetitionKey ^ Zobrist::SIDE;
        old = old->previous;
        if (other != 0)
            continue;
        // A twofold cycle is usable only after the root. Across the root it
        // must create a third occurrence, just like is_draw(ply + 1).
        if (ply <= distance && !old->repetition)
            continue;

        Move move = reversible_move(current ^ old->repetitionKey);
        if (move == MOVE_NONE)
            continue;
        if (!(pieces(sideToMove) & from_sq(move)))
            move = make_move(to_sq(move), from_sq(move));
        if (!empty(to_sq(move)) || type_of(piece_on(from_sq(move))) == PAWN)
            continue;
        if (pseudo_legal(move) && legal(move))
            return move;
    }
    return MOVE_NONE;
}

// ------------------------------------------------------------- debugging

Key Position::compute_key() const {
    Key k = 0;

    for (Bitboard b = pieces(); b;) {
        const Square s = pop_lsb(b);
        k ^= Zobrist::PSQ[piece_on(s)][s];
    }

    if (st->epSquare != SQ_NONE)
        k ^= Zobrist::EN_PASSANT[file_of(st->epSquare)];
    if (sideToMove == BLACK)
        k ^= Zobrist::SIDE;

    return k ^ Zobrist::CASTLING[st->castlingRights];
}

Key Position::compute_repetition_key() const {
    if (st->epSquare == SQ_NONE)
        return st->key;

    const Color us = sideToMove, them = ~us;
    const Square ep = st->epSquare, captured = ep - pawn_push(us);
    Bitboard candidates = pawn_attacks_bb(them, ep) & pieces(us, PAWN);
    // Unlike Position::legal(), this also covers any existing non-slider
    // checker. The captured pawn must disappear from the attacker set.
    while (candidates) {
        const Square from = pop_lsb(candidates);
        const Bitboard occupied = (pieces() ^ from ^ captured) | ep;
        if (!(attackers_to(king_square(us), occupied)
              & (pieces(them) ^ captured)))
            return st->key;
    }
    return st->key ^ Zobrist::EN_PASSANT[file_of(ep)];
}

Key Position::compute_pawn_key() const {
    Key k = 0;

    for (Bitboard b = pieces(PAWN); b;) {
        const Square s = pop_lsb(b);
        k ^= Zobrist::PSQ[piece_on(s)][s];
    }
    return k;
}

Key Position::compute_minor_piece_key() const {
    Key k = 0;
    for (Bitboard b = pieces(KNIGHT, BISHOP); b;) {
        const Square s = pop_lsb(b);
        k ^= Zobrist::PSQ[piece_on(s)][s];
    }
    return k;
}

Key Position::compute_non_pawn_key(Color c) const {
    Key k = 0;
    for (Bitboard b = pieces(c) & ~pieces(PAWN); b;) {
        const Square s = pop_lsb(b);
        k ^= Zobrist::PSQ[piece_on(s)][s];
    }
    return k;
}

bool Position::pos_is_ok() const {
    if (piece_on(king_square(WHITE)) != W_KING || piece_on(king_square(BLACK)) != B_KING)
        return false;

    if (count<KING>(WHITE) != 1 || count<KING>(BLACK) != 1)
        return false;

    // The side that just moved must not be in check.
    if (attackers_to(king_square(~sideToMove)) & pieces(sideToMove))
        return false;

    if ((pieces(PAWN) & (RANK_1BB | RANK_8BB)))
        return false;

    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces())
        return false;

    for (PieceType p1 = PAWN; p1 <= KING; ++p1)
        for (PieceType p2 = PieceType(int(p1) + 1); p2 <= KING; ++p2)
            if (pieces(p1) & pieces(p2))
                return false;

    if (st->key != compute_key() || st->repetitionKey != compute_repetition_key()
        || st->pawnKey != compute_pawn_key()
        || st->minorPieceKey != compute_minor_piece_key()
        || st->nonPawnKey[WHITE] != compute_non_pawn_key(WHITE)
        || st->nonPawnKey[BLACK] != compute_non_pawn_key(BLACK))
        return false;

    return true;
}

std::ostream& operator<<(std::ostream& os, const Position& pos) {
    os << "\n +---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f)
            os << " | " << PIECE_TO_CHAR[pos.piece_on(make_square(f, r))];

        os << " | " << (1 + int(r)) << "\n +---+---+---+---+---+---+---+---+\n";
    }

    os << "   a   b   c   d   e   f   g   h\n";
    os << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << pos.key()
       << std::dec << std::nouppercase << "\n";

    return os;
}

}  // namespace boring
