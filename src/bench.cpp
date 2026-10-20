// bench.cpp — perft and the embedded test suites.

#include "bench.h"

#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>

#include "movegen.h"
#include "position.h"
#include "search.h"
#include "timeman.h"
#include "uci.h"

namespace boring {

namespace {

struct PerftCase {
    const char* fen;
    int         depth;
    uint64_t    nodes;
    const char* note;
    bool        chess960 = false;
};

// The six standard positions from the Chess Programming Wiki, followed by the
// small targeted positions that catch the rules almost every generator gets
// wrong the first time: en passant that would expose the king, castling rights
// lost by a rook capture, promotions while in check, and stalemate.
constexpr PerftCase PERFT_CASES[] = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6, 119060324, "startpos"},

    {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5, 193690690,
     "kiwipete: every piece type active, all four castles available"},

    {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661,
     "en passant discovered check along the fifth rank"},

    {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5, 15833292,
     "promotions with and without capture, both colours"},

    {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194,
     "sharp middlegame with a pawn on the seventh"},

    {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 5, 164075551,
     "quiet symmetrical middlegame, high branching factor"},

    // --- targeted rule cases ---
    {"3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 6, 1134888,
     "en passant capture would expose our own king to the rook"},

    {"8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 6, 1015133,
     "double push is illegal because the bishop pins the pawn"},

    {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 6, 1440467,
     "en passant is the only way out of check"},

    {"5k2/8/8/8/8/8/8/4K2R w K - 0 1", 6, 661072, "castling out of, through and into check"},

    {"3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 6, 803711, "queenside castling clears b1 as well"},

    {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 4, 1274206,
     "castling rights revoked by moving and by capture"},

    {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 4, 1720476,
     "both sides may castle while queens rake the board"},

    {"2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 6, 3821001, "promotion gives check and blocks it"},

    {"8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 5, 1004658, "knight and queen fork the promotion"},

    {"4k3/1P6/8/8/8/8/K7/8 w - - 0 1", 6, 217342, "under-promotion to avoid stalemate"},

    {"8/P1k5/K7/8/8/8/8/8 w - - 0 1", 6, 92683, "promotion with the kings adjacent"},

    {"K1k5/8/P7/8/8/8/8/8 w - - 0 1", 6, 2217, "stalemate and immobility"},

    {"8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 7, 567584, "deep pawn race"},

    {"8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", 4, 23527, "double check forces a king move"},

    // --- Chess960 ---
    //
    // Castling is where 960 differs, and it differs in ways that are easy to
    // get subtly wrong: the king may not move at all, the king and rook may
    // swap squares, and the rook may be pinned even when the king's route is
    // clear. Node counts were cross-checked move-by-move against a reference
    // engine; the "+N" cases are positions reached after N random plies, so the
    // trees actually contain castling rather than only the right to castle.
    {"rnqnbbkr/pppppppp/8/8/8/8/PPPPPPPP/RNQNBBKR w HAha - 0 1", 5, 4995757,
     "960 start #426", true},

    {"rknnqbbr/pppppppp/8/8/8/8/PPPPPPPP/RKNNQBBR w HAha - 0 1", 5, 5043679,
     "960 start #750: king on b1, rook on a1", true},

    {"qnnrbbkr/pppppppp/8/8/8/8/PPPPPPPP/QNNRBBKR w HDhd - 0 1", 5, 4796067,
     "960 start #10: queenside rook on d1", true},

    {"rkbnrqnb/pppppppp/8/8/8/8/PPPPPPPP/RKBNRQNB w EAea - 0 1", 5, 4890778,
     "960 start #839: both rooks adjacent to the king", true},

    {"rnqnbbkr/ppp1ppp1/3p3p/8/6P1/5P2/PPPPP2P/RNQNBBKR w HAha - 0 3", 5, 17065664,
     "960 midgame, both sides still able to castle", true},

    {"rknn1bbr/ppp1p1pp/1q6/3p1p2/8/1PP5/PN1PPPPP/RK1NQBBR w HAha - 3 5", 5, 11767984,
     "960 midgame, king already adjacent to its rook", true},

    {"qnnrbb1r/p1ppp1pp/4k3/1p3p2/2P5/3NPP2/PP1P2PP/QN1RBBKR w HD - 1 5", 5, 14883338,
     "960 midgame, black has lost both castling rights", true},

    {"rknnqbbr/1p1ppppp/p7/2p5/P7/8/KPPPPPPP/R1NNQBBR w ha - 0 3", 5, 6895274,
     "960 midgame, white king has moved off its start square", true},
};

// Positions the bench searches. Chosen to cover the shapes the search has to
// cope with rather than to be pretty: open and closed middlegames, tactical
// positions, endgames with and without pawns, and a couple of mates.
constexpr const char* BENCH_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "4rrk1/pp1n1pp1/1q2p2p/1P1bP3/3P4/P2NBP2/2Q2P1P/R4RK1 w - - 0 21",
    "r1bqk2r/pp2bppp/2p5/3pP3/P2Q1P2/2N1B3/1PP3PP/R4RK1 b kq - 0 12",
    "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQk - 0 11",
    "r1bq1rk1/pp2ppbp/2np2p1/2n5/P3PP2/N1P2N2/1PB3PP/R1B1QRK1 b - - 1 12",
    "3rr1k1/pp3pp1/1qn2np1/8/3p4/PP1R1P2/2P1NQPP/R1B3K1 b - - 3 20",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "8/8/8/8/8/6k1/6p1/6K1 w - - 0 1",
    "8/2k5/8/8/8/8/5PPP/6K1 w - - 0 1",
    "6k1/6p1/6Pp/ppp5/3pP3/2P1P1P1/PP5P/K7 w - - 0 1",
    "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
    "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
    "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
    "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
    "r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1",
    "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1",
    "r1bqk2r/pp3ppp/2n1pn2/2bp4/2P5/2N1PN2/PPQ1BPPP/R1B1K2R w KQkq - 0 1",
    "8/8/3k4/8/8/3K4/8/3Q4 w - - 0 1",
    "8/8/8/8/4k3/8/4K3/4R3 w - - 0 1",
    "6k1/5ppp/8/8/8/8/5PPP/2R3K1 w - - 0 1",
    "r3k2r/pb3p2/5npp/n2p4/1p1PPB2/6P1/P2N1PBP/R3K2R w KQkq - 0 1",
    "3rr3/2pq2pk/p2p1pnp/8/2QBPP2/1P6/P5PP/4RRK1 b - - 0 1",
};

}  // namespace

template<bool Root>
uint64_t perft(Position& pos, int depth) {
    StateInfo st;
    uint64_t  nodes = 0;

    // At depth 2 the children are leaves, so counting the legal move list is
    // enough and saves a make/unmake per leaf.
    const bool leaf = (depth == 2);

    for (const ExtMove& m : MoveList<LEGAL>(pos)) {
        uint64_t cnt;

        if (Root && depth <= 1) {
            cnt = 1;
            ++nodes;
        } else {
            pos.do_move(m, st);
            cnt = leaf ? uint64_t(MoveList<LEGAL>(pos).size()) : perft<false>(pos, depth - 1);
            nodes += cnt;
            pos.undo_move(m);
        }

        if (Root)
            std::cout << UCI::move(m) << ": " << cnt << '\n';
    }

    return nodes;
}

template uint64_t perft<true>(Position&, int);
template uint64_t perft<false>(Position&, int);

bool perft_suite() {
    using Clock = std::chrono::steady_clock;

    Position   pos;
    StateInfo  st;
    uint64_t   total   = 0;
    int        failures = 0;
    const auto start   = Clock::now();

    for (const PerftCase& c : PERFT_CASES) {
        pos.set(c.fen, &st, c.chess960);

        const uint64_t got = perft<false>(pos, c.depth);
        const bool     ok  = (got == c.nodes);

        total += got;
        failures += !ok;

        std::cout << (ok ? "  ok   " : "  FAIL ") << (c.chess960 ? "960 " : "    ") << "depth "
                  << c.depth << std::setw(12) << got;

        if (!ok)
            std::cout << "  expected " << c.nodes;

        std::cout << "   " << c.note << '\n';

        if (!ok)
            std::cout << "         " << c.fen << '\n';
    }

    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();

    std::cout << "\n" << (failures ? "PERFT SUITE FAILED" : "perft suite passed") << " — "
              << (sizeof(PERFT_CASES) / sizeof(PERFT_CASES[0])) << " positions, " << total
              << " nodes in " << ms << " ms";

    if (ms > 0)
        std::cout << " (" << (total / uint64_t(ms)) << " knps)";

    std::cout << std::endl;

    return failures == 0;
}

void bench() {
    using Clock = std::chrono::steady_clock;

    constexpr int BENCH_DEPTH = 11;

    Position              pos;
    std::deque<StateInfo> states;

    uint64_t   total = 0;
    const auto start = Clock::now();

    // Start from a clean table so the result does not depend on whatever the
    // engine happened to be doing beforehand.
    Search::clear();

    for (const char* fen : BENCH_FENS) {
        states.clear();
        states.emplace_back();
        pos.set(fen, &states.back());

        LimitsType limits;
        limits.startTime = now();
        limits.depth     = BENCH_DEPTH;

        Search::start(pos, limits);
        Search::wait();

        total += Search::nodes_searched();
    }

    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();

    std::cout << "\n===========================\n"
              << "Total time (ms) : " << ms << "\n"
              << "Nodes searched  : " << total << "\n"
              << "Nodes/second    : " << (total * 1000 / uint64_t(std::max<int64_t>(ms, 1)))
              << std::endl;
}

}  // namespace boring
