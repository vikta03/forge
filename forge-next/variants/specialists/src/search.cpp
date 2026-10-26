
#ifdef FORGE_NEXT_PROBE
#include "forge_next_probe.h"
#endif
// search.cpp — iterative deepening, principal variation search, quiescence.
//
// The search is fail-soft: it returns the best value it actually proved rather
// than clamping to the window. That gives the transposition table sharper
// bounds and makes aspiration re-searches converge faster.

#include "search.h"

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#include "eval.h"
#include "eval_terms.h"
#include "movegen.h"
#include "movepick.h"
#include "tt.h"
#include "uci.h"

namespace boring {

namespace Search {
// At and above this clock, TT identity includes the exact halfmove count.
// 101 disables clock qualification and reproduces the old behaviour.
int  Rule50TTThreshold = 80;
int  CorrHistScale     = 25;
CorrHistModeType CorrHistMode = CORR_LEGACY;
bool CorrHistTelemetry = false;
bool SpecialMoveSEE    = false;
// Converts a FORGE score gap back to Boring's raw material/SEE units.
// 1 reproduces the old dimensional mismatch; 4 matches PruneScale=25.
int  ProbCutSeeScale   = 4;
int  AspDelta          = 6;
// 25, not 100. Boring's forward-pruning margins are literals in Boring's
// centipawn scale and FORGE's readout is ~3.5x narrower, which left null
// move firing on 0.43% of interior nodes and razoring on 0.18%. Measured
// +78.8 +/- 31.8 Elo over 260 games at 8+0.08; 30 and 20 measured +68.0 and
// +52.5, all overlapping, so this sits on a plateau rather than a peak.
int  PruneScale   = 25;
int  RfpScale      = 100;
int  RazorScale    = 100;
int  NmpScale      = 100;
int  ProbCutScale  = 100;
int  FutScale      = 100;
int  QDeltaScale   = 100;
int  MoveOverhead = 10;
int  MultiPV      = 1;
std::atomic<bool> Ponder{false};
bool AdaptiveTime         = false;
bool TimeManagerTelemetry = false;
#ifdef FORGE_CACHE_AUDIT
uint64_t EvalCacheAudit[2][2]{};  // [main/quiescence][reuse/recompute]
#endif
}  // namespace Search

using namespace Search;

namespace {

enum NodeType { NonPV, PV, Root };

// --------------------------------------------------------------- tuning

// Late move reductions, indexed by depth and move number. Both axes are
// logarithmic: the difference between the 1st and 4th move matters far more
// than the difference between the 40th and 43rd.
int LMR[64][64];

int reduction(bool improving, int depth, int moveCount, bool pvNode) {
    int r = LMR[std::min(depth, 63)][std::min(moveCount, 63)];

    r -= pvNode;     // the principal variation deserves a careful look
    r -= improving;  // things are going well; do not cut the search short

    return std::max(r, 0);
}

// Every eval-scale pruning margin below goes through pm(). SEE thresholds are
// deliberately excluded: those are raw material units, and mixing the two
// scales is exactly the trap this whole exercise is about.
int pm(int centipawns, int family) {
    return int(static_cast<long long>(centipawns) * PruneScale * family / 10000);
}

// Position::key() retains the trained EP representation; repetition has its
// own legal-EP identity. FORGE's clock-sensitive search-cache identity gains
// an exact halfmove tag near the fifty-move horizon.
Key tt_key(const Position& pos) {
    const int clock = pos.rule50_count();
    if (clock < Rule50TTThreshold)
        return pos.key();

    std::uint64_t tag = std::uint64_t(clock) + 0x9e3779b97f4a7c15ULL;
    tag = (tag ^ (tag >> 30)) * 0xbf58476d1ce4e5b9ULL;
    tag = (tag ^ (tag >> 27)) * 0x94d049bb133111ebULL;
    return pos.key() ^ (tag ^ (tag >> 31));
}

template<bool Quiescence>
Value cached_static_eval(const Position& pos, TTEntry* entry) {
    Value value = entry->eval();
    const auto clock = TT.eval_rule50(entry);
    const bool refresh = value == VALUE_NONE || clock == 255 || clock != pos.rule50_count();
#ifdef FORGE_CACHE_AUDIT
    ++EvalCacheAudit[Quiescence][refresh];
#endif
    if (refresh) {
        value = Eval::evaluate(pos);
        TT.store_eval(entry, value, pos.rule50_count());
    }
    return value;
}

// How far above beta the static eval must be before we trust it enough to skip
// the search entirely.
Value futility_margin(int depth, bool improving) {
    return Value(pm(140, RfpScale) * (depth - improving));
}

// After this many moves at this depth, the rest are unlikely to be worth
// searching at all.
int futility_move_count(bool improving, int depth) {
    return improving ? 3 + depth * depth : (3 + depth * depth) / 2;
}

// --------------------------------------------------------- search state

// All the mutable state of one search. Single-threaded, so there is exactly one.
constexpr int CORR_HISTORY_SIZE = 1 << 14;
constexpr int CORR_HISTORY_MASK = CORR_HISTORY_SIZE - 1;
constexpr int CORR_HISTORY_LIMIT = 512;

struct CorrectionEntry {
    std::int16_t value = 0;

    void update(int target, int depth) {
        target = std::clamp(target, -CORR_HISTORY_LIMIT, CORR_HISTORY_LIMIT);
        // Direct exponential tracking in score units. A depth-2 bound
        // moves 12.5% towards its target; depth-12 moves 43.75%.
        const int weight = std::clamp(8 + 4 * depth, 12, 64);
        const int delta = (target - int(value)) * weight;
        const int rounded = delta >= 0 ? (delta + 64) / 128
                                       : (delta - 64) / 128;
        value = std::int16_t(std::clamp(int(value) + rounded,
                                        -CORR_HISTORY_LIMIT, CORR_HISTORY_LIMIT));
    }
};

struct Worker {
    ButterflyHistory    mainHistory;
    CaptureHistory      captureHistory;
    ContinuationHistory continuationHistory;
    CounterMoveHistory  counterMoves;
    CorrectionEntry pawnCorrection[COLOR_NB][CORR_HISTORY_SIZE]{};
    CorrectionEntry legacyNonPawnCorrection[COLOR_NB][CORR_HISTORY_SIZE]{};
    CorrectionEntry minorCorrection[COLOR_NB][CORR_HISTORY_SIZE]{};
    CorrectionEntry nonPawnCorrection[COLOR_NB][COLOR_NB][CORR_HISTORY_SIZE]{};
    CorrectionEntry continuationCorrection[COLOR_NB][2][CORR_HISTORY_SIZE]{};
    static constexpr int CORR_FAMILIES = 7;
    std::uint64_t corrProbes[CORR_FAMILIES]{};
    std::uint64_t corrAbs[CORR_FAMILIES]{};
    std::uint64_t corrUpdates[CORR_FAMILIES]{};
    std::uint64_t corrTargetsClamped[CORR_FAMILIES]{};

    // The plane used for a null move and for the sentinel entries below the
    // bottom of the stack, so that continuation history lookups never have to
    // test for null.
    PieceToHistory sentinelContinuation;

    std::vector<RootMove> rootMoves;

    uint64_t nodes          = 0;
    int      rootDepth      = 0;
    int      completedDepth = 0;
    int      selDepth       = 0;
    int      pvIdx          = 0;
    int      nmpMinPly      = 0;

    void clear() {
        mainHistory.clear();
        captureHistory.clear();
        continuationHistory.clear();
        counterMoves.clear();
        std::memset(pawnCorrection, 0, sizeof(pawnCorrection));
        std::memset(legacyNonPawnCorrection, 0, sizeof(legacyNonPawnCorrection));
        std::memset(minorCorrection, 0, sizeof(minorCorrection));
        std::memset(nonPawnCorrection, 0, sizeof(nonPawnCorrection));
        std::memset(continuationCorrection, 0, sizeof(continuationCorrection));
        sentinelContinuation = PieceToHistory{};
        nodes = rootDepth = completedDepth = selDepth = pvIdx = nmpMinPly = 0;
        reset_correction_telemetry();
    }

    void reset_correction_telemetry() {
        std::memset(corrProbes, 0, sizeof(corrProbes));
        std::memset(corrAbs, 0, sizeof(corrAbs));
        std::memset(corrUpdates, 0, sizeof(corrUpdates));
        std::memset(corrTargetsClamped, 0, sizeof(corrTargetsClamped));
    }
};

Worker      worker;
TimeManager Time;
LimitsType  Limits;

std::atomic<bool> Stopped{false};
std::atomic<bool> Running{false};
std::atomic<bool> Pondering{false};
std::atomic<bool> StopOnPonderHit{false};
std::thread       SearchThread;
std::mutex        OutputMutex;
std::condition_variable OutputReady;
bool ExplicitStop = false;  // protected by OutputMutex, unlike a node/time limit

void wait_for_output_permission() {
    std::unique_lock<std::mutex> lock(OutputMutex);
    OutputReady.wait(lock, [] {
        return ExplicitStop || (!Pondering.load(std::memory_order_relaxed) && !Limits.infinite);
    });
}

struct TimeTelemetry {
    double stability = 1.0;
    double scoreTrend = 1.0;
    double rootEffort = 1.0;
    TimePoint target = 0;
    TimePoint lastIteration = 0;
} LastTimeTelemetry;

std::size_t correction_index(Key key) {
    return std::size_t((key ^ (key >> 32)) & CORR_HISTORY_MASK);
}

enum CorrFamily {
    CORR_PAWN_FAMILY,
    CORR_LEGACY_NONPAWN_FAMILY,
    CORR_MINOR_FAMILY,
    CORR_WHITE_NONPAWN_FAMILY,
    CORR_BLACK_NONPAWN_FAMILY,
    CORR_CONT2_FAMILY,
    CORR_CONT4_FAMILY
};

constexpr const char* CORR_FAMILY_NAME[Worker::CORR_FAMILIES] = {
    "pawn", "legacy_nonpawn", "minor", "white_nonpawn", "black_nonpawn", "cont2", "cont4"
};

int probe_correction(CorrectionEntry& entry, CorrFamily family) {
    if (CorrHistTelemetry) {
        ++worker.corrProbes[family];
        worker.corrAbs[family] += std::uint64_t(std::abs(int(entry.value)));
    }
    return entry.value;
}

void train_correction(CorrectionEntry& entry, CorrFamily family, int target, int depth) {
    if (CorrHistTelemetry)
        worker.corrTargetsClamped[family] += std::abs(target) > CORR_HISTORY_LIMIT;
    entry.update(target, depth);
    if (CorrHistTelemetry)
        ++worker.corrUpdates[family];
}

bool continuation_index(const Position& pos, const Stack* ss, int plies, std::size_t& index) {
    const Stack* previous = ss - plies;
    if (previous->currentMove == MOVE_NONE || previous->currentMove == MOVE_NULL
        || previous->movedPiece == NO_PIECE)
        return false;

    Key signature = pos.key();
    signature ^= Key(int(previous->movedPiece) * 64 + int(to_sq(previous->currentMove)))
               * 0x9e3779b97f4a7c15ULL;
    signature ^= Key(plies) * 0xbf58476d1ce4e5b9ULL;
    index = correction_index(signature);
    return true;
}

Value corrected_static_eval(const Position& pos, const Stack* ss, Value raw) {
    if (CorrHistScale == 0 || raw == VALUE_NONE || is_decisive(raw))
        return raw;

    const Color us = pos.side_to_move();
    const Key pawns = pos.pawn_key();
    int sum = probe_correction(
        worker.pawnCorrection[us][correction_index(pawns)], CORR_PAWN_FAMILY);
    int count = 1;

    if (CorrHistMode == CORR_LEGACY) {
        sum += probe_correction(
            worker.legacyNonPawnCorrection[us][correction_index(pos.key() ^ pawns)],
            CORR_LEGACY_NONPAWN_FAMILY);
        ++count;
    } else {
        sum += probe_correction(
            worker.minorCorrection[us][correction_index(pos.minor_piece_key())],
            CORR_MINOR_FAMILY);
        sum += probe_correction(
            worker.nonPawnCorrection[us][WHITE][correction_index(pos.non_pawn_key(WHITE))],
            CORR_WHITE_NONPAWN_FAMILY);
        sum += probe_correction(
            worker.nonPawnCorrection[us][BLACK][correction_index(pos.non_pawn_key(BLACK))],
            CORR_BLACK_NONPAWN_FAMILY);
        count += 3;

        if (CorrHistMode == CORR_FULL) {
            std::size_t index;
            if (continuation_index(pos, ss, 2, index)) {
                sum += probe_correction(worker.continuationCorrection[us][0][index],
                                        CORR_CONT2_FAMILY);
                ++count;
            }
            if (continuation_index(pos, ss, 4, index)) {
                sum += probe_correction(worker.continuationCorrection[us][1][index],
                                        CORR_CONT4_FAMILY);
                ++count;
            }
        }
    }

    const int correction = (sum / count) * CorrHistScale / 100;
    return Value(std::clamp(int(raw) + correction,
                            int(VALUE_MATED_IN_MAX_PLY + 1),
                            int(VALUE_MATE_IN_MAX_PLY - 1)));
}

void update_correction_history(const Position& pos, const Stack* ss, Value raw,
                               Value searched, int depth) {
    if (CorrHistScale == 0 || raw == VALUE_NONE || is_decisive(raw)
        || is_decisive(searched))
        return;

    const int target = int(searched) - int(raw);
    const Color us = pos.side_to_move();
    const Key pawns = pos.pawn_key();
    train_correction(worker.pawnCorrection[us][correction_index(pawns)],
                     CORR_PAWN_FAMILY, target, depth);

    if (CorrHistMode == CORR_LEGACY) {
        train_correction(
            worker.legacyNonPawnCorrection[us][correction_index(pos.key() ^ pawns)],
            CORR_LEGACY_NONPAWN_FAMILY, target, depth);
        return;
    }

    train_correction(worker.minorCorrection[us][correction_index(pos.minor_piece_key())],
                     CORR_MINOR_FAMILY, target, depth);
    train_correction(
        worker.nonPawnCorrection[us][WHITE][correction_index(pos.non_pawn_key(WHITE))],
        CORR_WHITE_NONPAWN_FAMILY, target, depth);
    train_correction(
        worker.nonPawnCorrection[us][BLACK][correction_index(pos.non_pawn_key(BLACK))],
        CORR_BLACK_NONPAWN_FAMILY, target, depth);

    if (CorrHistMode == CORR_FULL) {
        std::size_t index;
        if (continuation_index(pos, ss, 2, index))
            train_correction(worker.continuationCorrection[us][0][index],
                             CORR_CONT2_FAMILY, target, depth);
        if (continuation_index(pos, ss, 4, index))
            train_correction(worker.continuationCorrection[us][1][index],
                             CORR_CONT4_FAMILY, target, depth);
    }
}

template<typename Table>
std::uint64_t occupied_correction_entries(const Table& table) {
    const std::int16_t* first = reinterpret_cast<const std::int16_t*>(&table);
    const std::int16_t* last = first + sizeof(table) / sizeof(std::int16_t);
    return std::count_if(first, last, [](std::int16_t value) { return value != 0; });
}

void print_correction_telemetry() {
    const std::uint64_t occupied[Worker::CORR_FAMILIES] = {
        occupied_correction_entries(worker.pawnCorrection),
        occupied_correction_entries(worker.legacyNonPawnCorrection),
        occupied_correction_entries(worker.minorCorrection),
        occupied_correction_entries(worker.nonPawnCorrection[0][WHITE])
            + occupied_correction_entries(worker.nonPawnCorrection[1][WHITE]),
        occupied_correction_entries(worker.nonPawnCorrection[0][BLACK])
            + occupied_correction_entries(worker.nonPawnCorrection[1][BLACK]),
        occupied_correction_entries(worker.continuationCorrection[0][0])
            + occupied_correction_entries(worker.continuationCorrection[1][0]),
        occupied_correction_entries(worker.continuationCorrection[0][1])
            + occupied_correction_entries(worker.continuationCorrection[1][1])
    };

    for (int family = 0; family < Worker::CORR_FAMILIES; ++family) {
        const std::uint64_t meanAbs = worker.corrProbes[family]
            ? worker.corrAbs[family] / worker.corrProbes[family] : 0;
        std::cout << "info string corrhist family " << CORR_FAMILY_NAME[family]
                  << " probes " << worker.corrProbes[family]
                  << " updates " << worker.corrUpdates[family]
                  << " mean_abs " << meanAbs
                  << " target_clamps " << worker.corrTargetsClamped[family]
                  << " occupied " << occupied[family] << std::endl;
    }
}

// ------------------------------------------------------ mate score fixup

// A mate score is "mate in N plies counting from the root". The transposition
// table is shared between positions at different distances from the root, so
// the score has to be rebased to be relative to the position itself on the way
// in, and back to the root on the way out.
//
// Getting this wrong is invisible in ordinary play and produces nonsense mate
// announcements and unstable mate lines. It was the most consequential bug in
// this engine's predecessor.
Value value_to_tt(Value v, int ply) {
    assert(v != VALUE_NONE);
    return is_win(v) ? Value(v + ply) : is_loss(v) ? Value(v - ply) : v;
}

Value value_from_tt(Value v, int ply, int rule50) {
    if (v == VALUE_NONE)
        return VALUE_NONE;

    if (is_win(v)) {
        // A mate that the fifty-move rule would arrive first to prevent is not
        // really a mate; downgrade it rather than claiming a win we cannot force.
        if (VALUE_MATE - v > 99 - rule50)
            return VALUE_MATE_IN_MAX_PLY - 1;
        return Value(v - ply);
    }

    if (is_loss(v)) {
        if (VALUE_MATE + v > 99 - rule50)
            return VALUE_MATED_IN_MAX_PLY + 1;
        return Value(v + ply);
    }

    return v;
}

// ------------------------------------------------------------- helpers

void update_pv(Move* pv, Move move, const Move* childPv) {
    *pv++ = move;

    if (childPv)
        while (*childPv != MOVE_NONE)
            *pv++ = *childPv++;

    *pv = MOVE_NONE;
}

void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {
    // One, two, four and six plies back. Two-ply and four-ply continuations
    // capture "this piece manoeuvre works"; the six-ply one is weak but cheap.
    for (int i : {1, 2, 4, 6}) {
        if (ss->inCheck && i > 2)
            break;

        if ((ss - i)->currentMove != MOVE_NONE && (ss - i)->currentMove != MOVE_NULL)
            (*const_cast<PieceToHistory*>((ss - i)->continuationHistory))(pc, to)
                .update(bonus / (1 + (i > 2)));
    }
}

void update_quiet_stats(const Position& pos, Stack* ss, Move move, int bonus) {
    // Killers: the two quiet moves that most recently caused a cutoff at this
    // depth. Tried early at sibling nodes, where they very often cut again.
    if (ss->killers[0] != move) {
        ss->killers[1] = ss->killers[0];
        ss->killers[0] = move;
    }

    const Color us = pos.side_to_move();
    worker.mainHistory(us, move).update(bonus);
    update_continuation_histories(ss, pos.moved_piece(move), to_sq(move), bonus);

    // Countermove: the reply that refuted the opponent's last move.
    if ((ss - 1)->currentMove != MOVE_NONE && (ss - 1)->currentMove != MOVE_NULL) {
        const Square prevSq = to_sq((ss - 1)->currentMove);
        worker.counterMoves(pos.piece_on(prevSq), prevSq) = move;
    }
}

// Reward the move that caused the cutoff, and penalise the ones that were
// searched first and did not. Applying only bonuses would inflate every counter
// towards its limit and destroy the ordering signal.
void update_all_stats(const Position& pos,
                      Stack*          ss,
                      Move            bestMove,
                      Move*           quietsSearched,
                      int             quietCount,
                      Move*           capturesSearched,
                      int             captureCount,
                      int             depth) {
    const Color us    = pos.side_to_move();
    const int   bonus = history_bonus(depth);
    const int   malus = history_malus(depth);

    if (!pos.capture_stage(bestMove)) {
        update_quiet_stats(pos, ss, bestMove, bonus);

        for (int i = 0; i < quietCount; ++i) {
            worker.mainHistory(us, quietsSearched[i]).update(-malus);
            update_continuation_histories(ss, pos.moved_piece(quietsSearched[i]),
                                          to_sq(quietsSearched[i]), -malus);
        }
    } else {
        const Square    to       = to_sq(bestMove);
        const PieceType captured = type_of(bestMove) == EN_PASSANT && SpecialMoveSEE
            ? PAWN : type_of(pos.piece_on(to));
        worker.captureHistory(pos.moved_piece(bestMove), to, captured).update(bonus);
    }

    for (int i = 0; i < captureCount; ++i) {
        const Square    to       = to_sq(capturesSearched[i]);
        const PieceType captured = type_of(capturesSearched[i]) == EN_PASSANT
                                      && SpecialMoveSEE
            ? PAWN : type_of(pos.piece_on(to));
        worker.captureHistory(pos.moved_piece(capturesSearched[i]), to, captured).update(-malus);
    }
}

void check_time() {
    if (Limits.nodes && worker.nodes >= Limits.nodes) {
        Stopped.store(true, std::memory_order_relaxed);
        return;
    }

    if (Time.maximum() && Time.elapsed() >= Time.maximum()) {
        if (Pondering.load(std::memory_order_relaxed))
            StopOnPonderHit.store(true, std::memory_order_relaxed);
        else
            Stopped.store(true, std::memory_order_relaxed);
    }
}

std::string pv_line(const RootMove& rm, int depth, TimePoint elapsed, int multiPvIdx) {
    std::ostringstream ss;

    ss << "info depth " << depth << " seldepth " << rm.selDepth;

    if (Search::MultiPV > 1)
        ss << " multipv " << multiPvIdx + 1;

    ss << " score " << UCI::value(rm.uciScore) << " nodes " << worker.nodes << " nps "
       << (worker.nodes * 1000 / std::max<TimePoint>(elapsed, 1)) << " hashfull " << TT.hashfull()
       << " time " << elapsed << " pv";

    for (Move m : rm.pv)
        ss << ' ' << UCI::move(m);

    return ss.str();
}

// --------------------------------------------------------------- search

template<NodeType nodeType>
Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, int depth);

template<NodeType nodeType>
Value search(Position& pos, Stack* ss, Value alpha, Value beta, int depth, bool cutNode) {
    constexpr bool PvNode   = (nodeType != NonPV);
    constexpr bool rootNode = (nodeType == Root);

    // Dropping out of the search proper: everything left is captures.
    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta, 0);

    // Extensions at the depth ceiling must not overflow the TT depth byte or
    // violate the search's depth contract. Stack distance is capped separately.
    depth = std::min(depth, MAX_PLY - 1);

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(depth > 0 && depth < MAX_PLY);

    const Value alphaOriginal = alpha;
    const Value betaOriginal = beta;
    Move      pv[MAX_PLY + 1];
    StateInfo st;

    Move capturesSearched[32];
    Move quietsSearched[64];

    Key      posKey;
    TTEntry* tte      = nullptr;
    bool     ttHit    = false;
    Move     ttMove   = MOVE_NONE;
    Value    ttValue  = VALUE_NONE;
    Move     move     = MOVE_NONE;
    Move     bestMove = MOVE_NONE;
    Value    value    = -VALUE_INFINITE;
    Value    eval     = VALUE_NONE;
    Value    rawStaticEval = VALUE_NONE;
    Value    bestValue = -VALUE_INFINITE;

    int  moveCount = 0, captureCount = 0, quietCount = 0;
    int  extension = 0, newDepth = 0;
    bool givesCheck = false, improving = false, capture = false;
    bool moveCountPruning = false;

    const Color us           = pos.side_to_move();
    const Move  excludedMove = ss->excludedMove;

    ss->inCheck   = bool(pos.checkers());
    ss->moveCount = 0;

    if (!rootNode) {
        if (Stopped.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        // Repetition, fifty-move, and running out of stack. Note this is asked
        // here and not inside evaluate(): a history scan at every leaf is a
        // measurable waste, and the answer is a property of the search path
        // rather than of the position.
        if (pos.is_draw(ss->ply) || (!ss->inCheck && !has_legal_move(pos)))
            return VALUE_DRAW;
        if (ss->ply >= MAX_PLY)
            return !ss->inCheck ? Eval::evaluate(pos) : VALUE_DRAW;

        // Mate distance pruning: if we already have a mate as fast as anything
        // this subtree could produce, there is nothing here worth finding.
        alpha = std::max(mated_in(ss->ply), alpha);
        beta  = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta)
            return alpha;
    }

    (ss + 1)->excludedMove = MOVE_NONE;
    (ss + 2)->killers[0] = (ss + 2)->killers[1] = MOVE_NONE;
    ss->doubleExtensions                        = (ss - 1)->doubleExtensions;

    if (PvNode && worker.selDepth < ss->ply + 1)
        worker.selDepth = ss->ply + 1;

    // --- transposition table ---
    posKey  = tt_key(pos);
    tte     = TT.probe(posKey, ttHit);
    const TTEntry ttData = *tte;
    ttValue = ttHit ? value_from_tt(ttData.value(), ss->ply, pos.rule50_count()) : VALUE_NONE;
    ttMove  = rootNode ? worker.rootMoves[worker.pvIdx].pv[0] : ttHit ? ttData.move() : MOVE_NONE;

    if (!excludedMove)
        ss->ttPv = PvNode || (ttHit && ttData.is_pv());

    // A stored result deep enough to answer this window ends the node. Not in
    // PV nodes: there we want the whole line, not just a bound.
    if (!PvNode && !excludedMove && ttHit && pos.rule50_count() < 90
        && ttData.depth() > depth - (ttValue <= beta) && ttValue != VALUE_NONE
        && (ttData.bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER))) {

        // Even a node we did not search should teach the move ordering.
        if (ttMove != MOVE_NONE && ttValue >= beta && !pos.capture_stage(ttMove))
            update_quiet_stats(pos, ss, ttMove, history_bonus(depth));

        return ttValue;
    }

    // --- static evaluation ---
    if (ss->inCheck) {
        // No useful static evaluation exists when the king is attacked.
        ss->staticEval = eval = VALUE_NONE;
        improving             = false;
        goto moves_loop;
    }

    if (excludedMove)
        eval = ss->staticEval;
    else if (ttHit) {
        rawStaticEval = cached_static_eval<false>(pos, tte);
        eval = ss->staticEval = corrected_static_eval(pos, ss, rawStaticEval);

        // The stored search value is a better estimate than the static eval,
        // where its bound permits.
        if (pos.rule50_count() < 90 && ttValue != VALUE_NONE
            && (ttData.bound() & (ttValue > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttValue;
    } else {
        rawStaticEval = Eval::evaluate(pos);
        eval = ss->staticEval = corrected_static_eval(pos, ss, rawStaticEval);
        TT.save(tte, posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, DEPTH_NONE, MOVE_NONE,
                rawStaticEval, TT.generation(), pos.rule50_count());
    }

    // Is our position better than it was two plies ago? Several pruning rules
    // are safe when it is and risky when it is not.
    improving = (ss - 2)->staticEval == VALUE_NONE
                  ? ((ss - 4)->staticEval == VALUE_NONE || ss->staticEval > (ss - 4)->staticEval)
                  : ss->staticEval > (ss - 2)->staticEval;

    // Razoring: so far below alpha that only a tactic can save this node, and
    // quiescence will find one if it exists.
    if (!PvNode && eval < alpha - pm(450, RazorScale) - pm(250, RazorScale) * depth * depth) {
        value = qsearch<NonPV>(pos, ss, alpha - 1, alpha, 0);
        if (Stopped.load(std::memory_order_relaxed))
            return VALUE_ZERO;
        if (value < alpha)
            return value;
    }

    // Reverse futility: so far above beta that even conceding material would
    // leave us winning. The mirror image of futility pruning.
    if (!ss->ttPv && depth < 9 && eval >= beta && !is_decisive(eval) && !is_loss(beta)
        && eval - futility_margin(depth, improving) >= beta)
        return eval;

    // Null move pruning: give the opponent a free move. If our position is
    // still good enough to fail high, the real move will be at least as good.
    // Skipped without non-pawn material, where zugzwang makes the assumption
    // false.
    if (!PvNode && !excludedMove && (ss - 1)->currentMove != MOVE_NULL && eval >= beta
        && ss->staticEval >= beta - pm(20, NmpScale) * depth + pm(300, NmpScale) && pos.non_pawn_material(us)
        && ss->ply >= worker.nmpMinPly && !is_loss(beta)) {

        const int R = std::min(int(eval - beta) / std::max(pm(200, NmpScale), 1), 6) + depth / 3 + 4;

        ss->currentMove         = MOVE_NULL;
        ss->movedPiece          = NO_PIECE;
        ss->continuationHistory = &worker.sentinelContinuation;

        pos.do_null_move(st);
        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, !cutNode);
        pos.undo_null_move();
        if (Stopped.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        if (nullValue >= beta) {
            // Do not return an unproven mate found through a null move.
            if (is_win(nullValue))
                nullValue = beta;

            if (worker.nmpMinPly || depth < 14)
                return nullValue;

            // At high depth, confirm with a real search that has null move
            // disabled, so a zugzwang position cannot slip through.
            worker.nmpMinPly = ss->ply + 3 * (depth - R) / 4;
            const Value v    = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);
            worker.nmpMinPly = 0;
            if (Stopped.load(std::memory_order_relaxed))
                return VALUE_ZERO;

            if (v >= beta)
                return nullValue;
        }
    }

    // ProbCut. If some capture beats beta by a comfortable margin even at
    // reduced depth, the full-depth search would almost certainly fail high
    // too — so prove it cheaply and leave.
    {
        const Value probCutBeta =
            Value(beta + pm(180, ProbCutScale) - pm(60, ProbCutScale) * improving);

        if (!PvNode && depth > 3 && !is_decisive(beta)
            // Nothing to gain if the table already says this node cannot reach
            // that high.
            && !(ttHit && ttData.depth() >= depth - 3 && ttValue != VALUE_NONE
                 && ttValue < probCutBeta)) {

            const int seeThreshold = ProbCutSeeScale
                               * int(probCutBeta - ss->staticEval);
            MovePicker mp(pos, ttMove, seeThreshold, &worker.captureHistory);

            Move pcMove;
            while ((pcMove = mp.next_move()) != MOVE_NONE) {
                if (pcMove == excludedMove || !pos.legal(pcMove))
                    continue;

                ss->currentMove = pcMove;
                ss->movedPiece = pos.moved_piece(pcMove);
                ss->continuationHistory =
                    &worker.continuationHistory(ss->movedPiece, to_sq(pcMove));

                pos.do_move(pcMove, st);
                ++worker.nodes;
                if ((worker.nodes & 1023) == 0)
                    check_time();

                // Screen with quiescence first — far cheaper than the real
                // search, and it rejects most candidates.
                Value v = -qsearch<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, 0);

                if (v >= probCutBeta)
                    v = -search<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, depth - 4,
                                       !cutNode);

                pos.undo_move(pcMove);
                if (Stopped.load(std::memory_order_relaxed))
                    return VALUE_ZERO;

                if (v >= probCutBeta) {
                    TT.save(tte, posKey, value_to_tt(v, ss->ply), ss->ttPv, BOUND_LOWER, depth - 3,
                            pcMove, rawStaticEval, TT.generation(), pos.rule50_count());
                    return v;
                }
            }
        }
    }

    // Internal iterative reduction. With no transposition move the ordering has
    // nothing to work from, so a full-depth search here mostly wastes effort;
    // reduce, and let the next visit — which will have a move — do it properly.
    if (PvNode && !ttMove && depth >= 3)
        depth -= 2;
    if (cutNode && !ttMove && depth >= 8)
        depth -= 2;

    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta, 0);

moves_loop:

    {
        const PieceToHistory* contHist[] = {
            (ss - 1)->continuationHistory, (ss - 2)->continuationHistory,
            (ss - 3)->continuationHistory, (ss - 4)->continuationHistory};

        Move countermove = MOVE_NONE;
        if ((ss - 1)->currentMove != MOVE_NONE && (ss - 1)->currentMove != MOVE_NULL) {
            const Square prevSq = to_sq((ss - 1)->currentMove);
            countermove         = worker.counterMoves(pos.piece_on(prevSq), prevSq);
        }

        MovePicker mp(pos, ttMove, depth, &worker.mainHistory, &worker.captureHistory, contHist,
                      ss->killers, countermove);

        value = bestValue;

        while ((move = mp.next_move(moveCountPruning)) != MOVE_NONE) {
            assert(is_ok(move));

            // The move being tested for singularity is searched by the caller.
            if (move == excludedMove)
                continue;

            if (rootNode
                && std::find(worker.rootMoves.begin() + worker.pvIdx, worker.rootMoves.end(), move)
                     == worker.rootMoves.end())
                continue;

            if (!rootNode && !pos.legal(move))
                continue;

            ss->moveCount = ++moveCount;

            givesCheck            = pos.gives_check(move);
            capture               = pos.capture_stage(move);
            const Piece  movedPiece = pos.moved_piece(move);
            const Square to         = to_sq(move);

            newDepth = depth - 1;

            int r = reduction(improving, depth, moveCount, PvNode);
            if (!capture) {
                const int hist =
                    (*contHist[0])(movedPiece, to) + (*contHist[1])(movedPiece, to);
                r = std::max(r - std::clamp(hist / 6000, -2, 2), 0);
            }
            const int lmrDepth = std::max(newDepth - r, 0);

            // --- pruning at shallow depth ---
            if (!rootNode && pos.non_pawn_material(us) && bestValue > VALUE_MATED_IN_MAX_PLY) {
                // Once enough moves have been tried without improving alpha,
                // stop generating quiets at all.
                moveCountPruning = moveCount >= futility_move_count(improving, depth);

                if (capture || givesCheck) {
                    // A capture that loses too much material is not worth a
                    // search, however tempting it looks.
                    if (!pos.see_ge(move, -200 * depth))
                        continue;
                } else {
                    const int history = (*contHist[0])(movedPiece, to) + (*contHist[1])(movedPiece, to);

                    // A quiet move that has been bad as a follow-up before is
                    // unlikely to be good now.
                    if (lmrDepth < 5 && history < -4000 * depth)
                        continue;

                    // Futility: the static eval plus a generous margin still
                    // does not reach alpha, and a quiet move will not change
                    // the material balance.
                    if (!ss->inCheck && lmrDepth < 12
                        && ss->staticEval + pm(120, FutScale) + pm(130, FutScale) * lmrDepth <= alpha)
                        continue;

                    // A quiet move that simply hangs a piece.
                    if (!pos.see_ge(move, -25 * lmrDepth * lmrDepth))
                        continue;
                }
            }

            // --- extensions ---
            extension = 0;
            // The budget belongs to this path, not to previously searched siblings.
            ss->doubleExtensions = (ss - 1)->doubleExtensions;

            if (ss->ply < worker.rootDepth * 2) {
                // Singular extension. If the transposition move is much better
                // than every alternative, the position hinges on it, and it is
                // worth an extra ply. This is verified by searching every
                // *other* move at reduced depth below the stored value.
                if (!rootNode && depth >= 5 && move == ttMove && !excludedMove && ttHit
                    && !is_decisive(ttValue) && (ttData.bound() & BOUND_LOWER)
                    && ttData.depth() >= depth - 3) {

                    const Value singularBeta  = Value(ttValue - 2 * depth);
                    const int   singularDepth = (depth - 1) / 2;

                    ss->excludedMove = move;
                    value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth,
                                          cutNode);
                    ss->excludedMove = MOVE_NONE;
                    if (Stopped.load(std::memory_order_relaxed))
                        return VALUE_ZERO;

                    if (value < singularBeta) {
                        extension = 1;

                        // Overwhelmingly singular: worth two plies, but only a
                        // limited number of times along one path.
                        if (!PvNode && value < singularBeta - 20 && ss->doubleExtensions <= 10) {
                            extension = 2;
                            ss->doubleExtensions++;
                        }
                    }
                    // Every move beat beta, so the whole node does.
                    else if (singularBeta >= beta)
                        return singularBeta;
                    // The transposition move already fails high on its own; the
                    // node is easy and can be shortened.
                    else if (ttValue >= beta)
                        extension = -2;
                    else if (cutNode)
                        extension = -1;
                }
                // A check is forcing, so the tree below it is narrow. Extending
                // is cheap — but only at depth, and never unconditionally: an
                // uncapped check extension can explode on a perpetual.
                else if (givesCheck && depth > 6)
                    extension = 1;
            }

            newDepth += extension;

            ss->currentMove         = move;
            ss->movedPiece          = movedPiece;
            ss->continuationHistory = &worker.continuationHistory(movedPiece, to);

            TT.prefetch(pos.key_after(move));
            const uint64_t nodesBeforeMove = AdaptiveTime ? worker.nodes : 0;
            pos.do_move(move, st, givesCheck);
            ++worker.nodes;

            if ((worker.nodes & 1023) == 0)
                check_time();

            // --- late move reductions ---
            //
            // Later moves are searched shallower on the assumption the ordering
            // is roughly right. If a reduced search unexpectedly beats alpha,
            // it is re-searched at full depth.
            if (depth >= 2 && moveCount > 1 + rootNode && (!capture || cutNode)) {
                const int d = std::clamp(newDepth - r, 1, newDepth + 1);

                value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, d, true);

                if (value > alpha && d < newDepth)
                    value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);
            }
            // The first move of a PV node gets a full window below; everything
            // else starts with a null window.
            else if (!PvNode || moveCount > 1)
                value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

            // Full-window search: the first move, or one that beat alpha.
            if (PvNode && (moveCount == 1 || value > alpha)) {
                (ss + 1)->pv    = pv;
                (ss + 1)->pv[0] = MOVE_NONE;

                value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
            }

            pos.undo_move(move);

            if (Stopped.load(std::memory_order_relaxed))
                return VALUE_ZERO;

            if (rootNode) {
                RootMove& rm =
                    *std::find(worker.rootMoves.begin(), worker.rootMoves.end(), move);
                if (AdaptiveTime)
                    rm.effortNodes += worker.nodes - nodesBeforeMove;

                if (moveCount == 1 || value > alpha) {
                    rm.score = rm.uciScore = value;
                    rm.selDepth            = worker.selDepth;

                    rm.pv.resize(1);
                    for (Move* m = (ss + 1)->pv; m && *m != MOVE_NONE; ++m)
                        rm.pv.push_back(*m);
                } else
                    // Not searched with a real window this iteration; keep it
                    // out of the way of the sort.
                    rm.score = -VALUE_INFINITE;
            }

            if (value > bestValue) {
                bestValue = value;

                if (value > alpha) {
                    bestMove = move;

                    if (PvNode && !rootNode)
                        update_pv(ss->pv, move, (ss + 1)->pv);

                    if (value >= beta)
                        break;  // fail high

                    alpha = value;
                }
            }

            // Remember what was tried but did not work, for the history malus.
            if (move != bestMove) {
                if (capture && captureCount < 32)
                    capturesSearched[captureCount++] = move;
                else if (!capture && quietCount < 64)
                    quietsSearched[quietCount++] = move;
            }
        }
    }

    // No legal move: checkmate or stalemate. Under a singular search it just
    // means every other move was excluded, which is not a terminal result.
    if (moveCount == 0)
        bestValue = excludedMove ? alpha : ss->inCheck ? mated_in(ss->ply) : VALUE_DRAW;
    else if (bestMove != MOVE_NONE)
        update_all_stats(pos, ss, bestMove, quietsSearched, quietCount, capturesSearched,
                         captureCount, depth);

    if (!excludedMove && !ss->inCheck && depth >= 2 && rawStaticEval != VALUE_NONE
        && (bestMove == MOVE_NONE || !pos.capture_stage(bestMove))) {
        const bool informative = bestValue >= betaOriginal
                                   ? bestValue > rawStaticEval
                                 : bestValue <= alphaOriginal
                                   ? bestValue < rawStaticEval
                                   : true;
        if (informative)
            update_correction_history(pos, ss, rawStaticEval, bestValue, depth);
    }

    if (!excludedMove && !(rootNode && worker.pvIdx))
        TT.save(tte, posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                  bestValue >= beta      ? BOUND_LOWER
                  : (PvNode && bestMove) ? BOUND_EXACT
                                         : BOUND_UPPER,
                  depth, bestMove, rawStaticEval, TT.generation(), pos.rule50_count());

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);
    return bestValue;
}

// Search only the forcing moves, so that the value returned by search() is that
// of a position where nothing is hanging. Without this, every evaluation at a
// leaf would be taken in the middle of an exchange.
template<NodeType nodeType>
Value qsearch(Position& pos, Stack* ss, Value alpha, Value beta, int depth) {
    constexpr bool PvNode = (nodeType == PV);

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(depth <= 0);
    if (Stopped.load(std::memory_order_relaxed))
        return VALUE_ZERO;

    Move      pv[MAX_PLY + 1];
    StateInfo st;

    if (PvNode) {
        (ss + 1)->pv = pv;
        ss->pv[0]    = MOVE_NONE;
    }

    Move  bestMove = MOVE_NONE;
    int   moveCount = 0;

    ss->inCheck = bool(pos.checkers());

    // A terminal draw precedes stand-pat, TT cutoffs and the stack fallback.
    // Stand-pat assumes some legal move exists; a stalemate has none.
    if (pos.is_draw(ss->ply) || (!ss->inCheck && !has_legal_move(pos)))
        return VALUE_DRAW;
    if (ss->ply >= MAX_PLY)
        return !ss->inCheck ? Eval::evaluate(pos) : VALUE_DRAW;

    if (PvNode && worker.selDepth < ss->ply + 1)
        worker.selDepth = ss->ply + 1;

    // Quiescence stores only two depths: nodes that include checks and nodes
    // that do not. Anything finer would just churn the table.
    const int ttDepth = (ss->inCheck || depth >= DEPTH_QS) ? DEPTH_QS : DEPTH_QS - 1;

    const Key posKey  = tt_key(pos);
    bool      ttHit   = false;
    TTEntry*  tte     = TT.probe(posKey, ttHit);
    const Value ttValue = ttHit ? value_from_tt(tte->value(), ss->ply, pos.rule50_count())
                                : VALUE_NONE;
    const Move  ttMove  = ttHit ? tte->move() : MOVE_NONE;
    const bool  pvHit   = ttHit && tte->is_pv();

    if (!PvNode && ttHit && pos.rule50_count() < 90 && tte->depth() >= ttDepth
        && ttValue != VALUE_NONE
        && (tte->bound() & (ttValue >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttValue;

    Value bestValue, futilityBase;
    Value rawStaticEval = VALUE_NONE;

    if (ss->inCheck) {
        // In check there is no stand-pat: every evasion must be examined, or we
        // would report a mate score for a position that has a way out.
        ss->staticEval = VALUE_NONE;
        bestValue = futilityBase = -VALUE_INFINITE;
    } else {
        if (ttHit) {
            rawStaticEval = cached_static_eval<true>(pos, tte);
            ss->staticEval = bestValue = corrected_static_eval(pos, ss, rawStaticEval);

            if (pos.rule50_count() < 90 && ttValue != VALUE_NONE
                && (tte->bound() & (ttValue > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttValue;
        } else {
            rawStaticEval = Eval::evaluate(pos);
            ss->staticEval = bestValue = corrected_static_eval(pos, ss, rawStaticEval);
        }

        // Stand pat: we are not obliged to capture, so the static evaluation is
        // a lower bound on what this position is worth.
        if (bestValue >= beta) {
            if (!ttHit)
                TT.save(tte, posKey, value_to_tt(bestValue, ss->ply), false, BOUND_LOWER, DEPTH_NONE,
                        MOVE_NONE, rawStaticEval, TT.generation(), pos.rule50_count());
            return bestValue;
        }

        if (bestValue > alpha)
            alpha = bestValue;

        futilityBase = bestValue + pm(200, QDeltaScale);
    }

    const PieceToHistory* contHist[] = {(ss - 1)->continuationHistory,
                                        (ss - 2)->continuationHistory};

    MovePicker mp(pos, ttMove, &worker.mainHistory, &worker.captureHistory, contHist);

    Move move;
    while ((move = mp.next_move()) != MOVE_NONE) {
        assert(is_ok(move));

        if (!pos.legal(move))
            continue;

        ++moveCount;

        const bool givesCheck = pos.gives_check(move);

        if (bestValue > VALUE_MATED_IN_MAX_PLY && !ss->inCheck
            && pos.non_pawn_material(pos.side_to_move())) {
            // Delta pruning: winning the piece on the target square would still
            // not reach alpha, so the capture cannot help.
            if (!givesCheck && futilityBase > -VALUE_INFINITE && type_of(move) != PROMOTION) {
                if (moveCount > 2)
                    continue;

                const Value futilityValue =
                    Value(futilityBase
                          + pm(Eval::SIMPLE_PIECE_VALUE[type_of(move) == EN_PASSANT
                              ? PAWN : type_of(pos.piece_on(to_sq(move)))],
                               QDeltaScale));

                if (futilityValue <= alpha) {
                    bestValue = std::max(bestValue, futilityValue);
                    continue;
                }

                // The capture would have to win material outright to matter,
                // and it does not.
                if (futilityBase <= alpha
                    && (type_of(move) != EN_PASSANT || SpecialMoveSEE)
                    && !pos.see_ge(move, 1)) {
                    bestValue = std::max(bestValue, futilityBase);
                    continue;
                }
            }

            // Losing captures are noise in quiescence; the search above will
            // consider a real sacrifice on its own terms.
            if (!pos.see_ge(move, -80))
                continue;
        }

        ss->currentMove         = move;
        ss->movedPiece          = pos.moved_piece(move);
        ss->continuationHistory = &worker.continuationHistory(ss->movedPiece, to_sq(move));

        pos.do_move(move, st, givesCheck);
        ++worker.nodes;

        if ((worker.nodes & 1023) == 0)
            check_time();

        const Value value = -qsearch<nodeType>(pos, ss + 1, -beta, -alpha, depth - 1);

        pos.undo_move(move);

        if (Stopped.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;

                if (PvNode)
                    update_pv(ss->pv, move, (ss + 1)->pv);

                if (value >= beta)
                    break;

                alpha = value;
            }
        }
    }

    // In check with nothing legal is mate. Note the evasion generator produced
    // every legal reply, so this is sound.
    if (ss->inCheck && bestValue == -VALUE_INFINITE)
        return mated_in(ss->ply);

    TT.save(tte, posKey, value_to_tt(bestValue, ss->ply), pvHit,
              bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, ttDepth, bestMove, rawStaticEval,
              TT.generation(), pos.rule50_count());

    return bestValue;
}

// ------------------------------------------------------ iterative deepening

void iterative_deepening(Position* pos) {
    if (worker.rootMoves.empty()) {
        std::cout << "info depth 0 score "
                  << UCI::value(pos->checkers() ? -VALUE_MATE : VALUE_DRAW) << std::endl;
        wait_for_output_permission();
        Stopped.store(true, std::memory_order_relaxed);
        std::cout << "bestmove (none)" << std::endl;
        Running.store(false, std::memory_order_relaxed);
        return;
    }

#ifdef FORGE_NEXT_PROBE
    ForgeNext::reset(*pos);
#endif
    Stack  stack[MAX_PLY + 12] = {};
    Stack* ss                  = stack + 8;  // room for the ss-1 .. ss-7 lookbacks
    Move   pv[MAX_PLY + 1];

    for (int i = 1; i <= 8; ++i) {
        (ss - i)->continuationHistory = &worker.sentinelContinuation;
        (ss - i)->staticEval          = VALUE_NONE;
        (ss - i)->currentMove         = MOVE_NONE;
        (ss - i)->movedPiece          = NO_PIECE;
    }
    for (int i = 0; i <= MAX_PLY + 2; ++i)
        (ss + i)->ply = i;

    ss->pv = pv;

    Value bestValue    = -VALUE_INFINITE;
    Value previousBest = VALUE_NONE;
    Move  lastBestMove = MOVE_NONE;
    int   lastBestMoveDepth = 0;
    Value scoreHistory[3] = {VALUE_NONE, VALUE_NONE, VALUE_NONE};
    int   scoreHistoryCount = 0;
    int   scoreHistoryNext = 0;

    LastTimeTelemetry = TimeTelemetry{};

    const int multiPV = std::min<int>(std::max(1, Search::MultiPV), int(worker.rootMoves.size()));

    while (++worker.rootDepth < MAX_PLY && !Stopped.load(std::memory_order_relaxed)
           && !(Limits.depth && worker.rootDepth > Limits.depth)) {

        const TimePoint iterationStartTime = AdaptiveTime ? Time.elapsed() : 0;
        const uint64_t iterationStartNodes = AdaptiveTime ? worker.nodes : 0;
        for (RootMove& rm : worker.rootMoves) {
            rm.previousScore = rm.score;
            if (AdaptiveTime)
                rm.effortNodes = 0;
        }

        for (worker.pvIdx = 0; worker.pvIdx < multiPV && !Stopped.load(std::memory_order_relaxed);
             ++worker.pvIdx) {

            worker.selDepth = 0;

            // Aspiration windows: search a narrow band around the previous
            // iteration's score. Most iterations land inside it, and a narrow
            // window prunes far more. When it fails, widen and retry.
            Value delta = Value(AspDelta);
            Value alpha = -VALUE_INFINITE;
            Value beta  = VALUE_INFINITE;

            if (worker.rootDepth >= 4) {
                const Value prev = worker.rootMoves[worker.pvIdx].previousScore;
                delta            = Value(AspDelta + std::abs(prev) / 128);
                alpha            = std::max(Value(prev - delta), -VALUE_INFINITE);
                beta             = std::min(Value(prev + delta), VALUE_INFINITE);
            }

            int failHighCount = 0;

            while (true) {
                // A repeated fail high means the previous score was too low;
                // searching shallower gets a usable answer sooner.
                const int adjustedDepth = std::max(1, worker.rootDepth - failHighCount);

                bestValue = search<Root>(*pos, ss, alpha, beta, adjustedDepth, false);

                std::stable_sort(worker.rootMoves.begin() + worker.pvIdx, worker.rootMoves.end());

                if (Stopped.load(std::memory_order_relaxed))
                    break;

                if (bestValue <= alpha) {
                    beta          = Value((alpha + beta) / 2);
                    alpha         = std::max(Value(bestValue - delta), -VALUE_INFINITE);
                    failHighCount = 0;
                } else if (bestValue >= beta) {
                    beta = std::min(Value(bestValue + delta), VALUE_INFINITE);
                    ++failHighCount;
                } else
                    break;

                delta = Value(delta + delta / 3);
            }

            std::stable_sort(worker.rootMoves.begin(),
                             worker.rootMoves.begin() + worker.pvIdx + 1);
        }

        // Report after the whole iteration, not after each line. That sort just
        // above can move the line we were working on to a different index, so
        // printing rootMoves[pvIdx] inside the loop can emit the same PV twice
        // under MultiPV.
        if (!Stopped.load(std::memory_order_relaxed) || worker.rootDepth == 1)
            for (int i = 0; i < multiPV; ++i)
                std::cout << pv_line(worker.rootMoves[i], worker.rootDepth, Time.elapsed(), i)
                          << std::endl;

        if (!Stopped.load(std::memory_order_relaxed))
            worker.completedDepth = worker.rootDepth;

        if (worker.rootMoves[0].pv[0] != lastBestMove) {
            lastBestMove      = worker.rootMoves[0].pv[0];
            lastBestMoveDepth = worker.rootDepth;
        }

        // `go mate N`: stop as soon as a mate that short is found.
        if (Limits.mate && bestValue >= VALUE_MATE_IN_MAX_PLY
            && VALUE_MATE - bestValue <= 2 * Limits.mate)
            break;

        // --- should we start another iteration? ---
        if (Time.optimum() && !Stopped.load(std::memory_order_relaxed)) {
            const TimePoint elapsed = Time.elapsed();
            const TimePoint lastIteration = AdaptiveTime ? elapsed - iterationStartTime : 0;
            TimePoint target = Time.optimum();
            double stabilityFactor = 1.0;
            double scoreFactor = 1.0;
            double effortFactor = 1.0;

            if (AdaptiveTime && multiPV == 1) {
                const int stableFor = worker.rootDepth - lastBestMoveDepth;
                stabilityFactor = stableFor == 0 ? 1.30
                                : stableFor == 1 ? 1.10
                                : stableFor <= 3 ? 1.00
                                : stableFor <= 5 ? 0.80 : 0.65;

                if (scoreHistoryCount) {
                    int scoreSum = 0;
                    for (int i = 0; i < scoreHistoryCount; ++i)
                        scoreSum += int(scoreHistory[i]);
                    const double rollingMean = double(scoreSum) / scoreHistoryCount;
                    const double scoreDrop = rollingMean - int(worker.rootMoves[0].score);
                    scoreFactor = std::clamp(1.0 + scoreDrop / 150.0, 0.80, 1.50);
                }

                const uint64_t iterationNodes = worker.nodes - iterationStartNodes;
                const double effort = iterationNodes
                    ? double(worker.rootMoves[0].effortNodes) / double(iterationNodes) : 1.0;
                effortFactor = effort >= 0.70 ? 0.85 : effort <= 0.35 ? 1.15 : 1.00;

                const double scaled = double(Time.optimum()) * stabilityFactor
                                    * scoreFactor * effortFactor;
                target = std::clamp<TimePoint>(TimePoint(scaled),
                    std::max<TimePoint>(1, Time.optimum() / 2), Time.maximum());
            } else {
                // Exact classic policy, retained for AdaptiveTime=false and MultiPV.
                double scale = 1.0;
                const int stableFor = worker.rootDepth - lastBestMoveDepth;
                if (stableFor >= 4)
                    scale *= 0.65;
                else if (stableFor == 0)
                    scale *= 1.35;
                if (previousBest != VALUE_NONE && bestValue + 30 < previousBest)
                    scale *= 1.4;
                target = std::min<TimePoint>(TimePoint(double(Time.optimum()) * scale),
                                             Time.maximum());
            }

            LastTimeTelemetry = {stabilityFactor, scoreFactor, effortFactor,
                                 target, lastIteration};
            const bool targetExpired = elapsed > target;
            const bool nextTooLate = AdaptiveTime && multiPV == 1
                && (elapsed + TimePoint(1.5 * double(lastIteration)) > target
                    || elapsed + TimePoint(1.5 * double(lastIteration)) > Time.maximum());

            if (targetExpired || nextTooLate) {
                if (Pondering.load(std::memory_order_relaxed))
                    StopOnPonderHit.store(true, std::memory_order_relaxed);
                else
                    break;
            }
        }

        const Value completedScore = worker.rootMoves[0].score;
        if (!is_decisive(completedScore)) {
            scoreHistory[scoreHistoryNext] = completedScore;
            scoreHistoryNext = (scoreHistoryNext + 1) % 3;
            scoreHistoryCount = std::min(scoreHistoryCount + 1, 3);
        }
        previousBest = bestValue;
    }

    // A mate/depth limit can finish while the pondered move is still
    // hypothetical.  Wait for ponderhit or an explicit stop; never
    // leak a bestmove during `go ponder`.
    // Reaching an internal depth/node ceiling is not a GUI stop. Infinite and
    // ponder searches retain their answer until stop/ponderhit permits output.
    wait_for_output_permission();

    Stopped.store(true, std::memory_order_relaxed);

    // Ponder on the opponent's expected reply, when we have one.
    const RootMove& best = worker.rootMoves[0];

    if (CorrHistTelemetry)
        print_correction_telemetry();
    if (TimeManagerTelemetry)
        std::cout << "info string timemanager stability "
                  << LastTimeTelemetry.stability
                  << " score_trend " << LastTimeTelemetry.scoreTrend
                  << " root_effort " << LastTimeTelemetry.rootEffort
                  << " target " << LastTimeTelemetry.target
                  << " elapsed " << Time.elapsed()
                  << " depth " << worker.completedDepth
                  << " remaining " << Time.remaining() << std::endl;

#ifdef FORGE_CACHE_AUDIT
    if (ForgeNext::telemetry())
        std::cout << "info string forge2nodes " << worker.nodes << std::endl;
#endif

#ifdef FORGE_NEXT_PROBE
    ForgeNext::report();
#endif
    std::cout << "bestmove " << UCI::move(best.pv[0]);
    if (Search::Ponder.load(std::memory_order_relaxed) && best.pv.size() > 1)
        std::cout << " ponder " << UCI::move(best.pv[1]);
    std::cout << std::endl;

    Running.store(false, std::memory_order_relaxed);
}

}  // namespace

// ---------------------------------------------------------------- public

void Search::init() {
    for (int d = 1; d < 64; ++d)
        for (int m = 1; m < 64; ++m)
            LMR[d][m] = int(0.80 + std::log(double(d)) * std::log(double(m)) / 2.25);
}

void Search::clear() {
    wait();
    TT.clear();
    worker.clear();
}

void Search::start(Position& pos, const LimitsType& limits) {
    wait();
    worker.reset_correction_telemetry();

    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        ExplicitStop = false;
    }
    Limits = limits;
    Pondering.store(limits.ponder, std::memory_order_relaxed);
    StopOnPonderHit.store(false, std::memory_order_relaxed);
    Stopped.store(false, std::memory_order_relaxed);

    worker.nodes          = 0;
    worker.rootDepth      = 0;
    worker.completedDepth = 0;
    worker.selDepth       = 0;
    worker.pvIdx          = 0;
    worker.nmpMinPly      = 0;

    // Build the root move list once, so the search never has to ask whether a
    // root move is legal or permitted by `searchmoves`.
    worker.rootMoves.clear();
    for (const ExtMove& m : MoveList<LEGAL>(pos))
        if (Limits.searchmoves.empty()
            || std::find(Limits.searchmoves.begin(), Limits.searchmoves.end(), Move(m))
                 != Limits.searchmoves.end())
            worker.rootMoves.emplace_back(Move(m));

    Time.init(Limits, pos.side_to_move(), pos.game_ply(), Search::MoveOverhead);
    TT.new_search();

    Running.store(true, std::memory_order_relaxed);
    SearchThread = std::thread(iterative_deepening, &pos);
}

void Search::stop() {
    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        ExplicitStop = true;
        Pondering.store(false, std::memory_order_relaxed);
        Stopped.store(true, std::memory_order_relaxed);
    }
    OutputReady.notify_all();
    wait();
}

void Search::wait() {
    if (SearchThread.joinable())
        SearchThread.join();
}

void Search::ponderhit() {
    {
        std::lock_guard<std::mutex> lock(OutputMutex);
        Pondering.store(false, std::memory_order_relaxed);
        if (StopOnPonderHit.exchange(false, std::memory_order_relaxed))
            Stopped.store(true, std::memory_order_relaxed);
    }
    OutputReady.notify_all();
}

bool Search::running() { return Running.load(std::memory_order_relaxed); }

uint64_t Search::nodes_searched() { return worker.nodes; }

}  // namespace boring
