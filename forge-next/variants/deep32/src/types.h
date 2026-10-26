// Boring — a classic hand-crafted-evaluation chess engine.
//
// types.h — the vocabulary the whole engine is written in: colours, pieces,
// squares, moves, and the packed middlegame/endgame score pair.

#ifndef BORING_TYPES_H
#define BORING_TYPES_H

#include <cassert>
#include <cstdint>

namespace boring {

using Bitboard = uint64_t;
using Key      = uint64_t;

// A position can legally have at most 218 moves; 256 keeps lists cache-friendly
// and leaves room for the generator to write without bounds checks.
constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

// ------------------------------------------------------------------ colours

enum Color : int { WHITE, BLACK, COLOR_NB = 2 };

constexpr Color operator~(Color c) { return Color(int(c) ^ 1); }

// ------------------------------------------------------------------- pieces
//
// Pieces are laid out so that colour is bit 3 and type is bits 0-2. That makes
// make_piece/type_of/color_of pure bit twiddling, and lets a single Piece index
// PSQT and Zobrist tables directly.

enum PieceType : int {
    NO_PIECE_TYPE,
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    ALL_PIECES    = 0,
    PIECE_TYPE_NB = 8
};

enum Piece : int {
    NO_PIECE,
    W_PAWN = PAWN,
    W_KNIGHT,
    W_BISHOP,
    W_ROOK,
    W_QUEEN,
    W_KING,
    B_PAWN = PAWN + 8,
    B_KNIGHT,
    B_BISHOP,
    B_ROOK,
    B_QUEEN,
    B_KING,
    PIECE_NB = 16
};

constexpr Piece     make_piece(Color c, PieceType pt) { return Piece((int(c) << 3) + int(pt)); }
constexpr PieceType type_of(Piece pc)                 { return PieceType(int(pc) & 7); }

// Only meaningful for an occupied square; NO_PIECE would report WHITE.
constexpr Color color_of(Piece pc) {
    assert(pc != NO_PIECE);
    return Color(int(pc) >> 3);
}

// ------------------------------------------------------------------ squares
//
// A1 = 0 and H8 = 63, so rank is s >> 3 and file is s & 7. Rank 1 is White's
// back rank, which makes the board print upside down but keeps every shift in
// the engine pointing the intuitive way (NORTH = +8 = towards Black).

enum Square : int {
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 64
};

enum File : int { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_NB };
enum Rank : int { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_NB };

enum Direction : int {
    NORTH = 8,
    EAST  = 1,
    SOUTH = -NORTH,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

constexpr Square make_square(File f, Rank r) { return Square((int(r) << 3) + int(f)); }
constexpr File   file_of(Square s)           { return File(int(s) & 7); }
constexpr Rank   rank_of(Square s)           { return Rank(int(s) >> 3); }
constexpr bool   is_ok(Square s)             { return s >= SQ_A1 && s <= SQ_H8; }

// Mirror vertically (a1 <-> a8). This is how a White table is read for Black.
constexpr Square flip_rank(Square s) { return Square(int(s) ^ SQ_A8); }
// Mirror horizontally (a1 <-> h1).
constexpr Square flip_file(Square s) { return Square(int(s) ^ SQ_H1); }

// A square/rank as seen from `c`: relative_rank(BLACK, RANK_7) == RANK_2.
constexpr Square relative_square(Color c, Square s) { return Square(int(s) ^ (int(c) * 56)); }
constexpr Rank   relative_rank(Color c, Rank r)     { return Rank(int(r) ^ (int(c) * 7)); }
constexpr Rank   relative_rank(Color c, Square s)   { return relative_rank(c, rank_of(s)); }

constexpr Direction pawn_push(Color c) { return c == WHITE ? NORTH : SOUTH; }

// Enum arithmetic. Squares and ranks/files get incremented in loops constantly,
// so give them the operators rather than casting at every use site.
#define ENABLE_INCR_OPERATORS_ON(T)                                            \
    inline T& operator++(T& d) { return d = T(int(d) + 1); }                   \
    inline T& operator--(T& d) { return d = T(int(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

#undef ENABLE_INCR_OPERATORS_ON

constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square&   operator+=(Square& s, Direction d) { return s = s + d; }
inline Square&   operator-=(Square& s, Direction d) { return s = s - d; }

constexpr Direction operator+(Direction a, Direction b) { return Direction(int(a) + int(b)); }
constexpr Direction operator*(int i, Direction d)       { return Direction(i * int(d)); }
constexpr Direction operator-(Direction d)              { return Direction(-int(d)); }

// --------------------------------------------------------- castling rights

enum CastlingRights : int {
    NO_CASTLING,
    WHITE_OO  = 1,
    WHITE_OOO = 2,
    BLACK_OO  = 4,
    BLACK_OOO = 8,

    KING_SIDE      = WHITE_OO | BLACK_OO,
    QUEEN_SIDE     = WHITE_OOO | BLACK_OOO,
    WHITE_CASTLING = WHITE_OO | WHITE_OOO,
    BLACK_CASTLING = BLACK_OO | BLACK_OOO,
    ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

    CASTLING_RIGHTS_NB = 16
};

constexpr CastlingRights operator&(Color c, CastlingRights cr) {
    return CastlingRights((c == WHITE ? WHITE_CASTLING : BLACK_CASTLING) & cr);
}

// ------------------------------------------------------------------- values
//
// Scores are centipawns. Mate scores count down from VALUE_MATE so that a
// shorter mate always outranks a longer one; anything at or beyond
// VALUE_MATE_IN_MAX_PLY is recognised as a mate score by the search and the TT.

using Value = int;

constexpr Value VALUE_ZERO     = 0;
constexpr Value VALUE_DRAW     = 0;
constexpr Value VALUE_NONE     = 32002;
constexpr Value VALUE_INFINITE = 32001;
constexpr Value VALUE_MATE     = 32000;

constexpr Value VALUE_MATE_IN_MAX_PLY  = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

constexpr Value mate_in(int ply)  { return VALUE_MATE - ply; }
constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

constexpr bool is_win(Value v)  { return v >= VALUE_MATE_IN_MAX_PLY && v <= VALUE_MATE; }
constexpr bool is_loss(Value v) { return v <= VALUE_MATED_IN_MAX_PLY && v >= -VALUE_MATE; }
constexpr bool is_decisive(Value v) { return is_win(v) || is_loss(v); }

// ------------------------------------------------------------ tapered score
//
// Every evaluation term is a (middlegame, endgame) pair. A plain struct rather
// than the usual two-halves-of-an-int32 packing: the compiler keeps both halves
// in registers anyway, and this cannot silently break on the sign of the low
// half the way the packed form does.

struct Score {
    int mg = 0;
    int eg = 0;

    constexpr Score() = default;
    constexpr Score(int m, int e) : mg(m), eg(e) {}

    constexpr Score  operator+(Score o) const { return {mg + o.mg, eg + o.eg}; }
    constexpr Score  operator-(Score o) const { return {mg - o.mg, eg - o.eg}; }
    constexpr Score  operator-() const        { return {-mg, -eg}; }
    constexpr Score  operator*(int i) const   { return {mg * i, eg * i}; }

    constexpr Score& operator+=(Score o) { mg += o.mg; eg += o.eg; return *this; }
    constexpr Score& operator-=(Score o) { mg -= o.mg; eg -= o.eg; return *this; }
    constexpr Score& operator*=(int i)   { mg *= i;    eg *= i;    return *this; }

    constexpr bool operator==(Score o) const { return mg == o.mg && eg == o.eg; }
    constexpr bool operator!=(Score o) const { return !(*this == o); }
};

constexpr Score operator*(int i, Score s) { return s * i; }

// Shorthand used throughout eval_terms.h, where readability of the table
// matters more than anything else.
constexpr Score S(int mg, int eg) { return Score(mg, eg); }

// -------------------------------------------------------------------- moves
//
//   bits  0- 5   destination square
//   bits  6-11   origin square
//   bits 12-13   promotion piece type - KNIGHT  (so 0=N, 1=B, 2=R, 3=Q)
//   bits 14-15   special move flag: normal, promotion, en passant, castling
//
// Whether a move is a capture is NOT stored. It is derived from the board at
// use time, which makes it impossible for the flag to disagree with reality.
//
// MOVE_NONE and MOVE_NULL both decode to "normal", and are distinguishable
// because no real move has from == to.

enum MoveType : int {
    NORMAL     = 0,
    PROMOTION  = 1 << 14,
    EN_PASSANT = 2 << 14,
    CASTLING   = 3 << 14
};

enum Move : int {
    MOVE_NONE = 0,
    MOVE_NULL = 65  // b1b1: a from==to move that is not all-zero
};

constexpr Square   from_sq(Move m)  { return Square((int(m) >> 6) & 0x3F); }
constexpr Square   to_sq(Move m)    { return Square(int(m) & 0x3F); }
constexpr MoveType type_of(Move m)  { return MoveType(int(m) & (3 << 14)); }

// Only meaningful when type_of(m) == PROMOTION.
constexpr PieceType promotion_type(Move m) { return PieceType(((int(m) >> 12) & 3) + KNIGHT); }

// from and to together; used as a compact index into history tables.
constexpr int from_to(Move m) { return int(m) & 0xFFF; }

constexpr bool is_ok(Move m) { return from_sq(m) != to_sq(m); }

constexpr Move make_move(Square from, Square to) {
    return Move((int(from) << 6) + int(to));
}

template<MoveType T>
constexpr Move make_move(Square from, Square to, PieceType pt = KNIGHT) {
    return Move(T + ((int(pt) - KNIGHT) << 12) + (int(from) << 6) + int(to));
}

}  // namespace boring

#endif  // BORING_TYPES_H
