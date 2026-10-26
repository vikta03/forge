// history.h — the move-ordering statistics the search accumulates.
//
// Every table here answers the same question in a different way: "when this
// sort of move has been tried before, did it turn out to be good?" They are
// what makes a move-ordering scheme adaptive rather than fixed, and they are
// deliberately kept alive across `go` commands — the ordering knowledge learned
// while thinking about one move is still valid for the next.

#ifndef BORING_HISTORY_H
#define BORING_HISTORY_H

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "types.h"

namespace boring {

// A saturating counter. Applying a bonus moves the value towards +/-D by an
// amount proportional to how far it still has to go, so entries approach the
// limit but never exceed it. Without this, a move that has cut off thousands of
// times would drown out one that has cut off ten times in a far more relevant
// position.
template<int D>
struct StatsEntry {
    int16_t v = 0;

    operator int() const { return v; }
    void operator=(int x) { v = int16_t(x); }

    void update(int bonus) {
        bonus = std::clamp(bonus, -D, D);
        v += int16_t(bonus - v * std::abs(bonus) / D);
    }
};

constexpr int MAIN_HISTORY_LIMIT         = 7000;
constexpr int CAPTURE_HISTORY_LIMIT      = 10000;
constexpr int CONTINUATION_HISTORY_LIMIT = 25000;

// Quiet moves indexed by who is moving and from where to where. Deliberately
// ignores which piece moved: the point is to notice that, say, "something to g5"
// keeps refuting things in this search.
struct ButterflyHistory {
    StatsEntry<MAIN_HISTORY_LIMIT> table[COLOR_NB][int(SQUARE_NB) * int(SQUARE_NB)];

    StatsEntry<MAIN_HISTORY_LIMIT>& operator()(Color c, Move m) { return table[c][from_to(m)]; }
    int operator()(Color c, Move m) const { return table[c][from_to(m)]; }

    void clear() { std::memset(table, 0, sizeof(table)); }
};

// Captures indexed by the moving piece, its destination, and what it took.
// Ordering captures by MVV-LVA alone cannot learn that a particular recapture
// is bad in this structure; this can.
struct CaptureHistory {
    StatsEntry<CAPTURE_HISTORY_LIMIT> table[PIECE_NB][SQUARE_NB][PIECE_TYPE_NB];

    StatsEntry<CAPTURE_HISTORY_LIMIT>& operator()(Piece pc, Square to, PieceType captured) {
        return table[pc][to][captured];
    }
    int operator()(Piece pc, Square to, PieceType captured) const {
        return table[pc][to][captured];
    }

    void clear() { std::memset(table, 0, sizeof(table)); }
};

// One plane of continuation history: "given the move that was just played, how
// good is this reply?"
struct PieceToHistory {
    StatsEntry<CONTINUATION_HISTORY_LIMIT> table[PIECE_NB][SQUARE_NB];

    StatsEntry<CONTINUATION_HISTORY_LIMIT>& operator()(Piece pc, Square to) {
        return table[pc][to];
    }
    int operator()(Piece pc, Square to) const { return table[pc][to]; }
};

// Indexed by the earlier move, giving the plane above. The search keeps
// pointers to the planes for one and two plies back.
struct ContinuationHistory {
    PieceToHistory table[PIECE_NB][SQUARE_NB];

    PieceToHistory& operator()(Piece pc, Square to) { return table[pc][to]; }

    void clear() { std::memset(table, 0, sizeof(table)); }
};

// The move that refuted a given move last time it was played.
struct CounterMoveHistory {
    Move table[PIECE_NB][SQUARE_NB] = {};

    Move& operator()(Piece pc, Square to) { return table[pc][to]; }
    Move  operator()(Piece pc, Square to) const { return table[pc][to]; }

    void clear() { std::memset(table, 0, sizeof(table)); }
};

// The bonus a cutoff at this depth is worth. Grows with depth because a cutoff
// found deep in the tree is stronger evidence than one found at depth 1.
inline int history_bonus(int depth) { return std::min(155 * depth - 90, 1800); }

// The penalty applied to the quiet moves that were searched and did *not* cause
// the cutoff. Applying only bonuses inflates every counter towards the limit
// and destroys the ordering signal.
inline int history_malus(int depth) { return std::min(700 * depth - 200, 2400); }

}  // namespace boring

#endif  // BORING_HISTORY_H
