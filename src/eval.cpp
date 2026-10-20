// eval.cpp — the hand-crafted evaluation.
//
// Structure: build the attack maps once for both sides, then walk the terms.
// Everything is computed for a colour through a template, so White and Black go
// down literally the same code path with the board mirrored. That is what makes
// the symmetry test — eval(pos) == -eval(colour-flipped pos) — a real check
// rather than a tautology, since any term that reads an absolute rank or file
// instead of a relative one will break it.

#include "eval.h"

#include <algorithm>

#include "bitboard.h"
#include "endgame.h"
#include "eval_terms.h"
#include "forge_factor_net.h"
#include "pawns.h"
#include "position.h"

namespace boring {
namespace Eval {

Score PSQ[PIECE_NB][SQUARE_NB];

// Fold material value into placement, and mirror the tables for White.
//
// The tables in eval_terms.h are written the way a board is printed, rank 8
// first. That is already Black's orientation, so Black reads a square directly
// and White reads its vertical mirror. White's entries are positive and Black's
// negative, because every incremental score in the engine is kept from White's
// point of view.
void init_psqt() {
    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        const Score* table = PSQT_TABLE[pt];

        for (Square s = SQ_A1; s <= SQ_H8; ++s) {
            PSQ[make_piece(WHITE, pt)][s] = PIECE_VALUE[pt] + table[flip_rank(s)];
            PSQ[make_piece(BLACK, pt)][s] = -(PIECE_VALUE[pt] + table[s]);
        }
    }
}

Value tapered(Score score, int phase) {
    phase = std::clamp(phase, 0, PHASE_MIDGAME);
    return Value((score.mg * phase + score.eg * (PHASE_MIDGAME - phase)) / PHASE_MIDGAME);
}

namespace {

// The four files nearest the king, used for the flank-attack term.
constexpr Bitboard KING_FLANK[FILE_NB] = {
    QUEEN_SIDE_BB ^ FILE_DBB, QUEEN_SIDE_BB,   QUEEN_SIDE_BB,              CENTER_FILES_BB,
    CENTER_FILES_BB,          KING_SIDE_BB,    KING_SIDE_BB,               KING_SIDE_BB ^ FILE_EBB
};

// Ranks 2-4 from our point of view, where a space advantage is measured.
constexpr Bitboard SPACE_MASK[COLOR_NB] = {
    CENTER_FILES_BB & (RANK_2BB | RANK_3BB | RANK_4BB),
    CENTER_FILES_BB & (RANK_7BB | RANK_6BB | RANK_5BB)
};

// One evaluation of one position. Holds the attack maps that nearly every term
// needs, so they are built once rather than recomputed per term.
class Evaluation {
   public:
    explicit Evaluation(const Position& p, Trace* t = nullptr) : pos(p), trace(t) {}

    Value value();

   private:
    template<Color Us>
    void initialize();
    template<Color Us, PieceType Pt>
    Score pieces();
    template<Color Us>
    Score king() const;
    template<Color Us>
    Score threats() const;
    template<Color Us>
    Score passed() const;
    template<Color Us>
    int space() const;

    int scale_factor(Value eg) const;

    template<Color Us>
    void extras() const;

    const Position& pos;
    mutable Trace* trace = nullptr;
    Pawns::Entry*   pe = nullptr;

    // Squares a piece of ours may usefully go to: not occupied by our own
    // pieces, not attacked by an enemy pawn, and not one of our blocked pawns
    // or our king.
    Bitboard mobilityArea[COLOR_NB] = {};
    Score    mobility[COLOR_NB]     = {};

    // attackedBy[c][pt] — squares attacked by c's pieces of type pt.
    // attackedBy[c][ALL_PIECES] — the union.
    // attackedBy2[c] — squares attacked at least twice.
    Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB] = {};
    Bitboard attackedBy2[COLOR_NB]               = {};

    // The king's neighbourhood, extended one rank forward and shifted away from
    // the board edge, since an attack on a king in the corner still comes
    // through the squares beside it.
    Bitboard kingRing[COLOR_NB]        = {};
    int      kingAttackersCount[COLOR_NB]  = {};
    int      kingAttackersWeight[COLOR_NB] = {};
    int      kingAttacksCount[COLOR_NB]    = {};
};

template<Color Us>
void Evaluation::initialize() {
    constexpr Color     Them = ~Us;
    constexpr Direction Up   = pawn_push(Us);
    constexpr Direction Down = -Up;
    constexpr Bitboard  LowRanks = (Us == WHITE ? RANK_2BB | RANK_3BB : RANK_7BB | RANK_6BB);

    const Square ksq = pos.king_square(Us);

    // Pawns that cannot move are not going to contribute, and the squares they
    // sit on are not available to our pieces either.
    const Bitboard blockedPawns = pos.pieces(Us, PAWN) & (shift<Down>(pos.pieces()) | LowRanks);

    mobilityArea[Us] = ~(blockedPawns | pos.pieces(Us, KING, QUEEN)
                         | pos.blockers_for_king(Us) | pe->pawn_attacks(Them));

    attackedBy[Us][PAWN]       = pe->pawn_attacks(Us);
    attackedBy[Us][KING]       = attacks_bb<KING>(ksq);
    attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
    attackedBy2[Us]            = pe->double_attacks(Us) | (attackedBy[Us][KING] & attackedBy[Us][PAWN]);

    // Build the ring around a king that is one square from the edge as though
    // he stood one square further in: the attack still arrives the same way.
    Square kr = ksq;
    if (relative_rank(Us, ksq) == RANK_1)
        kr = kr + Up;
    if (file_of(ksq) == FILE_A)
        kr = kr + EAST;
    else if (file_of(ksq) == FILE_H)
        kr = kr + WEST;

    kingRing[Us] = attacks_bb<KING>(kr) | kr;

    // Squares in our ring defended by two of our own pawns are not weak, and an
    // attacker there is not really attacking.
    kingRing[Us] &= ~pe->double_attacks(Us);

    kingAttackersCount[Them]  = popcount(kingRing[Us] & pe->pawn_attacks(Them));
    kingAttackersWeight[Them] = 0;
    kingAttacksCount[Them]    = 0;
}

template<Color Us, PieceType Pt>
Score Evaluation::pieces() {
    static_assert(Pt >= KNIGHT && Pt <= QUEEN, "pawns and kings are handled separately");

    constexpr Color    Them = ~Us;
    constexpr Bitboard OutpostRanks =
        (Us == WHITE ? RANK_4BB | RANK_5BB | RANK_6BB : RANK_5BB | RANK_4BB | RANK_3BB);

    Score score;

    attackedBy[Us][Pt] = 0;

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb) {
        const Square s = pop_lsb(bb);

        // A piece pinned to its own king still defends and still counts as an
        // attacker along the pin ray, so x-ray through our own queens and
        // rooks/bishops when computing what it sees.
        Bitboard b = Pt == BISHOP ? attacks_bb<BISHOP>(s, pos.pieces() ^ pos.pieces(QUEEN))
                   : Pt == ROOK   ? attacks_bb<ROOK>(s, pos.pieces() ^ pos.pieces(QUEEN)
                                                          ^ pos.pieces(Us, ROOK))
                                  : attacks_bb(Pt, s, pos.pieces());

        // A pinned piece may only move along the pin, so it should not be
        // credited with mobility it does not have.
        if (pos.blockers_for_king(Us) & s)
            b &= LINE_BB[pos.king_square(Us)][s];

        if (trace) {
            if constexpr (Pt == ROOK || Pt == QUEEN) {
                if (b & pos.pieces(Us, ROOK, QUEEN)) ++trace->x.batteries[Us];
            } else if constexpr (Pt == BISHOP) {
                if (b & pos.pieces(Us, BISHOP, QUEEN)) ++trace->x.batteries[Us];
            }
        }

        attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & b;
        attackedBy[Us][Pt] |= b;
        attackedBy[Us][ALL_PIECES] |= b;

        // --- king attack accounting ---
        if (b & kingRing[Them]) {
            kingAttackersCount[Us]++;
            kingAttackersWeight[Us] += KING_ATTACK_WEIGHT[Pt];
            kingAttacksCount[Us] += popcount(b & attacks_bb<KING>(pos.king_square(Them)));
        }

        // --- mobility ---
        const int mob = popcount(b & mobilityArea[Us]);

        if constexpr (Pt == KNIGHT)
            mobility[Us] += KNIGHT_MOBILITY[mob];
        else if constexpr (Pt == BISHOP)
            mobility[Us] += BISHOP_MOBILITY[mob];
        else if constexpr (Pt == ROOK)
            mobility[Us] += ROOK_MOBILITY[mob];
        else
            mobility[Us] += QUEEN_MOBILITY[mob];

        if constexpr (Pt == KNIGHT || Pt == BISHOP) {
            // An outpost is a square no enemy pawn can ever attack, on ranks
            // 4-6, defended by one of our own pawns.
            const Bitboard outpostSquares =
                OutpostRanks & ~pe->pawn_attacks_span(Them) & attackedBy[Us][PAWN];

            if (outpostSquares & s)
                score += (Pt == KNIGHT ? OUTPOST_KNIGHT : OUTPOST_BISHOP);
            else if (Pt == KNIGHT && (b & outpostSquares & ~pos.pieces(Us)))
                score += REACHABLE_OUTPOST;

            // Sheltering directly behind one of our own pawns.
            if (shift<pawn_push(Us)>(square_bb(s)) & pos.pieces(Us, PAWN))
                score += MINOR_BEHIND_PAWN;

            // A minor far from our king is not defending it.
            score += (Pt == KNIGHT ? KING_PROTECTOR_KNIGHT : KING_PROTECTOR_BISHOP)
                   * distance(s, pos.king_square(Us));

            if constexpr (Pt == BISHOP) {
                // Bad bishop: our own pawns on its colour block it, and blocked
                // pawns in the centre block it worst of all.
                const Bitboard sameColour = (DARK_SQUARES & s) ? DARK_SQUARES : ~DARK_SQUARES;
                const Bitboard blocked    = pos.pieces(Us, PAWN) & shift<-pawn_push(Us)>(pos.pieces());

                score += BISHOP_PAWNS * popcount(pos.pieces(Us, PAWN) & sameColour)
                       * (1 + popcount(blocked & CENTER_FILES_BB));

                // A bishop raking an unobstructed long diagonal through the
                // centre is worth a tempo or two on its own.
                if (more_than_one(attacks_bb<BISHOP>(s, pos.pieces(PAWN)) & CENTER_BB))
                    score += LONG_DIAGONAL_BISHOP;

                // The a7/h7 trap: a bishop that grabbed a pawn and cannot get
                // back out past the pawn chain.
                if (relative_rank(Us, s) == RANK_7 && (file_of(s) == FILE_A || file_of(s) == FILE_H)) {
                    const Square escape = s + pawn_push(Us) + (file_of(s) == FILE_A ? EAST : WEST);
                    if (is_ok(escape) && (pos.pieces(Them, PAWN) & escape))
                        score += TRAPPED_BISHOP;
                }
            }
        }

        if constexpr (Pt == ROOK) {
            // Sharing a file with the enemy queen, even through pieces.
            if (pos.pieces(Them, QUEEN) & file_bb(s))
                score += ROOK_ON_QUEEN_FILE;

            if (trace && relative_rank(Us, s) >= RANK_7) ++trace->x.rookOnSeventh[Us];

            const bool ourPawnsOnFile   = bool(pos.pieces(Us, PAWN) & file_bb(s));
            const bool theirPawnsOnFile = bool(pos.pieces(Them, PAWN) & file_bb(s));

            if (!ourPawnsOnFile)
                score += theirPawnsOnFile ? ROOK_ON_SEMI_OPEN_FILE : ROOK_ON_OPEN_FILE;
            else if (mob <= 3) {
                // A rook with almost no squares, boxed in by its own king on
                // the wrong side. Worse when the king cannot castle out of it.
                const File kf = file_of(pos.king_square(Us));

                if ((kf < FILE_E) == (file_of(s) < kf))
                    score += TRAPPED_ROOK * (1 + !pos.castling_rights(Us));
            }
        }

        if constexpr (Pt == QUEEN) {
            // A queen that a discovered attack could hit if one enemy piece
            // steps aside.
            Bitboard pinners;
            if (pos.slider_blockers(pos.pieces(Them, ROOK, BISHOP), s, pinners))
                score += WEAK_QUEEN;
        }
    }

    return score;
}

template<Color Us>
Score Evaluation::king() const {
    constexpr Color    Them     = ~Us;
    constexpr Bitboard Camp     = (Us == WHITE ? ALL_SQUARES ^ RANK_6BB ^ RANK_7BB ^ RANK_8BB
                                               : ALL_SQUARES ^ RANK_3BB ^ RANK_2BB ^ RANK_1BB);

    const Square ksq = pos.king_square(Us);

    // Squares around the king that they attack, we do not adequately defend.
    const Bitboard weak = attackedBy[Them][ALL_PIECES] & ~attackedBy2[Us]
                        & (~attackedBy[Us][ALL_PIECES] | attackedBy[Us][KING]
                           | attackedBy[Us][QUEEN]);

    Score score = pe->king_safety<Us>(pos);
    if (trace) { trace->x.shelterMg[Us] = score.mg; trace->x.shelterEg[Us] = score.eg; }

    // Squares from which a check could be given, on a board where our queen is
    // transparent (she is often the piece that would have to interpose).
    const Bitboard b1 = attacks_bb<ROOK>(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));
    const Bitboard b2 = attacks_bb<BISHOP>(ksq, pos.pieces() ^ pos.pieces(Us, QUEEN));

    // A square is safe for the enemy if we do not defend it, or if they attack
    // it twice and we defend it only once.
    const Bitboard safe = ~pos.pieces(Them)
                        & (~attackedBy[Us][ALL_PIECES] | (weak & attackedBy2[Them]));

    int kingDanger = 0;

    // Safe checks are the dominant term, and rightly so: a check the checker
    // survives is the first move of a mating attack.
    const Bitboard rookChecks = b1 & attackedBy[Them][ROOK] & safe;
    if (rookChecks)
        kingDanger += SAFE_CHECK_ROOK;
    else if (b1 & attackedBy[Them][ROOK])
        kingDanger += KING_DANGER_UNSAFE_CHECK;

    const Bitboard queenChecks = (b1 | b2) & attackedBy[Them][QUEEN] & safe
                               & ~attackedBy[Us][QUEEN] & ~rookChecks;
    if (queenChecks)
        kingDanger += SAFE_CHECK_QUEEN;

    const Bitboard bishopChecks =
        b2 & attackedBy[Them][BISHOP] & safe & ~queenChecks;
    if (bishopChecks)
        kingDanger += SAFE_CHECK_BISHOP;
    else if (b2 & attackedBy[Them][BISHOP])
        kingDanger += KING_DANGER_UNSAFE_CHECK;

    const Bitboard knightChecks = attacks_bb<KNIGHT>(ksq) & attackedBy[Them][KNIGHT];
    if (knightChecks & safe)
        kingDanger += SAFE_CHECK_KNIGHT;
    else if (knightChecks)
        kingDanger += KING_DANGER_UNSAFE_CHECK;

    // Enemy activity on our king's flank, counting doubly-attacked squares
    // twice, minus our own defence of the same area.
    const Bitboard flank      = KING_FLANK[file_of(ksq)] & Camp;
    const int      flankAttack = popcount(flank & attackedBy[Them][ALL_PIECES])
                          + popcount(flank & attackedBy2[Them]);
    const int flankDefense = popcount(flank & attackedBy[Us][ALL_PIECES]);

    kingDanger += kingAttackersCount[Them] * kingAttackersWeight[Them] / 8
                + KING_DANGER_ATTACK_SQUARES * kingAttacksCount[Them] / 8
                + KING_DANGER_WEAK_RING * popcount(kingRing[Us] & weak)
                + FLANK_ATTACK_WEIGHT * flankAttack
                - 4 * flankDefense
                - KING_DANGER_KNIGHT_DEFENDER * bool(attackedBy[Us][KNIGHT] & attackedBy[Us][KING])
                + KING_DANGER_BASE;

    // Without a queen there is rarely enough force to convert an attack.
    if (!pos.count<QUEEN>(Them))
        kingDanger += KING_DANGER_QUEENLESS;

    if (trace) {
        trace->x.kingDanger[Us]    = kingDanger;
        trace->x.kingAttackers[Us] = kingAttackersCount[Them];
        trace->x.flankAttack[Us]   = flankAttack;
        trace->x.flankDefense[Us]  = flankDefense;
        trace->x.safeChecks[Us]    = bool(rookChecks) | (bool(queenChecks) << 1)
                                   | (bool(bishopChecks) << 2)
                                   | (bool(knightChecks & safe) << 3);
    }

    // The quadratic is the point: two attackers are worth far more than twice
    // one attacker, which is how attacks behave over the board. Below zero the
    // "attack" is imaginary and is ignored rather than turned into a bonus.
    if (kingDanger > 100)
        score -= S(kingDanger * kingDanger / 4096, kingDanger / 16);

    // A king on a file with no pawn of ours is exposed to a rook lift.
    if (!(pos.pieces(Us, PAWN) & file_bb(ksq)))
        score += (pos.pieces(Them, PAWN) & file_bb(ksq)) ? KING_ON_SEMI_OPEN_FILE
                                                         : KING_ON_OPEN_FILE;

    return score;
}

template<Color Us>
Score Evaluation::threats() const {
    constexpr Color     Them = ~Us;
    constexpr Direction Up   = pawn_push(Us);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? RANK_3BB : RANK_6BB);

    Score score;

    // Enemy pieces (not pawns) that are not defended by one of their pawns.
    const Bitboard nonPawnEnemies = pos.pieces(Them) & ~pos.pieces(Them, PAWN);
    const Bitboard stronglyProtected =
        attackedBy[Them][PAWN] | (attackedBy2[Them] & ~attackedBy2[Us]);
    const Bitboard defended = nonPawnEnemies & stronglyProtected;
    const Bitboard weak     = pos.pieces(Them) & ~stronglyProtected & attackedBy[Us][ALL_PIECES];

    if (defended | weak) {
        Bitboard b = (defended | weak) & (attackedBy[Us][KNIGHT] | attackedBy[Us][BISHOP]);
        while (b)
            score += THREAT_BY_MINOR[type_of(pos.piece_on(pop_lsb(b)))];

        b = weak & attackedBy[Us][ROOK];
        while (b)
            score += THREAT_BY_ROOK[type_of(pos.piece_on(pop_lsb(b)))];

        if (weak & attackedBy[Us][KING])
            score += THREAT_BY_KING;

        // Undefended and attacked: a piece that is simply lost.
        b = ~attackedBy[Them][ALL_PIECES] | (nonPawnEnemies & attackedBy2[Us]);
        score += HANGING * popcount(weak & b);
        if (trace) {
            trace->x.hanging[Us] = popcount(weak & b);
            // A value-weighted read of the same bitboard. Boring runs no SEE
            // anywhere, so a hanging queen and a hanging pawn price the same;
            // this is the cheap correction -- no extra scan, one loop over a
            // set that is almost always empty or a single square.
            Bitboard h = weak & b;
            int mat = 0, mx = 0;
            while (h) {
                const int v = SIMPLE_PIECE_VALUE[type_of(pos.piece_on(pop_lsb(h)))];
                mat += v;
                if (v > mx) mx = v;
            }
            trace->x.hangingMaterial[Us]  = mat;
            trace->x.maxHangingVictim[Us] = mx;
        }

        // A piece whose only defender is the queen is not really defended.
        score += WEAK_QUEEN_PROTECTION * popcount(weak & attackedBy[Them][QUEEN]);
        if (trace) trace->x.weakQueenProt[Us] = popcount(weak & attackedBy[Them][QUEEN]);
    }

    // Squares they attack that they cannot actually use, because we cover them
    // too and they have nothing spare to add.
    const Bitboard restricted = attackedBy[Them][ALL_PIECES] & ~stronglyProtected
                              & attackedBy[Us][ALL_PIECES];
    score += RESTRICTED_PIECE * popcount(restricted);
    if (trace) trace->x.restricted[Us] = popcount(restricted);

    // A piece attacked by a pawn that is itself not en prise. The cheapest
    // possible attacker, and usually decisive.
    Bitboard safePawns = pos.pieces(Us, PAWN)
                       & (~attackedBy[Them][ALL_PIECES] | attackedBy[Us][ALL_PIECES]);
    score += THREAT_BY_SAFE_PAWN
           * popcount(pawn_attacks_bb<Us>(safePawns) & nonPawnEnemies);
    if (trace) trace->x.threatSafePawn[Us] =
        popcount(pawn_attacks_bb<Us>(safePawns) & nonPawnEnemies);

    // Pawn pushes that would create such a threat next move.
    Bitboard push = shift<Up>(pos.pieces(Us, PAWN)) & ~pos.pieces();
    push |= shift<Up>(push & TRank3BB) & ~pos.pieces();
    push &= ~attackedBy[Them][PAWN]
          & (attackedBy[Us][ALL_PIECES] | ~attackedBy[Them][ALL_PIECES]);

    score += THREAT_BY_PAWN_PUSH * popcount(pawn_attacks_bb<Us>(push) & nonPawnEnemies);
    if (trace) trace->x.threatPawnPush[Us] =
        popcount(pawn_attacks_bb<Us>(push) & nonPawnEnemies);

    // Lining up on the enemy queen.
    if (pos.count<QUEEN>(Them) == 1) {
        const bool   queenImbalance = (pos.count<QUEEN>(WHITE) + pos.count<QUEEN>(BLACK)) == 1;
        const Square qsq            = lsb(pos.pieces(Them, QUEEN));
        const Bitboard safeSquares  = mobilityArea[Us] & ~stronglyProtected;

        Bitboard b = attackedBy[Us][KNIGHT] & attacks_bb<KNIGHT>(qsq);
        score += KNIGHT_ON_QUEEN * popcount(b & safeSquares) * (1 + queenImbalance);

        b = (attackedBy[Us][BISHOP] & attacks_bb<BISHOP>(qsq, pos.pieces()))
          | (attackedBy[Us][ROOK] & attacks_bb<ROOK>(qsq, pos.pieces()));
        score += SLIDER_ON_QUEEN * popcount(b & safeSquares & attackedBy2[Us])
               * (1 + queenImbalance);
    }

    return score;
}

template<Color Us>
Score Evaluation::passed() const {
    constexpr Color     Them = ~Us;
    constexpr Direction Up   = pawn_push(Us);

    Score score;

    Bitboard b = pe->passed_pawns(Us);

    while (b) {
        const Square s = pop_lsb(b);
        const Rank   r = relative_rank(Us, s);

        assert(!(pos.pieces(Them, PAWN) & forward_file_bb(Us, s)));

        score += PASSED_RANK[r];
        score += PASSED_FILE * edge_distance(file_of(s));
        if (trace) {
            ++trace->x.passerCount[Us];
            if (int(r) > trace->x.passerMaxRank[Us]) trace->x.passerMaxRank[Us] = int(r);
        }

        // The bonus is worth much more once the pawn is actually running.
        if (r > RANK_3) {
            const Square blockSq = s + Up;
            const int    w       = 5 * int(r) - 13;

            // How far each king is from the square in front of the pawn. This
            // is what decides most pawn endings.
            score += PASSER_KING_DISTANCE_THEIRS
                   * (std::min(distance(pos.king_square(Them), blockSq), 5) * w);
            score += PASSER_KING_DISTANCE_OURS
                   * (std::min(distance(pos.king_square(Us), blockSq), 5) * w);

            // The stop square being free and controlled by us is the difference
            // between a passer and a passer that queens.
            if (pos.empty(blockSq)) {
                const Bitboard squaresToQueen = forward_file_bb(Us, s);

                // A rook or queen behind the passer defends the whole path.
                Bitboard behind = forward_file_bb(Them, s)
                                & pos.pieces(ROOK, QUEEN)
                                & attacks_bb<ROOK>(s, pos.pieces());

                Bitboard defendedPath = attackedBy[Us][ALL_PIECES];
                Bitboard unsafePath   = attackedBy[Them][ALL_PIECES] | pos.pieces(Them);

                if (behind & pos.pieces(Us))
                    defendedPath |= squaresToQueen;
                if (behind & pos.pieces(Them))
                    unsafePath |= squaresToQueen;

                if (trace && !(unsafePath & squaresToQueen)) ++trace->x.passerFree[Us];
                if (!(unsafePath & squaresToQueen))
                    score += PASSED_FREE_ADVANCE * 2;
                else if (defendedPath & blockSq)
                    score += PASSED_FREE_ADVANCE;

                if (behind & pos.pieces(Us, ROOK))
                    score += ROOK_BEHIND_PASSER;
                if (pos.pieces(Them, ROOK, QUEEN) & forward_file_bb(Us, s))
                    score += ROOK_BLOCKS_PASSER;
            }
        }
    }

    return score;
}

// Safe squares behind our own pawns on the central files. A crude but effective
// proxy for room to manoeuvre — and meaningless without pieces to manoeuvre,
// hence the material threshold applied by the caller.
template<Color Us>
int Evaluation::space() const {
    constexpr Color    Them = ~Us;
    constexpr Direction Down = -pawn_push(Us);

    const Bitboard safe = SPACE_MASK[Us] & ~pos.pieces(Us, PAWN) & ~attackedBy[Them][PAWN];

    // Squares behind our own pawns count double: that is the space we can
    // actually use without moving anything.
    Bitboard behind = pos.pieces(Us, PAWN);
    behind |= shift<Down>(behind);
    behind |= shift<Down>(shift<Down>(behind));

    const int bonus = popcount(safe) + popcount(behind & safe & ~attackedBy[Them][ALL_PIECES]);
    const int weight = pos.count(Us) - 3 + std::min(pe->blocked_count(), 9);

    if (trace) { trace->x.spaceBonus[Us] = bonus; trace->x.spaceWeight[Us] = weight; }
    return bonus * weight * weight / 16;
}

// New terms that need no per-piece loop of their own: one shift, one popcount,
// and three reads out of the pawn-key cache. RULES.md R4.
template<Color Us>
void Evaluation::extras() const {
    constexpr Color     Them = ~Us;
    constexpr Direction Down = -pawn_push(Us);
    constexpr Bitboard  EnemyHalf =
        (Us == WHITE ? RANK_5BB | RANK_6BB | RANK_7BB | RANK_8BB
                     : RANK_4BB | RANK_3BB | RANK_2BB | RANK_1BB);

    if (!trace) return;

    // A minor standing on the square an enemy passer must cross is the classic
    // blockader; the passer bitboards are already cached, so this is a shift.
    trace->x.blockaderMinor[Us] =
        popcount(shift<Down>(pe->passed_pawns(Them)) & pos.pieces(Us, KNIGHT, BISHOP));

    trace->x.infiltration[Us] =
        popcount(pos.pieces(Us) & ~pos.pieces(Us, PAWN, KING) & EnemyHalf);

    trace->x.connectedPassers[Us] = pe->connected_passers(Us);
    trace->x.candidatePassers[Us] = pe->candidate_passers(Us);
    trace->x.bothFlanks[Us]       = pe->both_flanks(Us);
}

// How much of the endgame score should actually be believed. Some endings are
// far more drawish than the material count suggests.
int Evaluation::scale_factor(Value eg) const {
    const Color strong = (eg > VALUE_ZERO) ? WHITE : BLACK;
    const Color weak   = ~strong;

    // Opposite-coloured bishops and nothing else: the classic fortress. Each
    // side controls a colour complex the other cannot touch.
    if (pos.count<BISHOP>(WHITE) == 1 && pos.count<BISHOP>(BLACK) == 1
        && bool(pos.pieces(WHITE, BISHOP) & DARK_SQUARES)
             != bool(pos.pieces(BLACK, BISHOP) & DARK_SQUARES)) {

        if (pos.non_pawn_material(WHITE) == SIMPLE_PIECE_VALUE[BISHOP]
            && pos.non_pawn_material(BLACK) == SIMPLE_PIECE_VALUE[BISHOP])
            return SCALE_OPPOSITE_BISHOPS;

        // Even with other pieces on, opposite bishops make progress hard.
        return 38;
    }

    // No pawns for the stronger side and only a small material edge: there is
    // nothing to promote, so there is usually nothing to win.
    if (!pos.count<PAWN>(strong)) {
        const Value diff = pos.non_pawn_material(strong) - pos.non_pawn_material(weak);

        if (diff <= SIMPLE_PIECE_VALUE[BISHOP])
            return SCALE_DRAW;
        if (diff <= SIMPLE_PIECE_VALUE[ROOK])
            return 12;
    }

    // A single extra pawn in a piece endgame is often not enough.
    if (pos.count<PAWN>(strong) == 1
        && pos.non_pawn_material(strong) - pos.non_pawn_material(weak)
             <= SIMPLE_PIECE_VALUE[BISHOP])
        return SCALE_ONE_PAWN;

    // Very few pawns left: scale down gently as the count drops.
    const int pawnCount = pos.count<PAWN>(strong);
    if (pawnCount <= 3)
        return SCALE_NORMAL - 8 * (4 - pawnCount);

    return SCALE_NORMAL;
}

Value Evaluation::value() {
    assert(!pos.checkers());

    // Material and placement come free: the position maintains them
    // incrementally through do_move().
    const Score psqt = pos.psq_score();

    pe = Pawns::probe(pos);
    const Score pawnWhite = pe->pawn_score(WHITE);
    const Score pawnBlack = pe->pawn_score(BLACK);

    // The attack maps have to exist for both sides before any term that reads
    // them, so initialisation and piece evaluation are separate passes.
    initialize<WHITE>();
    initialize<BLACK>();

    const Score knightWhite = pieces<WHITE, KNIGHT>();
    const Score knightMobilityWhite = mobility[WHITE];
    const Score knightBlack = pieces<BLACK, KNIGHT>();
    const Score knightMobilityBlack = mobility[BLACK];
    const Score bishopWhite = pieces<WHITE, BISHOP>();
    const Score bishopMobilityWhite = mobility[WHITE] - knightMobilityWhite;
    const Score bishopBlack = pieces<BLACK, BISHOP>();
    const Score bishopMobilityBlack = mobility[BLACK] - knightMobilityBlack;
    const Score rookWhite = pieces<WHITE, ROOK>();
    const Score rookMobilityWhite = mobility[WHITE] - knightMobilityWhite - bishopMobilityWhite;
    const Score rookBlack = pieces<BLACK, ROOK>();
    const Score rookMobilityBlack = mobility[BLACK] - knightMobilityBlack - bishopMobilityBlack;
    const Score queenWhite = pieces<WHITE, QUEEN>();
    const Score queenMobilityWhite = mobility[WHITE] - knightMobilityWhite
                                   - bishopMobilityWhite - rookMobilityWhite;
    const Score queenBlack = pieces<BLACK, QUEEN>();
    const Score queenMobilityBlack = mobility[BLACK] - knightMobilityBlack
                                   - bishopMobilityBlack - rookMobilityBlack;
    const Score kingWhite = king<WHITE>();
    const Score kingBlack = king<BLACK>();
    const Score threatsWhite = threats<WHITE>();
    const Score threatsBlack = threats<BLACK>();
    const Score passedWhite = passed<WHITE>();
    const Score passedBlack = passed<BLACK>();
    const int spaceWhite = pos.non_pawn_material() >= SPACE_THRESHOLD ? space<WHITE>() : 0;
    const int spaceBlack = pos.non_pawn_material() >= SPACE_THRESHOLD ? space<BLACK>() : 0;
    const bool bishopPairWhite = pos.count<BISHOP>(WHITE) >= 2;
    const bool bishopPairBlack = pos.count<BISHOP>(BLACK) >= 2;

    Score score = psqt + pawnWhite - pawnBlack
                + knightWhite - knightBlack + bishopWhite - bishopBlack
                + rookWhite - rookBlack + queenWhite - queenBlack
                + mobility[WHITE] - mobility[BLACK] + kingWhite - kingBlack
                + threatsWhite - threatsBlack + passedWhite - passedBlack
                + S(spaceWhite - spaceBlack, 0);

    // Two bishops cover both colour complexes.
    if (bishopPairWhite)
        score += BISHOP_PAIR;
    if (bishopPairBlack)
        score -= BISHOP_PAIR;

    // Interpolate, then temper the endgame half by how winnable the ending
    // actually is.
    extras<WHITE>();
    extras<BLACK>();

    const int phase = std::clamp(pos.phase(), 0, PHASE_MIDGAME);
    const int sf    = scale_factor(Value(score.eg));

    Value v = Value((score.mg * phase + score.eg * (PHASE_MIDGAME - phase) * sf / SCALE_NORMAL)
                    / PHASE_MIDGAME);

    // Shuffling towards the fifty-move rule should drift the score to zero:
    // a "winning" position you cannot convert in fifty moves is a draw.
    v = Value(v * (100 - pos.rule50_count()) / 100);

    if (trace) {
        trace->psqt = psqt;
        trace->section[PAWNS][WHITE] = pawnWhite;
        trace->section[PAWNS][BLACK] = pawnBlack;
        trace->section[KNIGHT_BONUS][WHITE] = knightWhite;
        trace->section[KNIGHT_BONUS][BLACK] = knightBlack;
        trace->section[BISHOP_BONUS][WHITE] = bishopWhite;
        trace->section[BISHOP_BONUS][BLACK] = bishopBlack;
        trace->section[ROOK_BONUS][WHITE] = rookWhite;
        trace->section[ROOK_BONUS][BLACK] = rookBlack;
        trace->section[QUEEN_BONUS][WHITE] = queenWhite;
        trace->section[QUEEN_BONUS][BLACK] = queenBlack;
        trace->section[TRACE_KNIGHT_MOBILITY][WHITE] = knightMobilityWhite;
        trace->section[TRACE_KNIGHT_MOBILITY][BLACK] = knightMobilityBlack;
        trace->section[TRACE_BISHOP_MOBILITY][WHITE] = bishopMobilityWhite;
        trace->section[TRACE_BISHOP_MOBILITY][BLACK] = bishopMobilityBlack;
        trace->section[TRACE_ROOK_MOBILITY][WHITE] = rookMobilityWhite;
        trace->section[TRACE_ROOK_MOBILITY][BLACK] = rookMobilityBlack;
        trace->section[TRACE_QUEEN_MOBILITY][WHITE] = queenMobilityWhite;
        trace->section[TRACE_QUEEN_MOBILITY][BLACK] = queenMobilityBlack;
        trace->section[KING_SAFETY][WHITE] = kingWhite;
        trace->section[KING_SAFETY][BLACK] = kingBlack;
        trace->section[THREATS][WHITE] = threatsWhite;
        trace->section[THREATS][BLACK] = threatsBlack;
        trace->section[PASSERS][WHITE] = passedWhite;
        trace->section[PASSERS][BLACK] = passedBlack;
        trace->space[WHITE] = spaceWhite;
        trace->space[BLACK] = spaceBlack;
        trace->bishopPair[WHITE] = bishopPairWhite;
        trace->bishopPair[BLACK] = bishopPairBlack;
        trace->phase = phase;
        trace->scale = sf;
        trace->rule50 = pos.rule50_count();
        trace->whiteValue = v;
    }

    return v;
}

}  // namespace

Value evaluate_trace(const Position& pos, Trace& trace) {
    trace = Trace{};
    // In check there is no meaningful static score; the search never asks for
    // one, but guard anyway rather than return nonsense.
    assert(!pos.checkers());

    // Endings we know outright bypass the heuristics entirely. Without this a
    // general evaluation cheerfully shuffles in a drawn king-and-pawn ending,
    // and never mates with bishop and knight at all.
    Value known;
    if (Endgames::probe(pos, known)) {
        trace.knownEndgame = true;
        trace.finalValue = known;
        return known;
    }

    const Value v = Evaluation(pos, &trace).value();
    trace.finalValue = (pos.side_to_move() == WHITE ? v : -v) + TEMPO;
    return trace.finalValue;
}

Value evaluate(const Position& pos) {
    Trace trace;
    const Value boringValue = evaluate_trace(pos, trace);
    if (trace.knownEndgame)
        return boringValue;
    return ForgeFactor::score(pos, boringValue, trace, &pos.psq_planes());
}

}  // namespace Eval
}  // namespace boring
