// timeman.cpp — time allocation.

#include "timeman.h"

#include <algorithm>

namespace boring {

void TimeManager::init(const LimitsType& limits, Color us, int ply, TimePoint moveOverhead) {
    startTime = limits.startTime;
    clockTime = limits.time[us];

    // "Spend exactly this long" — still leave the overhead, or we lose on time
    // to the GUI's own latency. Checked before use_time_management(), because
    // `go movetime 1000` carries no wtime/btime and would otherwise look like
    // an untimed search.
    if (limits.movetime) {
        // Fixed duration uses only the hard deadline, never stability-based
        // soft stopping between iterations.
        optimumTime = 0;
        maximumTime = std::max<TimePoint>(1, limits.movetime - moveOverhead);
        return;
    }

    // No clock at all: fixed depth, fixed nodes, or infinite analysis. The
    // search stops when it is told to, or when it runs out of depth.
    if (!limits.use_time_management()) {
        optimumTime = maximumTime = 0;
        return;
    }

    const TimePoint left = std::max<TimePoint>(1, limits.time[us] - moveOverhead);
    const TimePoint inc  = limits.inc[us];

    // How many more moves this time allocation has to cover. With movestogo we
    // know exactly; without it, assume a long game — but not an infinite one,
    // or the engine hoards time it will never get to use.
    //
    // This was missing entirely from the previous engine, which is why it
    // played badly at repeating tournament time controls.
    const int mtg = limits.movestogo ? std::min(limits.movestogo, 40) : 40;

    // The share of the clock for this move. Most of the increment is spendable
    // immediately, since it is refunded as soon as the move is played; the
    // remainder is a slice of what is left.
    TimePoint base = left / mtg + inc * 3 / 4;

    // The opening is usually book or near-forced, and the deep middlegame is
    // where thinking pays. Ramp the allocation up over roughly the first twenty
    // moves rather than spending a full share on move one.
    const int rampPly = std::min(ply, 40);
    base              = base * (70 + rampPly) / 100;

    // A single move may never take a large fraction of the remaining clock.
    // With movestogo the reserve has to last until the control, so it is
    // tighter there.
    const TimePoint cap = std::max<TimePoint>(
        1, limits.movestogo ? left / 2 : left / 4);

    optimumTime = std::clamp<TimePoint>(base, TimePoint(1), cap);

    // The hard ceiling. The search may exceed `optimum` when the position is
    // unstable — a fail low at the root, a best move that keeps changing — but
    // never this.
    maximumTime = std::clamp<TimePoint>(optimumTime * 5, 1, std::max<TimePoint>(1, left * 4 / 5));
}

}  // namespace boring
