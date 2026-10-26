// pawns.cpp — pawn structure and king shelter.

#include "pawns.h"

#include <algorithm>
#include <cstring>

#include "bitboard.h"
#include "eval_terms.h"

namespace boring {
namespace Pawns {

namespace {

Entry CACHE[CACHE_SIZE];

using namespace Eval;

template<Color Us>
Score evaluate(const Position& pos, Entry* e) {
    constexpr Color     Them = ~Us;
    constexpr Direction Up   = pawn_push(Us);

    const Bitboard ourPawns   = pos.pieces(Us, PAWN);
    const Bitboard theirPawns = pos.pieces(Them, PAWN);

    Score score;

    constexpr Bitboard QueensideBB = FILE_ABB | FILE_BBB | FILE_CBB | FILE_DBB;
    constexpr Bitboard KingsideBB  = FILE_EBB | FILE_FBB | FILE_GBB | FILE_HBB;

    e->passedPawns[Us]       = 0;
    e->connectedPassers[Us]  = 0;
    e->candidatePassers[Us]  = 0;
    e->bothFlanks[Us]        = bool(ourPawns & QueensideBB) && bool(ourPawns & KingsideBB);
    e->kingSquares[Us]       = SQ_NONE;
    e->pawnAttacks[Us]       = pawn_attacks_bb<Us>(ourPawns);
    e->pawnDoubleAttacks[Us] = pawn_double_attacks_bb<Us>(ourPawns);
    e->pawnAttacksSpan[Us]   = 0;

    Bitboard b = ourPawns;
    while (b) {
        const Square s = pop_lsb(b);
        assert(pos.piece_on(s) == make_piece(Us, PAWN));

        const File f = file_of(s);
        const Rank r = relative_rank(Us, s);

        e->pawnAttacksSpan[Us] |= pawn_attack_span(Us, s);

        // The neighbours that decide whether this pawn is healthy.
        const Bitboard neighbours = ourPawns & adjacent_files_bb(f);
        const Bitboard support    = neighbours & rank_bb(s - Up);
        const Bitboard phalanx    = neighbours & rank_bb(s);
        const Bitboard opposed    = theirPawns & forward_file_bb(Us, s);
        const Bitboard stoppers   = theirPawns & passed_pawn_span(Us, s);
        const Bitboard lever      = theirPawns & pawn_attacks_bb(Us, s);
        const Bitboard leverPush  = theirPawns & pawn_attacks_bb(Us, s + Up);
        const Bitboard blocked    = theirPawns & (s + Up);

        // Backward: no friendly pawn beside or behind it can ever support the
        // advance, and the square in front is contested by an enemy pawn.
        const bool backward = !(neighbours & forward_ranks_bb(Them, s + Up)) && (leverPush || blocked);

        e->blockedCount += bool(blocked);

        // Passed when nothing can stop it: no enemy pawn on its own file or on
        // the adjacent files ahead of it.
        //
        // Two near-misses also count. If the only stopper is a pawn that is
        // attacking us right now, we can simply take it. If the only stoppers
        // are pawns guarding the square ahead, and our phalanx outnumbers them,
        // we win the push. Neither case can leave an enemy pawn on our own
        // file, which is what makes the assertion in Evaluation::passed() hold.
        const bool passed = !stoppers
                         || stoppers == lever
                         || (stoppers == leverPush && popcount(phalanx) >= popcount(leverPush));

        // The rear pawn of a doubled pair is not passed however clear the file
        // ahead looks: its own colleague is in the way.
        if (passed && !(ourPawns & forward_file_bb(Us, s)))
            e->passedPawns[Us] |= s;
        // Nothing of ours on the file ahead, but an enemy pawn on an adjacent
        // file still covers the path: one exchange away from being passed.
        else if (!passed && !opposed && stoppers)
            ++e->candidatePassers[Us];

        if (support || phalanx) {
            // Connected pawns. A phalanx is worth more than mere support, and
            // both matter far more the further up the board they are.
            const int v = CONNECTED[r].mg * (2 + bool(phalanx) - bool(opposed))
                        + 18 * popcount(support);

            score += S(v, v * (int(r) - 2) / 4);
        } else if (!neighbours) {
            score += ISOLATED;
            if (!opposed)
                score += WEAK_UNOPPOSED;
        } else if (backward) {
            score += BACKWARD;
            if (!opposed)
                score += WEAK_UNOPPOSED;
        }

        // Doubled, and not defended from behind by a neighbour — the genuinely
        // crippled kind, as opposed to a doubled pawn that still has support.
        if (!support && (ourPawns & forward_file_bb(Them, s)))
            score += DOUBLED;

        if (blocked && r >= RANK_5)
            score += BLOCKED_PAWN[r];
    }

    // Two passers on neighbouring files support each other's advance; the same
    // two scattered across the board do not. passed() scores each passer alone,
    // so this distinction reaches nothing today.
    for (Bitboard p = e->passedPawns[Us]; p;) {
        const Square s = pop_lsb(p);
        if (e->passedPawns[Us] & adjacent_files_bb(s))
            ++e->connectedPassers[Us];
    }

    // Pawn islands: each contiguous group of occupied files beyond the first is
    // another set of weaknesses to defend.
    if (ourPawns) {
        int      islands = 0;
        Bitboard files   = 0;
        for (File f = FILE_A; f <= FILE_H; ++f)
            if (ourPawns & file_bb(f))
                files |= file_bb(f);

        bool inIsland = false;
        for (File f = FILE_A; f <= FILE_H; ++f) {
            const bool occupied = bool(files & file_bb(f));
            if (occupied && !inIsland)
                ++islands;
            inIsland = occupied;
        }

        score += PAWN_ISLAND * (islands - 1);
    }

    return score;
}

}  // namespace

// Score the pawn cover in front of a king standing on `ksq`, plus the enemy
// pawns rushing at it. Evaluated over the king's file and its two neighbours,
// which is where a mating attack actually comes from.
template<Color Us>
Score Entry::evaluate_shelter(const Position& pos, Square ksq) const {
    constexpr Color Them = ~Us;

    const Bitboard ourPawns =
        pos.pieces(Us, PAWN) & ~forward_ranks_bb(Them, ksq);  // pawns not behind the king
    const Bitboard theirPawns = pos.pieces(Them, PAWN);

    Score score = S(5, 5);

    // Keep the window on the board: a king on a1 looks at files a, b, c.
    const File center = File(std::clamp(int(file_of(ksq)), int(FILE_B), int(FILE_G)));

    for (File f = File(int(center) - 1); f <= File(int(center) + 1); ++f) {
        const Bitboard ourFilePawns = ourPawns & file_bb(f);
        const Rank ourRank = ourFilePawns ? relative_rank(Us, frontmost_sq(Them, ourFilePawns))
                                          : RANK_1;

        const Bitboard theirFilePawns = theirPawns & file_bb(f);
        const Rank theirRank = theirFilePawns
                                 ? relative_rank(Us, frontmost_sq(Them, theirFilePawns))
                                 : RANK_1;

        // Distance from the edge picks which of the four column profiles to use.
        const int d = edge_distance(f);

        score += SHELTER_STRENGTH[d][ourRank];

        // A storm pawn stopped dead by one of ours is far less frightening.
        if (ourRank != RANK_1 && ourRank == theirRank - 1)
            score += STORM_BLOCKED[theirRank];
        else
            score += STORM_UNBLOCKED[d][theirRank];
    }

    return score;
}

// Take the best shelter the king can still reach: if castling is available, the
// king is not really standing where he stands.
template<Color Us>
Score Entry::do_king_safety(const Position& pos) {
    const Square ksq = pos.king_square(Us);

    kingSquares[Us]    = ksq;
    castlingRights[Us] = pos.castling_rights(Us);

    Score shelter = evaluate_shelter<Us>(pos, ksq);

    if (pos.can_castle(Us & KING_SIDE)) {
        const Score after = evaluate_shelter<Us>(pos, relative_square(Us, SQ_G1));
        if (after.mg > shelter.mg)
            shelter = after;
    }
    if (pos.can_castle(Us & QUEEN_SIDE)) {
        const Score after = evaluate_shelter<Us>(pos, relative_square(Us, SQ_C1));
        if (after.mg > shelter.mg)
            shelter = after;
    }

    // The endgame half is a pure king-to-own-pawns distance: shelter stops
    // mattering once there is nothing left to attack with, but staying near
    // your pawns always does.
    int      minDistance = 6;
    Bitboard pawns       = pos.pieces(Us, PAWN);

    while (pawns)
        minDistance = std::min(minDistance, distance(ksq, pop_lsb(pawns)));

    return Score(shelter.mg, -16 * minDistance);
}

Entry* probe(const Position& pos) {
    const Key key   = pos.pawn_key();
    Entry*    e     = &CACHE[key & (CACHE_SIZE - 1)];

    if (e->key == key)
        return e;

    e->key          = key;
    e->blockedCount = 0;
    e->scores[WHITE] = evaluate<WHITE>(pos, e);
    e->scores[BLACK] = evaluate<BLACK>(pos, e);

    // Force the shelter cache to miss, so it is recomputed for this structure.
    e->kingSquares[WHITE] = e->kingSquares[BLACK] = SQ_NONE;

    return e;
}

void clear() {
    for (Entry& e : CACHE)
        e = Entry{};
}

// king_safety() lives in the header and is instantiated wherever it is used, so
// it is the callee that has to be instantiated here.
template Score Entry::do_king_safety<WHITE>(const Position&);
template Score Entry::do_king_safety<BLACK>(const Position&);

}  // namespace Pawns
}  // namespace boring
