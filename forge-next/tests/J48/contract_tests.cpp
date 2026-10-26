// J48 terminal-search witnesses. Build only under the approved CPU job.
// Retain the J47 suite unchanged as a regression gate.
#define main j47_contract_main
#include "j47_contracts.cpp"
#undef main

namespace boring {
void terminal_tests() {
    // Black is stalemated, not in check, and is not a dead-material draw.
    const char* fen = "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1";
    for (bool mainSearch : {false, true})
        for (int mode = 0; mode < 3; ++mode) {
            SearchFixture f; BoardFixture b(fen);
            const int raw = mode == 1 ? -200 : 200;
            seed(b.pos, mode == 2 ? Value(200) : VALUE_NONE, Value(raw),
                 mode == 2 ? BOUND_EXACT : BOUND_NONE,
                 mode == 2 ? 20 : DEPTH_NONE);
            const Value v = mainSearch
                ? search<NonPV>(b.pos, f.ss, Value(-1), Value(0), 2, false)
                : qsearch<NonPV>(b.pos, f.ss, Value(-1), Value(0), 0);
            const std::string name = std::string("terminal_")
                + (mainSearch ? "main_" : "q_") + std::to_string(mode);
            result(name.c_str(), v == VALUE_DRAW,
                   "stalemate, seeded raw=" + std::to_string(raw)
                   + ", result=" + std::to_string(v));
        }
    for (bool mainSearch : {false, true}) {
        SearchFixture f; BoardFixture b(fen);
        f.ss->ply = MAX_PLY;
        const Value v = mainSearch
            ? search<PV>(b.pos, f.ss, Value(-1000), Value(1000), 1, false)
            : qsearch<PV>(b.pos, f.ss, Value(-1000), Value(1000), 0);
        result(mainSearch ? "terminal_main_stack_limit" : "terminal_q_stack_limit",
               v == VALUE_DRAW, "terminal result precedes evaluator stack fallback: " + std::to_string(v));
    }
    {
        SearchFixture f; BoardFixture b("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
        const Value v = qsearch<PV>(b.pos, f.ss, Value(-VALUE_INFINITE), Value(VALUE_INFINITE), 0);
        result("terminal_checkmate_not_stalemate", v == mated_in(1),
               "mate score=" + std::to_string(v));
    }
    {
        SearchFixture f; BoardFixture b("7k/4Q3/6K1/8/8/8/8/8 b - - 0 1");
        seed(b.pos, VALUE_NONE, Value(200));
        const Value v = qsearch<NonPV>(b.pos, f.ss, Value(-1), Value(0), 0);
        result("terminal_legal_king_escape_control", v == 200,
               "Kg8 exists, stand-pat remains valid: " + std::to_string(v));
    }
}

void legality_file(const char* path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read legality fixtures");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line); std::string id, fen, mode;
        std::getline(row, id, '\t'); std::getline(row, fen, '\t'); std::getline(row, mode);
        StateInfo st; Position pos; UCI::Chess960 = mode == "1";
        pos.set(fen, &st, UCI::Chess960);
        const Key key = pos.key(); const std::string before = pos.fen();
#if TERMINAL_GUARD_PRESENT
        const bool exists = has_legal_move(pos);
#else
        const bool exists = MoveList<LEGAL>(pos).size() != 0;
#endif
        std::vector<std::string> legal;
        for (Move m : MoveList<LEGAL>(pos)) legal.push_back(UCI::move(m));
        std::sort(legal.begin(), legal.end());
        const bool unchanged = key == pos.key() && before == pos.fen() && pos.pos_is_ok();
        std::cout << "{\"id\":" << std::quoted(id)
                  << ",\"exists\":" << (exists ? "true" : "false")
                  << ",\"in_check\":" << (pos.checkers() ? "true" : "false")
                  << ",\"unchanged\":" << (unchanged ? "true" : "false")
                  << ",\"legal\":[";
        for (size_t i = 0; i < legal.size(); ++i)
            std::cout << (i ? "," : "") << std::quoted(legal[i]);
        std::cout << "]}\n";
    }
}
}  // namespace boring

int main(int argc, char** argv) {
    using namespace boring;
    try {
        if (argc == 4 && std::string(argv[2]) == "--legality-file") {
            Bitboards::init(); Position::init(); Eval::init_psqt(); Search::init();
            legality_file(argv[3]); return 0;
        }
        const int rc = j47_contract_main(argc, argv);
        if (rc) return rc;
        terminal_tests(); return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 2;
    }
}
