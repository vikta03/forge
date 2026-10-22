// timeman.h — deciding how long to think.
//
// Two limits come out of this. `optimum` is what we expect to spend: it is
// checked between iterations, and may be stretched or cut depending on how the
// search is going. `maximum` is a hard ceiling polled inside the search, and is
// never exceeded for any reason.

#ifndef BORING_TIMEMAN_H
#define BORING_TIMEMAN_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include "types.h"

namespace boring {

using TimePoint = std::chrono::milliseconds::rep;

inline TimePoint now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Everything a `go` command can ask for.
struct LimitsType {
    std::vector<Move> searchmoves;

    TimePoint time[COLOR_NB] = {0, 0};
    TimePoint inc[COLOR_NB]  = {0, 0};
    TimePoint movetime       = 0;
    TimePoint startTime      = 0;

    int      movestogo = 0;
    int      depth     = 0;
    int      mate      = 0;
    uint64_t nodes     = 0;
    bool     infinite  = false;
    bool     ponder    = false;

    // True when there is a real clock to manage, as opposed to a fixed depth,
    // node count, movetime, or an infinite analysis search.
    bool use_time_management() const { return time[WHITE] || time[BLACK]; }
};

class TimeManager {
   public:
    void init(const LimitsType& limits, Color us, int ply, TimePoint moveOverhead);

    TimePoint optimum() const { return optimumTime; }
    TimePoint maximum() const { return maximumTime; }
    TimePoint elapsed() const { return now() - startTime; }
    TimePoint remaining() const {
        return clockTime ? std::max<TimePoint>(0, clockTime - elapsed()) : 0;
    }

   private:
    TimePoint startTime   = 0;
    TimePoint optimumTime = 0;
    TimePoint maximumTime = 0;
    TimePoint clockTime   = 0;
};

}  // namespace boring

#endif  // BORING_TIMEMAN_H
