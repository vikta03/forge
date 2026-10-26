// eval.h — the interface to the hand-crafted evaluation.

#ifndef BORING_EVAL_H
#define BORING_EVAL_H

#include <algorithm>

#include "types.h"

namespace boring {

class Position;

namespace Eval {

enum TraceSection : int {
    PAWNS,
    KNIGHT_BONUS, BISHOP_BONUS, ROOK_BONUS, QUEEN_BONUS,
    TRACE_KNIGHT_MOBILITY, TRACE_BISHOP_MOBILITY, TRACE_ROOK_MOBILITY, TRACE_QUEEN_MOBILITY,
    KING_SAFETY, THREATS, PASSERS,
    TRACE_SECTION_NB
};

// Quantities Boring computes internally. E = already calculated today and
// discarded before the trace; N = newly calculated (see RULES.md R4: each is
// inside an existing loop or inside the pawn-key-cached entry).
struct TraceX {
    // --- E: emitted, previously discarded ---
    int kingDanger[COLOR_NB]{};       // pre-quadratic, pre-threshold
    int shelterMg[COLOR_NB]{};        // pawn shelter alone
    int shelterEg[COLOR_NB]{};        // king-to-own-pawns distance alone
    int safeChecks[COLOR_NB]{};       // 4-bit mask R,Q,B,N
    int kingAttackers[COLOR_NB]{};
    int flankAttack[COLOR_NB]{};
    int flankDefense[COLOR_NB]{};
    int hanging[COLOR_NB]{};
    int restricted[COLOR_NB]{};
    int threatSafePawn[COLOR_NB]{};
    int threatPawnPush[COLOR_NB]{};
    int weakQueenProt[COLOR_NB]{};
    int passerCount[COLOR_NB]{};
    int passerMaxRank[COLOR_NB]{};
    int passerFree[COLOR_NB]{};
    int spaceBonus[COLOR_NB]{};
    int spaceWeight[COLOR_NB]{};
    // --- N: newly computed ---
    int connectedPassers[COLOR_NB]{}; // passer with a friendly passer alongside
    int candidatePassers[COLOR_NB]{}; // own file clear, adjacent enemy pawn stops it
    int rookOnSeventh[COLOR_NB]{};
    int batteries[COLOR_NB]{};        // slider aligned behind a friendly slider
    int hangingMaterial[COLOR_NB]{};  // value-weighted, in place of a SEE call
    int maxHangingVictim[COLOR_NB]{};
    int blockaderMinor[COLOR_NB]{};   // our minor sitting on an enemy passer's stop square
    int bothFlanks[COLOR_NB]{};       // pawns on both wings
    int infiltration[COLOR_NB]{};     // our non-pawns in the enemy half
};

struct Trace {
    TraceX x;
    Score section[TRACE_SECTION_NB][COLOR_NB] = {};
    Score psqt = {};
    int space[COLOR_NB] = {};
    bool bishopPair[COLOR_NB] = {};
    int phase = 0;
    int scale = 0;
    int rule50 = 0;
    Value whiteValue = VALUE_ZERO;
    Value finalValue = VALUE_ZERO;
    bool knownEndgame = false;
};

// Score the position from the side to move's point of view, in centipawns.
Value evaluate(const Position& pos);
Value evaluate_trace(const Position& pos, Trace& trace);

// Collapse a (middlegame, endgame) pair into one number for the given phase.
Value tapered(Score score, int phase);

}  // namespace Eval
}  // namespace boring

#endif  // BORING_EVAL_H
