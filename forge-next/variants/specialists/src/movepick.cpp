
#ifdef FORGE_NEXT_PROBE
#include "forge_next_probe.h"
#endif
// movepick.cpp — the staged move picker.

#include "movepick.h"

#include <algorithm>
#include <limits>
#include <iostream>

#include "eval_terms.h"
#include "search.h"
#include "uci.h"

namespace boring {

namespace {

enum Stage {
    // Not in check, full search.
    MAIN_TT,
    CAPTURE_INIT,
    GOOD_CAPTURE,
    REFUTATION,
    QUIET_INIT,
    QUIET,
    BAD_CAPTURE,

    // In check (both full search and quiescence).
    EVASION_TT,
    EVASION_INIT,
    EVASION,

    // Quiescence, not in check.
    QSEARCH_TT,
    QCAPTURE_INIT,
    QCAPTURE,

    // ProbCut.
    PROBCUT_TT,
    PROBCUT_INIT,
    PROBCUT
};

// A capture is "good" if it does not lose material outright. The small negative
// threshold lets through captures that are a hair down but open lines.
constexpr int GOOD_CAPTURE_THRESHOLD = -55;

// Sort only the moves worth sorting. Anything scoring below `limit` keeps its
// generated order, on the assumption we will never search that far down the
// list. Insertion sort, because the sorted prefix stays short.
void partial_insertion_sort(ExtMove* begin, ExtMove* end, int limit) {
    for (ExtMove *sortedEnd = begin, *p = begin + 1; p < end; ++p)
        if (p->value >= limit) {
            const ExtMove tmp = *p;
            *p                = *++sortedEnd;

            ExtMove* q = sortedEnd;
            for (; q != begin && *(q - 1) < tmp; --q)
                *q = *(q - 1);
            *q = tmp;
        }
}

}  // namespace

MovePicker::MovePicker(const Position&         p,
                       Move                    ttm,
                       int                     d,
                       const ButterflyHistory* mh,
                       const CaptureHistory*   ch,
                       const PieceToHistory**  continuation,
                       const Move*             killers,
                       Move                    counter)
    : pos(p),
      mainHistory(mh),
      captureHistory(ch),
      continuationHistory(continuation),
      ttMove(ttm),
      refutations{{killers[0], 0}, {killers[1], 0}, {counter, 0}},
      depth(d) {

    assert(d > 0);

    // A move from the transposition table is only trusted after validation: the
    // 16-bit key check can collide, and killers and countermoves were recorded
    // in entirely different positions.
    stage = (pos.checkers() ? EVASION_TT : MAIN_TT) + !(ttm && pos.pseudo_legal(ttm));
}

MovePicker::MovePicker(const Position&         p,
                       Move                    ttm,
                       const ButterflyHistory* mh,
                       const CaptureHistory*   ch,
                       const PieceToHistory**  continuation)
    : pos(p),
      mainHistory(mh),
      captureHistory(ch),
      continuationHistory(continuation),
      ttMove(ttm),
      refutations{{MOVE_NONE, 0}, {MOVE_NONE, 0}, {MOVE_NONE, 0}},
      depth(0) {

    stage = (pos.checkers() ? EVASION_TT : QSEARCH_TT) + !(ttm && pos.pseudo_legal(ttm));
}

MovePicker::MovePicker(const Position& p, Move ttm, int th, const CaptureHistory* ch)
    : pos(p),
      mainHistory(nullptr),
      captureHistory(ch),
      continuationHistory(nullptr),
      ttMove(ttm),
      refutations{{MOVE_NONE, 0}, {MOVE_NONE, 0}, {MOVE_NONE, 0}},
      depth(0),
      threshold(th) {

    assert(!pos.checkers());

    // The transposition move is only usable here if it is itself a capture good
    // enough to clear the threshold.
    stage = PROBCUT_TT
          + !(ttm && pos.capture_stage(ttm) && pos.pseudo_legal(ttm)
              && pos.see_ge(ttm, threshold));
}

// Captures are ordered by what they win — most valuable victim first — adjusted
// by how this particular capture has historically turned out.
template<>
void MovePicker::score<CAPTURES>() {
    for (ExtMove& m : *this) {
        const Piece moved = pos.moved_piece(m);
        const PieceType captured = type_of(m) == EN_PASSANT && Search::SpecialMoveSEE
            ? PAWN : type_of(pos.piece_on(to_sq(m)));
        const int promotionGain = type_of(m) == PROMOTION && Search::SpecialMoveSEE
            ? Eval::SIMPLE_PIECE_VALUE[promotion_type(m)]
                - Eval::SIMPLE_PIECE_VALUE[PAWN]
            : 0;

        m.value = 7 * (Eval::SIMPLE_PIECE_VALUE[captured] + promotionGain)
                + (*captureHistory)(moved, to_sq(m), captured) / 16;
    }
}

// Quiet moves carry no material signal, so history supplies their base ordering:
// how often this from-to has caused a cutoff, and how well it has worked as a
// reply to the previous move and the one before that.
template<>
void MovePicker::score<QUIETS>() {
    const Color us = pos.side_to_move();

    for (ExtMove& m : *this) {
        const Piece  moved = pos.moved_piece(m);
        const Square to    = to_sq(m);

        m.value = 2 * (*mainHistory)(us, m);

        if (continuationHistory) {
            m.value += 2 * (*continuationHistory[0])(moved, to);
            m.value += (*continuationHistory[1])(moved, to);
        }

        // Moving a piece onto a square an enemy pawn attacks is nearly always
        // a mistake, and it is worth knowing that before searching it.
        if (type_of(moved) != PAWN && (pos.pieces(~us, PAWN) & pawn_attacks_bb(us, to)))
            m.value -= 4000;
    }
#ifdef FORGE_NEXT_PROBE
    ForgeNext::order_quiets(pos, depth, cur, endMoves);
#endif
}

// Diagnostic uses the real move-picker scoring and sorting, with empty history.
// It never runs during normal search. Generated moves are pseudo-legal; the
// search's normal legality filter is still responsible for rejecting pins.
void MovePicker::inspect_quiets(const Position& pos, int depth) {
    ButterflyHistory history{};
    CaptureHistory captures{};
    const Move killers[2] = {MOVE_NONE, MOVE_NONE};
    MovePicker picker(pos, MOVE_NONE, std::max(depth, 1), &history, &captures,
                      nullptr, killers, MOVE_NONE);
    picker.depth = depth;
    picker.cur = picker.moves;
    picker.endMoves = pos.checkers() ? picker.cur : generate<QUIETS>(pos, picker.cur);
    picker.score<QUIETS>();
    if (picker.cur != picker.endMoves)
        partial_insertion_sort(picker.cur, picker.endMoves, -3000 * std::max(depth, 1));
    std::cout << "forge2order {\"depth\":" << depth << ",\"moves\":[";
    bool first = true;
    for (auto it = picker.cur; it != picker.endMoves; ++it) {
        // Raw king-to-rook castling coordinates agree with forge2inspect.
        const Move move = *it;
        std::string uci = UCI::square(from_sq(move)) + UCI::square(to_sq(move));
        if (type_of(move) == PROMOTION) uci += " pnbrqk"[promotion_type(move)];
        std::cout << (first ? "" : ",") << "{\"move\":\"" << uci
                  << "\",\"score\":" << it->value << '}';
        first = false;
    }
    std::cout << "]}" << std::endl;
}

// Out of check there are few moves, so what matters is trying the obvious ones
// first: take the checker with something cheap, or step away.
template<>
void MovePicker::score<EVASIONS>() {
    const Color us = pos.side_to_move();

    for (ExtMove& m : *this) {
        if (pos.capture(m))
            // Captures ahead of everything else, ordered victim minus attacker.
            m.value = (1 << 28) + Eval::SIMPLE_PIECE_VALUE[type_of(pos.piece_on(to_sq(m)))]
                    - int(type_of(pos.moved_piece(m)));
        else
            m.value = (*mainHistory)(us, m);
    }
}

Move MovePicker::next_move(bool skipQuiets) {
    switch (stage) {

    case MAIN_TT:
    case EVASION_TT:
    case QSEARCH_TT:
    case PROBCUT_TT:
        ++stage;
        return ttMove;

    case CAPTURE_INIT:
    case QCAPTURE_INIT:
        cur = endBadCaptures = moves;
        endMoves             = generate<CAPTURES>(pos, cur);

        score<CAPTURES>();
        partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());

        ++stage;
        return next_move(skipQuiets);

    case GOOD_CAPTURE:
        // Captures that lose material are set aside at the front of the array
        // and replayed after the quiet moves. Writing them there is safe: a
        // slot is only overwritten once its move has already been handed out.
        while (cur < endMoves) {
            const Move m = *cur++;

            if (m == ttMove)
                continue;

            if (pos.see_ge(m, GOOD_CAPTURE_THRESHOLD))
                return m;

            *endBadCaptures++ = m;
        }

        // Line up the refutation moves. A countermove that duplicates a killer
        // would otherwise be searched twice.
        cur      = refutations;
        endMoves = refutations + 3;

        if (refutations[2].move == refutations[0].move
            || refutations[2].move == refutations[1].move)
            --endMoves;

        ++stage;
        [[fallthrough]];

    case REFUTATION:
        // Killers and the countermove: quiet moves that refuted something
        // similar before, worth trying ahead of the mass of ordinary quiets.
        while (cur < endMoves) {
            const Move m = (cur++)->move;

            if (m != MOVE_NONE && m != ttMove && !pos.capture(m) && pos.pseudo_legal(m))
                return m;
        }

        ++stage;
        [[fallthrough]];

    case QUIET_INIT:
        if (!skipQuiets) {
            cur      = endBadCaptures;
            endMoves = generate<QUIETS>(pos, cur);

            score<QUIETS>();

            // Deeper searches can afford to sort more of the list; shallow ones
            // will never reach the tail.
            partial_insertion_sort(cur, endMoves, -3000 * depth);
        }

        ++stage;
        [[fallthrough]];

    case QUIET:
        if (!skipQuiets)
            while (cur < endMoves) {
                const Move m = *cur++;

                if (m != ttMove && m != refutations[0].move && m != refutations[1].move
                    && m != refutations[2].move)
                    return m;
            }

        // Replay the bad captures collected during GOOD_CAPTURE.
        cur      = moves;
        endMoves = endBadCaptures;

        ++stage;
        [[fallthrough]];

    case BAD_CAPTURE:
        while (cur < endMoves) {
            const Move m = *cur++;
            if (m != ttMove)
                return m;
        }
        return MOVE_NONE;

    case EVASION_INIT:
        cur      = moves;
        endMoves = generate<EVASIONS>(pos, cur);

        score<EVASIONS>();
        partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());

        ++stage;
        [[fallthrough]];

    case EVASION:
    case QCAPTURE:
        while (cur < endMoves) {
            const Move m = *cur++;
            if (m != ttMove)
                return m;
        }
        return MOVE_NONE;

    case PROBCUT_INIT:
        cur      = moves;
        endMoves = generate<CAPTURES>(pos, cur);

        score<CAPTURES>();
        partial_insertion_sort(cur, endMoves, std::numeric_limits<int>::min());

        ++stage;
        [[fallthrough]];

    case PROBCUT:
        while (cur < endMoves) {
            const Move m = *cur++;
            if (m != ttMove && pos.see_ge(m, threshold))
                return m;
        }
        return MOVE_NONE;
    }

    assert(false);
    return MOVE_NONE;
}

}  // namespace boring
