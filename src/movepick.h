// movepick.h — lazy, staged move selection.
//
// The picker never builds a full sorted move list. It walks stages in the order
// a cutoff is most likely, generating and scoring only when a stage is actually
// reached. Most nodes fail high on the transposition move or the first good
// capture, and in those nodes no quiet move is ever generated, let alone scored.

#ifndef BORING_MOVEPICK_H
#define BORING_MOVEPICK_H

#include "history.h"
#include "movegen.h"
#include "position.h"
#include "types.h"

namespace boring {

class MovePicker {
   public:
    MovePicker(const MovePicker&)            = delete;
    MovePicker& operator=(const MovePicker&) = delete;

    // Main search.
    MovePicker(const Position& p,
               Move            ttm,
               int             d,
               const ButterflyHistory*    mh,
               const CaptureHistory*      ch,
               const PieceToHistory**     continuation,
               const Move*                killers,
               Move                       counter);

    // ProbCut: only captures that win at least `threshold` material, since the
    // whole point is to find a move good enough to prove a fail high cheaply.
    MovePicker(const Position& p, Move ttm, int threshold, const CaptureHistory* ch);

    // Quiescence search: captures and queen promotions only, plus evasions when
    // in check.
    MovePicker(const Position& p,
               Move            ttm,
               const ButterflyHistory* mh,
               const CaptureHistory*   ch,
               const PieceToHistory**  continuation);

    // Returns MOVE_NONE when the stages are exhausted. With skipQuiets set, the
    // quiet stages are passed over — used once a node has decided that late
    // quiet moves are not worth searching.
    Move next_move(bool skipQuiets = false);

   private:
    template<GenType>
    void score();

    ExtMove* begin() { return cur; }
    ExtMove* end() { return endMoves; }

    const Position&             pos;
    const ButterflyHistory*     mainHistory;
    const CaptureHistory*       captureHistory;
    const PieceToHistory**      continuationHistory;

    Move     ttMove;
    ExtMove  refutations[3];  // killer 1, killer 2, countermove
    ExtMove* cur;
    ExtMove* endMoves;
    ExtMove* endBadCaptures;

    int  stage;
    int  depth;
    int  threshold = 0;

    ExtMove moves[MAX_MOVES];
};

}  // namespace boring

#endif  // BORING_MOVEPICK_H
