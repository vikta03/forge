// bitboard.cpp — one-time construction of every geometry table, including the
// magic bitboard attack tables.

#include "bitboard.h"

#include <algorithm>

namespace boring {

uint8_t SQUARE_DISTANCE[SQUARE_NB][SQUARE_NB];

Bitboard BETWEEN_BB[SQUARE_NB][SQUARE_NB];
Bitboard LINE_BB[SQUARE_NB][SQUARE_NB];
Bitboard PSEUDO_ATTACKS[PIECE_TYPE_NB][SQUARE_NB];
Bitboard PAWN_ATTACKS[COLOR_NB][SQUARE_NB];

Magic ROOK_MAGICS[SQUARE_NB];
Magic BISHOP_MAGICS[SQUARE_NB];

namespace {

// Backing store shared by all 64 per-square attack tables. The sizes are the
// well-known totals for fixed-shift fancy magics.
Bitboard ROOK_TABLE[0x19000];    // 102400 entries
Bitboard BISHOP_TABLE[0x1480];   //   5248 entries

// xorshift64* — a fixed seed keeps magic generation bit-for-bit reproducible,
// so the engine's tables are identical on every run and every machine.
class PRNG {
    uint64_t s;

    uint64_t rand64() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    }

   public:
    explicit PRNG(uint64_t seed) : s(seed) { assert(seed); }

    uint64_t rand() { return rand64(); }

    // Magic multipliers need few set bits; ANDing three draws biases towards
    // roughly 8 bits set, which is where usable magics live.
    uint64_t sparse_rand() { return rand64() & rand64() & rand64(); }
};

// True when stepping from s by `step` stays on the board without wrapping
// around an edge. The distance check is what catches the wrap.
bool safe_destination(Square s, int step) {
    const Square to = Square(int(s) + step);
    return is_ok(to) && distance(s, to) <= 2;
}

// Reference implementation of a slider: walk each ray until it leaves the board
// or hits an occupied square (which is included, as it can be captured). Slow,
// but it is only used to build the magic tables.
Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occupied) {
    Bitboard attacks = 0;

    const Direction rook_dirs[4]   = {NORTH, SOUTH, EAST, WEST};
    const Direction bishop_dirs[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};
    const Direction* dirs          = (pt == ROOK) ? rook_dirs : bishop_dirs;

    for (int i = 0; i < 4; ++i) {
        const Direction d = dirs[i];
        Square          s = sq;
        while (safe_destination(s, int(d))) {
            s += d;
            attacks |= s;
            if (occupied & s)
                break;
        }
    }
    return attacks;
}

// Find a magic multiplier per square such that
//     ((occupancy & mask) * magic) >> shift
// is a collision-free index into that square's slice of `table`. Collisions
// that map to the *same* attack set are fine and are accepted (constructive
// collisions), which is why we compare against `reference` rather than
// demanding distinct indices.
void init_magics(PieceType pt, Bitboard table[], Magic magics[]) {
    // Per-rank seeds that are known to find magics quickly on 64-bit.
    static const int seeds[RANK_NB] = {728, 10316, 55013, 32803, 12281, 15100, 16645, 255};

    Bitboard occupancy[4096], reference[4096];
    int      epoch[4096] = {};
    int      cnt = 0, size = 0;

    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        // Board edges cannot block anything a slider on s could reach beyond,
        // so they are excluded from the mask — except the edge s itself is on.
        const Bitboard edges =
            ((RANK_1BB | RANK_8BB) & ~rank_bb(s)) | ((FILE_ABB | FILE_HBB) & ~file_bb(s));

        Magic& m  = magics[s];
        m.mask    = sliding_attack(pt, s, 0) & ~edges;
        m.shift   = 64 - popcount(m.mask);
        m.attacks = (s == SQ_A1) ? table : magics[int(s) - 1].attacks + size;

        // Carry-rippler: enumerate every subset of the mask exactly once.
        Bitboard b = 0;
        size       = 0;
        do {
            occupancy[size] = b;
            reference[size] = sliding_attack(pt, s, b);
            ++size;
            b = (b - m.mask) & m.mask;
        } while (b);

        PRNG rng(uint64_t(seeds[rank_of(s)]));

        for (int i = 0; i < size;) {
            // Require the magic to spread the top byte, a cheap prefilter that
            // rejects hopeless candidates before the expensive verification.
            for (m.magic = 0; popcount((m.magic * m.mask) >> 56) < 6;)
                m.magic = rng.sparse_rand();

            // `epoch` marks which slots this candidate has written, so we can
            // reuse the table without clearing it between attempts.
            for (++cnt, i = 0; i < size; ++i) {
                const unsigned idx = m.index(occupancy[i]);

                if (epoch[idx] < cnt) {
                    epoch[idx]   = cnt;
                    m.attacks[idx] = reference[i];
                } else if (m.attacks[idx] != reference[i])
                    break;  // destructive collision — try another magic
            }
        }
    }
}

}  // namespace

void Bitboards::init() {
    for (Square s1 = SQ_A1; s1 <= SQ_H8; ++s1)
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            SQUARE_DISTANCE[s1][s2] =
                uint8_t(std::max(distance<File>(s1, s2), distance<Rank>(s1, s2)));

    init_magics(ROOK, ROOK_TABLE, ROOK_MAGICS);
    init_magics(BISHOP, BISHOP_TABLE, BISHOP_MAGICS);

    for (Square s = SQ_A1; s <= SQ_H8; ++s) {
        PAWN_ATTACKS[WHITE][s] = pawn_attacks_bb<WHITE>(square_bb(s));
        PAWN_ATTACKS[BLACK][s] = pawn_attacks_bb<BLACK>(square_bb(s));

        for (int step : {-17, -15, -10, -6, 6, 10, 15, 17})
            if (safe_destination(s, step))
                PSEUDO_ATTACKS[KNIGHT][s] |= Square(int(s) + step);

        for (int step : {-9, -8, -7, -1, 1, 7, 8, 9})
            if (safe_destination(s, step))
                PSEUDO_ATTACKS[KING][s] |= Square(int(s) + step);

        PSEUDO_ATTACKS[BISHOP][s] = attacks_bb<BISHOP>(s, 0);
        PSEUDO_ATTACKS[ROOK][s]   = attacks_bb<ROOK>(s, 0);
        PSEUDO_ATTACKS[QUEEN][s]  = PSEUDO_ATTACKS[BISHOP][s] | PSEUDO_ATTACKS[ROOK][s];

        for (PieceType pt : {BISHOP, ROOK})
            for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2) {
                if (PSEUDO_ATTACKS[pt][s] & s2) {
                    LINE_BB[s][s2] = (attacks_bb(pt, s, 0) & attacks_bb(pt, s2, 0)) | s | s2;
                    BETWEEN_BB[s][s2] =
                        attacks_bb(pt, s, square_bb(s2)) & attacks_bb(pt, s2, square_bb(s));
                }
            }

        // BETWEEN_BB always includes the destination. For an aligned pair that
        // makes it "block the ray or capture the piece on s2", which is exactly
        // the target mask a check evasion needs; for an unaligned pair it
        // degrades to just s2, which is still the correct answer there.
        for (Square s2 = SQ_A1; s2 <= SQ_H8; ++s2)
            BETWEEN_BB[s][s2] |= s2;
    }
}

std::string Bitboards::pretty(Bitboard b) {
    std::string s = "+---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_8; r >= RANK_1; --r) {
        for (File f = FILE_A; f <= FILE_H; ++f)
            s += (b & make_square(f, r)) ? "| X " : "|   ";

        s += "| " + std::to_string(1 + int(r)) + "\n+---+---+---+---+---+---+---+---+\n";
    }
    s += "  a   b   c   d   e   f   g   h\n";

    return s;
}

}  // namespace boring
