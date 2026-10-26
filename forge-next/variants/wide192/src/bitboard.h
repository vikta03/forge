// bitboard.h — 64-bit board sets, the precomputed geometry tables, and magic
// bitboard sliding attacks.
//
// Everything here is pure geometry: it depends only on square indices, never on
// a position. All tables are filled once by Bitboards::init().

#ifndef BORING_BITBOARD_H
#define BORING_BITBOARD_H

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string>

#include "types.h"

namespace boring {

namespace Bitboards {
void        init();
std::string pretty(Bitboard b);
}  // namespace Bitboards

constexpr Bitboard ALL_SQUARES  = ~Bitboard(0);
constexpr Bitboard DARK_SQUARES = 0xAA55AA55AA55AA55ULL;

constexpr Bitboard FILE_ABB = 0x0101010101010101ULL;
constexpr Bitboard FILE_BBB = FILE_ABB << 1;
constexpr Bitboard FILE_CBB = FILE_ABB << 2;
constexpr Bitboard FILE_DBB = FILE_ABB << 3;
constexpr Bitboard FILE_EBB = FILE_ABB << 4;
constexpr Bitboard FILE_FBB = FILE_ABB << 5;
constexpr Bitboard FILE_GBB = FILE_ABB << 6;
constexpr Bitboard FILE_HBB = FILE_ABB << 7;

constexpr Bitboard RANK_1BB = 0xFFULL;
constexpr Bitboard RANK_2BB = RANK_1BB << (8 * 1);
constexpr Bitboard RANK_3BB = RANK_1BB << (8 * 2);
constexpr Bitboard RANK_4BB = RANK_1BB << (8 * 3);
constexpr Bitboard RANK_5BB = RANK_1BB << (8 * 4);
constexpr Bitboard RANK_6BB = RANK_1BB << (8 * 5);
constexpr Bitboard RANK_7BB = RANK_1BB << (8 * 6);
constexpr Bitboard RANK_8BB = RANK_1BB << (8 * 7);

constexpr Bitboard QUEEN_SIDE_BB   = FILE_ABB | FILE_BBB | FILE_CBB | FILE_DBB;
constexpr Bitboard KING_SIDE_BB    = FILE_EBB | FILE_FBB | FILE_GBB | FILE_HBB;
constexpr Bitboard CENTER_FILES_BB = FILE_CBB | FILE_DBB | FILE_EBB | FILE_FBB;
constexpr Bitboard CENTER_BB       = (FILE_DBB | FILE_EBB) & (RANK_4BB | RANK_5BB);

// Chebyshev distance between squares, i.e. king moves.
extern uint8_t SQUARE_DISTANCE[SQUARE_NB][SQUARE_NB];

extern Bitboard BETWEEN_BB[SQUARE_NB][SQUARE_NB];   // strictly between, plus `to`
extern Bitboard LINE_BB[SQUARE_NB][SQUARE_NB];      // full line through both, if aligned
extern Bitboard PSEUDO_ATTACKS[PIECE_TYPE_NB][SQUARE_NB];
extern Bitboard PAWN_ATTACKS[COLOR_NB][SQUARE_NB];

// ---------------------------------------------------------------- accessors

constexpr Bitboard square_bb(Square s) {
    assert(is_ok(s));
    return Bitboard(1) << int(s);
}

constexpr Bitboard operator&(Bitboard b, Square s) { return b & square_bb(s); }
constexpr Bitboard operator|(Bitboard b, Square s) { return b | square_bb(s); }
constexpr Bitboard operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
constexpr Bitboard& operator&=(Bitboard& b, Square s) { return b &= square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | square_bb(s2); }

constexpr bool more_than_one(Bitboard b) { return b & (b - 1); }

constexpr Bitboard file_bb(File f)   { return FILE_ABB << int(f); }
constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }
constexpr Bitboard rank_bb(Rank r)   { return RANK_1BB << (8 * int(r)); }
constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

// The two files flanking f (one file at the board edge).
constexpr Bitboard adjacent_files_bb(File f) {
    return (f > FILE_A ? file_bb(File(int(f) - 1)) : 0)
         | (f < FILE_H ? file_bb(File(int(f) + 1)) : 0);
}
constexpr Bitboard adjacent_files_bb(Square s) { return adjacent_files_bb(file_of(s)); }

// Every rank strictly in front of r, from c's point of view.
constexpr Bitboard forward_ranks_bb(Color c, Rank r) {
    return c == WHITE ? ~RANK_1BB << (8 * int(r)) : ~RANK_8BB >> (8 * (7 - int(r)));
}
constexpr Bitboard forward_ranks_bb(Color c, Square s) { return forward_ranks_bb(c, rank_of(s)); }

// The squares in front of s on its own file.
constexpr Bitboard forward_file_bb(Color c, Square s) {
    return forward_ranks_bb(c, s) & file_bb(s);
}

// Squares a pawn on s could ever attack as it advances.
constexpr Bitboard pawn_attack_span(Color c, Square s) {
    return forward_ranks_bb(c, s) & adjacent_files_bb(s);
}

// A pawn on s is passed iff no enemy pawn stands in this mask.
constexpr Bitboard passed_pawn_span(Color c, Square s) {
    return forward_ranks_bb(c, s) & (adjacent_files_bb(s) | file_bb(s));
}

// True when the three squares are on a common rank, file or diagonal.
inline bool aligned(Square s1, Square s2, Square s3) { return LINE_BB[s1][s2] & s3; }

template<typename T = Square>
inline int distance(Square x, Square y);
template<>
inline int distance<File>(Square x, Square y) { return std::abs(int(file_of(x)) - int(file_of(y))); }
template<>
inline int distance<Rank>(Square x, Square y) { return std::abs(int(rank_of(x)) - int(rank_of(y))); }
template<>
inline int distance<Square>(Square x, Square y) { return SQUARE_DISTANCE[x][y]; }

inline int edge_distance(File f) { return std::min(int(f), int(FILE_H) - int(f)); }

// ------------------------------------------------------------------- shifts

// Shift a whole set one step in direction D, dropping anything that wraps.
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    return D == NORTH      ? b << 8
         : D == SOUTH      ? b >> 8
         : D == NORTH + NORTH ? b << 16
         : D == SOUTH + SOUTH ? b >> 16
         : D == EAST       ? (b & ~FILE_HBB) << 1
         : D == WEST       ? (b & ~FILE_ABB) >> 1
         : D == NORTH_EAST ? (b & ~FILE_HBB) << 9
         : D == NORTH_WEST ? (b & ~FILE_ABB) << 7
         : D == SOUTH_EAST ? (b & ~FILE_HBB) >> 7
         : D == SOUTH_WEST ? (b & ~FILE_ABB) >> 9
                           : 0;
}

// All squares attacked by the pawns in `b`.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Bitboard b) {
    return C == WHITE ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b)
                      : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}

inline Bitboard pawn_attacks_bb(Color c, Bitboard b) {
    return c == WHITE ? pawn_attacks_bb<WHITE>(b) : pawn_attacks_bb<BLACK>(b);
}

inline Bitboard pawn_attacks_bb(Color c, Square s) {
    assert(is_ok(s));
    return PAWN_ATTACKS[c][s];
}

// Squares defended by two pawns of colour c.
template<Color C>
constexpr Bitboard pawn_double_attacks_bb(Bitboard b) {
    return C == WHITE ? shift<NORTH_WEST>(b) & shift<NORTH_EAST>(b)
                      : shift<SOUTH_WEST>(b) & shift<SOUTH_EAST>(b);
}

// ------------------------------------------------------------------- magics
//
// Fancy magic bitboards. For each square we keep the relevant-occupancy mask,
// a multiplier, and a pointer into one shared attack table. The multiply-shift
// maps the 2^n relevant occupancies onto a dense index.

struct Magic {
    Bitboard  mask;
    Bitboard  magic;
    Bitboard* attacks;
    unsigned  shift;

    unsigned index(Bitboard occupied) const {
        return unsigned(((occupied & mask) * magic) >> shift);
    }

    Bitboard operator[](Bitboard occupied) const { return attacks[index(occupied)]; }
};

extern Magic ROOK_MAGICS[SQUARE_NB];
extern Magic BISHOP_MAGICS[SQUARE_NB];

template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {
    static_assert(Pt == BISHOP || Pt == ROOK, "attacks_bb<Pt>(s, occ) needs BISHOP or ROOK");
    assert(is_ok(s));
    return Pt == BISHOP ? BISHOP_MAGICS[s][occupied] : ROOK_MAGICS[s][occupied];
}

// Attacks of any piece type from s on a board with the given occupancy.
inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {
    assert(pt != PAWN && is_ok(s));
    switch (pt) {
    case BISHOP:
        return attacks_bb<BISHOP>(s, occupied);
    case ROOK:
        return attacks_bb<ROOK>(s, occupied);
    case QUEEN:
        return attacks_bb<BISHOP>(s, occupied) | attacks_bb<ROOK>(s, occupied);
    default:
        return PSEUDO_ATTACKS[pt][s];
    }
}

// Attacks of a leaper (knight, king) — occupancy is irrelevant.
template<PieceType Pt>
inline Bitboard attacks_bb(Square s) {
    static_assert(Pt == KNIGHT || Pt == KING, "attacks_bb<Pt>(s) needs KNIGHT or KING");
    assert(is_ok(s));
    return PSEUDO_ATTACKS[Pt][s];
}

// -------------------------------------------------------------- bit fiddling

inline int popcount(Bitboard b) { return std::popcount(b); }

inline Square lsb(Bitboard b) {
    assert(b);
    return Square(std::countr_zero(b));
}

inline Square msb(Bitboard b) {
    assert(b);
    return Square(63 ^ std::countl_zero(b));
}

inline Bitboard least_significant_square_bb(Bitboard b) {
    assert(b);
    return b & -b;
}

// Remove and return the least significant set square.
inline Square pop_lsb(Bitboard& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

// The set square nearest to `c`'s own back rank.
inline Square frontmost_sq(Color c, Bitboard b) { return c == WHITE ? msb(b) : lsb(b); }

}  // namespace boring

#endif  // BORING_BITBOARD_H
