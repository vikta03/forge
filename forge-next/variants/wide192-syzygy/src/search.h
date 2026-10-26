// search.h — iterative deepening and the alpha-beta search.

#ifndef BORING_SEARCH_H
#define BORING_SEARCH_H

#include <atomic>
#include <vector>

#include "history.h"
#include "position.h"
#include "timeman.h"
#include "types.h"

namespace boring {

namespace Search {

// One entry per ply of the search, giving each node access to what happened
// above it — the moves played to get here, the static evaluations along the
// way, and the killers for this depth.
struct Stack {
    Move*                 pv            = nullptr;
    const PieceToHistory* continuationHistory = nullptr;
    int                   ply           = 0;
    Move                  currentMove   = MOVE_NONE;
    Piece                 movedPiece    = NO_PIECE;
    Move                  excludedMove  = MOVE_NONE;
    Move                  killers[2]    = {MOVE_NONE, MOVE_NONE};
    Value                 staticEval    = VALUE_NONE;
    int                   moveCount     = 0;
    bool                  inCheck       = false;
    bool                  ttPv          = false;
    int                   doubleExtensions = 0;
};

// A root move and everything known about it. The PV is stored per root move so
// that MultiPV output and the "best move changed" heuristics have somewhere to
// look.
struct RootMove {
    explicit RootMove(Move m) : pv(1, m) {}

    bool operator==(Move m) const { return pv[0] == m; }
    // Sorted descending by score, so the best move ends up first.
    bool operator<(const RootMove& m) const {
        return m.score != score ? m.score < score : m.previousScore < previousScore;
    }

    Value             score         = -VALUE_INFINITE;
    Value             previousScore = -VALUE_INFINITE;
    Value             uciScore      = -VALUE_INFINITE;
    int               selDepth      = 0;
    uint64_t          effortNodes   = 0;
    std::vector<Move> pv;
};

// Set once at start-up.
void init();

// Wipe the transposition table and every history. Called on `ucinewgame`.
void clear();

// Launch a search on a worker thread and return immediately, so that the UCI
// loop stays responsive and `stop` can be honoured. Prints `bestmove` itself
// when it finishes.
void start(Position& pos, const LimitsType& limits);

// Ask the running search to stop, then wait for the thread to finish.
void stop();

// Wait for any running search without asking it to stop (for `quit`, and to
// serialise a new `go` behind an old one).
void wait();

// The move we were pondering on was played: stop treating this as a ponder
// search so that the time manager is allowed to end it.
void ponderhit();

bool running();

#ifdef FORGE_CACHE_AUDIT
extern uint64_t EvalCacheAudit[2][2];
#endif

// Total nodes visited by the most recent search.
uint64_t nodes_searched();

// Configuration, set through `setoption`.
// Base half-width of the root aspiration window. Boring's 12 was tuned
// against Boring's own eval volatility; a learned readout moves by a
// different amount per iteration, so it needs to be settable.
// Boring's forward-pruning margins are literals in Boring's centipawn
// scale. A learned WDL readout is far narrower, which leaves razoring,
// null move and qsearch delta pruning unreachable rather than merely
// conservative. PruneScale moves the whole family; the per-family
// percentages exist so a win can be attributed to one of them.
extern int  PruneScale;
extern int  RfpScale;
extern int  RazorScale;
extern int  NmpScale;
extern int  ProbCutScale;
extern int  FutScale;
extern int  QDeltaScale;
extern int  Rule50TTThreshold;
enum CorrHistModeType { CORR_LEGACY, CORR_PIECE, CORR_FULL };
extern int  CorrHistScale;
extern CorrHistModeType CorrHistMode;
extern bool CorrHistTelemetry;
extern bool SpecialMoveSEE;
extern int  ProbCutSeeScale;
extern int  AspDelta;
extern int  MoveOverhead;
extern int  MultiPV;
extern std::atomic<bool> Ponder;
extern bool AdaptiveTime;
extern bool TimeManagerTelemetry;

}  // namespace Search
}  // namespace boring

#endif  // BORING_SEARCH_H
