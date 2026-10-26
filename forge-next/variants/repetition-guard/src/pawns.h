// pawns.h — pawn structure evaluation, cached.
//
// Pawn structure changes rarely: most moves in a search leave it untouched, so
// the same handful of structures are evaluated over and over. Caching them by
// pawn key turns the most expensive structural terms into a table lookup.
//
// King shelter is cached here too, even though it is not purely a pawn term,
// because it depends only on the pawns plus the king square and castling
// rights — all cheap to re-check.

#ifndef BORING_PAWNS_H
#define BORING_PAWNS_H

#include "position.h"
#include "types.h"

namespace boring {
namespace Pawns {

struct Entry {
    Score    pawn_score(Color c) const { return scores[c]; }
    Bitboard pawn_attacks(Color c) const { return pawnAttacks[c]; }
    Bitboard passed_pawns(Color c) const { return passedPawns[c]; }
    Bitboard pawn_attacks_span(Color c) const { return pawnAttacksSpan[c]; }
    Bitboard double_attacks(Color c) const { return pawnDoubleAttacks[c]; }
    int      blocked_count() const { return blockedCount; }
    int      connected_passers(Color c) const { return connectedPassers[c]; }
    int      candidate_passers(Color c) const { return candidatePassers[c]; }
    int      both_flanks(Color c) const { return bothFlanks[c]; }
    int      passed_count() const {
        return popcount(passedPawns[WHITE] | passedPawns[BLACK]);
    }

    // The shelter score for `c`'s king, recomputed only when the king has moved
    // or lost a castling right since this entry was filled.
    template<Color Us>
    Score king_safety(const Position& pos) {
        return (kingSquares[Us] == pos.king_square(Us)
                && castlingRights[Us] == pos.castling_rights(Us))
                 ? kingSafety[Us]
                 : (kingSafety[Us] = do_king_safety<Us>(pos));
    }

    Key      key;
    Score    scores[COLOR_NB];
    Bitboard passedPawns[COLOR_NB];
    Bitboard pawnAttacks[COLOR_NB];
    Bitboard pawnDoubleAttacks[COLOR_NB];
    Bitboard pawnAttacksSpan[COLOR_NB];
    Square   kingSquares[COLOR_NB];
    Score    kingSafety[COLOR_NB];
    int      castlingRights[COLOR_NB];
    int      blockedCount;
    int      connectedPassers[COLOR_NB];
    int      candidatePassers[COLOR_NB];
    int      bothFlanks[COLOR_NB];

   private:
    template<Color Us>
    Score do_king_safety(const Position& pos);
    template<Color Us>
    Score evaluate_shelter(const Position& pos, Square ksq) const;
};

// Direct-mapped cache. A collision simply recomputes, so no verification beyond
// the key comparison is needed. Sized to stay comfortably inside L2.
constexpr size_t CACHE_SIZE = 1 << 14;

Entry* probe(const Position& pos);
void   clear();

}  // namespace Pawns
}  // namespace boring

#endif  // BORING_PAWNS_H
