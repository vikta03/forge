// main.cpp — start-up and command dispatch.
//
// The relocatable launcher supplies --net. No network access is required.

#include <iostream>
#include <string>

#include "bench.h"
#include "bitboard.h"
#include "endgame.h"
#include "eval_terms.h"
#include "forge_factor_net.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "uci.h"

using namespace boring;

int main(int argc, char* argv[]) {
    int first = 1;
    std::string netPath;
    if (argc > 1 && std::string(argv[1]) == "--net") {
        if (argc < 3) {
            std::cerr << "FORGE startup error: --net needs a model path\n";
            return 2;
        }
        netPath = argv[2];
        first = 3;
    }
    const std::string cmd = argc > first ? argv[first] : "";
    // Rule/perft checks may deliberately exercise the inherited evaluator.
    // Playing and benchmarking require an explicitly supplied learned model.
    if (!netPath.empty()) {
        std::string error;
        if (!ForgeFactor::load(netPath, error)) {
            std::cerr << "FORGE startup error: " << error << '\n';
            return 2;
        }
    } else if (cmd != "test" && cmd != "perft" && cmd != "perftsuite") {
        std::cerr << "FORGE startup error: use the forge launcher, or --net MODEL.forge\n";
        return 2;
    }
    // Order matters: the magic tables must exist before anything asks for
    // attacks, and the piece-square tables before any position is set up.
    Bitboards::init();
    Position::init();
    Eval::init_psqt();
    Endgames::init();  // solves KPK by retrograde analysis
    Search::init();

    // Every path that searches needs a table, not just the UCI loop: `bench`
    // would otherwise probe a null pointer on its first node.
    TT.resize(DEFAULT_HASH_MB);

    if (cmd == "perftsuite") {
        std::cout << "FORGE netB 1.0.0" << std::endl;
        return perft_suite() ? 0 : 1;
    }

    if (cmd == "test") {
        std::cout << "FORGE netB 1.0.0" << std::endl;
        return run_tests() ? 0 : 1;
    }

    if (cmd == "bench") {
        std::cout << "FORGE netB 1.0.0" << std::endl;
        bench();
        return 0;
    }

    if (cmd == "perft") {
        const int         depth = (argc > first + 1) ? std::stoi(argv[first + 1]) : 5;
        const std::string fen   = (argc > first + 2) ? argv[first + 2] : StartFEN;

        Position  pos;
        StateInfo st;
        pos.set(fen, &st);

        std::cout << pos << std::endl;
        std::cout << "\nnodes: " << perft<true>(pos, depth) << std::endl;
        return 0;
    }

    std::cout << "FORGE netB 1.0.0 — learned evaluation, CPU alpha-beta search" << std::endl;

    UCI::loop();
    return 0;
}
