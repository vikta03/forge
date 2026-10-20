// movegen.h — pseudo-legal move generation, split by the categories the search
// actually asks for.
//
// CAPTURES and QUIETS partition the move list exactly: every capture and every
// queen promotion lands in CAPTURES, every quiet move and every under-promotion
// push lands in QUIETS. That partition is what lets MovePicker walk the stages
// without ever generating a move twice or missing one.

#ifndef BORING_MOVEGEN_H
#define BORING_MOVEGEN_H

#include <algorithm>
#include <cstddef>

#include "types.h"

namespace boring {

class Position;

enum GenType {
    CAPTURES,      // captures + queen promotions — the quiescence set
    QUIETS,        // everything else, including under-promotion pushes
    EVASIONS,      // legal-ish replies when in check
    NON_EVASIONS,  // CAPTURES + QUIETS, for when we are not in check
    LEGAL          // fully filtered
};

// A move plus the score the picker sorts on. Implicitly converts to Move so
// call sites that do not care about the score read naturally.
struct ExtMove {
    Move move;
    int  value;

    void operator=(Move m) { move = m; }
    operator Move() const { return move; }

    // Prevent the implicit conversion from being used where a bool is wanted.
    operator float() const = delete;
};

inline bool operator<(const ExtMove& a, const ExtMove& b) { return a.value < b.value; }

template<GenType>
ExtMove* generate(const Position& pos, ExtMove* moveList);

// Generates on construction; iterate it directly with a range-for.
template<GenType T>
struct MoveList {
    explicit MoveList(const Position& pos) : last(generate<T>(pos, moveList)) {}

    const ExtMove* begin() const { return moveList; }
    const ExtMove* end() const { return last; }
    size_t         size() const { return size_t(last - moveList); }

    bool contains(Move m) const { return std::find(begin(), end(), m) != end(); }

   private:
    ExtMove  moveList[MAX_MOVES];
    ExtMove* last;
};

}  // namespace boring

#endif  // BORING_MOVEGEN_H
