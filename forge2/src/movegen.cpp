// movegen.cpp — set-wise pseudo-legal move generation.
//
// Pawn moves are produced by shifting whole bitboards; every other piece is a
// loop over its squares. Nothing here checks whether a move leaves the king in
// check — that is Position::legal()'s job, and generate<LEGAL> applies it.

#include "movegen.h"

#include "bitboard.h"
#include "position.h"

namespace boring {

namespace {

// Emit the promotions for a pawn arriving on `to` from direction D.
//
// Queen promotions belong to CAPTURES (they are the only promotions quiescence
// search should see). Under-promotions follow their move: a capture-promotion's
// under-promotions go with CAPTURES, a push-promotion's with QUIETS. That keeps
// the two sets disjoint and their union complete.
template<GenType Type, Direction D, bool Enemy>
ExtMove* make_promotions(ExtMove* moveList, Square to) {
    constexpr bool all = (Type == EVASIONS || Type == NON_EVASIONS);

    if constexpr (Type == CAPTURES || all)
        *moveList++ = make_move<PROMOTION>(to - D, to, QUEEN);

    if constexpr ((Type == CAPTURES && Enemy) || (Type == QUIETS && !Enemy) || all) {
        *moveList++ = make_move<PROMOTION>(to - D, to, ROOK);
        *moveList++ = make_move<PROMOTION>(to - D, to, BISHOP);
        *moveList++ = make_move<PROMOTION>(to - D, to, KNIGHT);
    }

    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_pawn_moves(const Position& pos, ExtMove* moveList, Bitboard target) {
    constexpr Color     Them     = ~Us;
    constexpr Bitboard  TRank7BB = (Us == WHITE ? RANK_7BB : RANK_2BB);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? RANK_3BB : RANK_6BB);
    constexpr Direction Up       = pawn_push(Us);
    constexpr Direction UpRight  = (Us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction UpLeft   = (Us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard pawnsOn7    = pos.pieces(Us, PAWN) & TRank7BB;
    const Bitboard pawnsNotOn7 = pos.pieces(Us, PAWN) & ~TRank7BB;
    const Bitboard emptySquares = ~pos.pieces();

    // When in check, the only capture that helps is taking the checker.
    const Bitboard enemies = (Type == EVASIONS) ? pos.checkers() : pos.pieces(Them);

    // --- single and double pushes ---
    if constexpr (Type != CAPTURES) {
        Bitboard b1 = shift<Up>(pawnsNotOn7) & emptySquares;
        Bitboard b2 = shift<Up>(b1 & TRank3BB) & emptySquares;

        if constexpr (Type == EVASIONS) {
            // `target` is "block the check or capture the checker"; a push can
            // only ever do the former, and it is already masked to empty squares.
            b1 &= target;
            b2 &= target;
        }

        while (b1) {
            const Square to = pop_lsb(b1);
            *moveList++     = make_move(to - Up, to);
        }
        while (b2) {
            const Square to = pop_lsb(b2);
            *moveList++     = make_move(to - Up - Up, to);
        }
    }

    // --- promotions ---
    if (pawnsOn7) {
        Bitboard b1 = shift<UpRight>(pawnsOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsOn7) & enemies;
        Bitboard b3 = shift<Up>(pawnsOn7) & emptySquares;

        if constexpr (Type == EVASIONS)
            b3 &= target;

        while (b1)
            moveList = make_promotions<Type, UpRight, true>(moveList, pop_lsb(b1));
        while (b2)
            moveList = make_promotions<Type, UpLeft, true>(moveList, pop_lsb(b2));
        while (b3)
            moveList = make_promotions<Type, Up, false>(moveList, pop_lsb(b3));
    }

    // --- ordinary captures and en passant ---
    if constexpr (Type != QUIETS) {
        Bitboard b1 = shift<UpRight>(pawnsNotOn7) & enemies;
        Bitboard b2 = shift<UpLeft>(pawnsNotOn7) & enemies;

        while (b1) {
            const Square to = pop_lsb(b1);
            *moveList++     = make_move(to - UpRight, to);
        }
        while (b2) {
            const Square to = pop_lsb(b2);
            *moveList++     = make_move(to - UpLeft, to);
        }

        if (pos.ep_square() != SQ_NONE) {
            const Square epSq = pos.ep_square();
            assert(rank_of(epSq) == relative_rank(Us, RANK_6));

            // When in check, capturing en passant only helps if the checker is
            // the pawn being captured. `target` covers blocking squares, which
            // en passant can also do, so test the captured pawn explicitly.
            if constexpr (Type == EVASIONS)
                if (!(enemies & (epSq - Up)))
                    return moveList;

            Bitboard b = pawn_attacks_bb(Them, epSq) & pawnsNotOn7;
            assert(b);

            while (b)
                *moveList++ = make_move<EN_PASSANT>(pop_lsb(b), epSq);
        }
    }

    return moveList;
}

template<Color Us, PieceType Pt>
ExtMove* generate_piece_moves(const Position& pos, ExtMove* moveList, Bitboard target) {
    static_assert(Pt != KING && Pt != PAWN, "kings and pawns are generated separately");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb) {
        const Square from = pop_lsb(bb);
        Bitboard     b    = attacks_bb(Pt, from, pos.pieces()) & target;

        while (b)
            *moveList++ = make_move(from, pop_lsb(b));
    }

    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_all(const Position& pos, ExtMove* moveList) {
    const Square   ksq      = pos.king_square(Us);
    const Bitboard checkers = pos.checkers();

    // Under double check nothing but a king move can help, so skip everything
    // else rather than generating moves that all turn out to be illegal.
    const bool onlyKingMoves = (Type == EVASIONS) && more_than_one(checkers);

    Bitboard target = 0;

    if (!onlyKingMoves) {
        target = (Type == EVASIONS)     ? BETWEEN_BB[ksq][lsb(checkers)]
               : (Type == NON_EVASIONS) ? ~pos.pieces(Us)
               : (Type == CAPTURES)     ? pos.pieces(~Us)
                                        : ~pos.pieces();  // QUIETS

        moveList = generate_pawn_moves<Us, Type>(pos, moveList, target);
        moveList = generate_piece_moves<Us, KNIGHT>(pos, moveList, target);
        moveList = generate_piece_moves<Us, BISHOP>(pos, moveList, target);
        moveList = generate_piece_moves<Us, ROOK>(pos, moveList, target);
        moveList = generate_piece_moves<Us, QUEEN>(pos, moveList, target);
    }

    // The king can go anywhere not occupied by his own men; whether the square
    // is attacked is settled by Position::legal().
    {
        Bitboard b = attacks_bb<KING>(ksq)
                   & ((Type == EVASIONS) ? ~pos.pieces(Us) : target);

        while (b)
            *moveList++ = make_move(ksq, pop_lsb(b));
    }

    if constexpr (Type == QUIETS || Type == NON_EVASIONS) {
        constexpr CastlingRights KingSide  = (Us == WHITE ? WHITE_OO : BLACK_OO);
        constexpr CastlingRights QueenSide = (Us == WHITE ? WHITE_OOO : BLACK_OOO);

        // Encoded king-takes-own-rook: in Chess960 the king's destination alone
        // does not identify the move, but the rook's square always does.
        for (CastlingRights cr : {KingSide, QueenSide})
            if (pos.can_castle(cr) && !pos.castling_impeded(cr))
                *moveList++ = make_move<CASTLING>(ksq, pos.castling_rook_square(cr));
    }

    return moveList;
}

}  // namespace

// CAPTURES     — captures and queen promotions. Assumes we are not in check.
// QUIETS       — everything else. Assumes we are not in check.
// NON_EVASIONS — both of the above. Assumes we are not in check.
// EVASIONS     — replies to a check. Assumes we *are* in check.
template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) {
    static_assert(Type != LEGAL, "generate<LEGAL> has its own specialisation");
    assert((Type == EVASIONS) == bool(pos.checkers()));

    return pos.side_to_move() == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                                       : generate_all<BLACK, Type>(pos, moveList);
}

template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<EVASIONS>(const Position&, ExtMove*);
template ExtMove* generate<NON_EVASIONS>(const Position&, ExtMove*);

// Generate pseudo-legal moves, then drop the ones that leave our king in check.
// Only three kinds of move can possibly be illegal, so the filter is cheap:
// moves by a pinned piece, king moves, and en passant.
template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) {
    const Color    us     = pos.side_to_move();
    const Bitboard pinned = pos.blockers_for_king(us) & pos.pieces(us);
    const Square   ksq    = pos.king_square(us);

    ExtMove* cur = moveList;

    moveList = pos.checkers() ? generate<EVASIONS>(pos, moveList)
                              : generate<NON_EVASIONS>(pos, moveList);

    while (cur != moveList) {
        const Move m = *cur;

        if (((pinned & from_sq(m)) || from_sq(m) == ksq || type_of(m) == EN_PASSANT)
            && !pos.legal(m))
            *cur = *(--moveList);  // overwrite with the last move and re-test
        else
            ++cur;
    }

    return moveList;
}

}  // namespace boring
