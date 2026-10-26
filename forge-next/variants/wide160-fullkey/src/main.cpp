// FORGE 2 Placement: standalone CPU UCI engine. GPLv3.
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include "bench.h"
#include "bitboard.h"
#include "endgame.h"
#include "eval_terms.h"
#include "forge_next_probe.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "uci.h"

using namespace boring;

int main(int argc, char* argv[]) {
    try {
        int first = 1;
        std::string model;
        if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "--version")) {
            std::cout << "FORGE Wide160 FullKey (20261024)\n"
                      << "Usage: forge2-policy [--model MODEL.fgc] [test|perftsuite|selftest|perft DEPTH FEN]\n"
                      << "Linux x86-64 Haswell/AVX2, one search thread; no GPU runtime.\n";
            return 0;
        }
        if (argc > 1 && (std::string(argv[1]) == "--model" || std::string(argv[1]) == "--net")) {
            if (argc < 3) throw std::runtime_error("--model needs a file path");
            model = argv[2]; first = 3;
        } else {
            // Exactly one bundled model, independent of the GUI's directory.
            const auto binary = std::filesystem::read_symlink("/proc/self/exe");
            model = (binary.parent_path().parent_path() / "models/capacity.fgc").string();
        }
        std::string error;
        if (!ForgeNext::load_model(model, error))
            throw std::runtime_error(model + ": " + error);
        Bitboards::init();
        Position::init();
        Eval::init_psqt();
        // Match the candidate checks: no KPK bitbase or external tablebases.
        Endgames::Enabled = false;
        Search::init();
        TT.resize(DEFAULT_HASH_MB);
        const std::string cmd = argc > first ? argv[first] : "";
        if (cmd == "test") return run_tests() ? 0 : 1;
        if (cmd == "perftsuite") return perft_suite() ? 0 : 1;
        if (cmd == "selftest") return ForgeNext::self_test() ? 0 : 1;
        if (cmd == "perft") {
            const int depth = argc > first + 1 ? std::stoi(argv[first + 1]) : 4;
            if (depth < 0 || depth > 10) throw std::runtime_error("perft depth must be 0..10");
            Position pos; StateInfo st;
            pos.set(argc > first + 2 ? argv[first + 2] : StartFEN, &st);
            std::cout << "nodes: " << perft<true>(pos, depth) << std::endl;
            return 0;
        }
        if (!cmd.empty()) throw std::runtime_error("unknown argument: " + cmd);
        UCI::loop();
        return 0;
    } catch (const std::exception& e) {
        Search::stop();
        std::cerr << "FORGE 2 error: " << e.what() << std::endl;
        return 2;
    }
}
