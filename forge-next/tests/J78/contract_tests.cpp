// J78: compile under the approved CPU job; retain all J47/J48 contracts.
#include "j48_contracts.cpp"

namespace boring {
std::vector<Move> brute_repetitions(Position& pos, int ply) {
    std::vector<Move> moves;
    for (Move move : MoveList<LEGAL>(pos)) {
        StateInfo child;
        pos.do_move(move, child);
        // Repetition only: do not confuse dead material / 50-move draws with
        // a legal repetition move. This uses the original state updater.
        if (child.repetition && child.repetition < ply + 1)
            moves.push_back(move);
        pos.undo_move(move);
    }
    return moves;
}

void repetition_file(const char* path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot read repetition fixtures");
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string id, fen, mode, sequence;
        std::getline(row, id, '\t'); std::getline(row, fen, '\t');
        std::getline(row, mode, '\t'); std::getline(row, sequence);
        UCI::Chess960 = mode == "1";
        std::deque<StateInfo> states(1);
        Position pos; pos.set(fen, &states.back(), UCI::Chess960);
        std::istringstream moves(sequence); std::string text;
        int step = 0;
        do {
            for (int ply : {0, 1, 3, 4, 8, 64}) {
                const Key key = pos.key(), repKey = pos.repetition_key();
                const std::string before = pos.fen();
                const Move got = pos.repetition_move(ply);
                const auto expected = brute_repetitions(pos, ply);
                const bool unchanged = key == pos.key() && repKey == pos.repetition_key()
                                       && before == pos.fen() && pos.pos_is_ok();
                const bool member = std::find(expected.begin(), expected.end(), got) != expected.end();
                const bool passed = unchanged && (got == MOVE_NONE ? expected.empty() : member);
                std::cout << "{\"id\":" << std::quoted(id) << ",\"step\":" << step
                          << ",\"search_ply\":" << ply << ",\"move\":" << std::quoted(UCI::move(got))
                          << ",\"unchanged\":" << (unchanged ? "true" : "false")
                          << ",\"passed\":" << (passed ? "true" : "false") << ",\"brute\":[";
                for (size_t i = 0; i < expected.size(); ++i)
                    std::cout << (i ? "," : "") << std::quoted(UCI::move(expected[i]));
                std::cout << "]}\n";
            }
            if (!(moves >> text)) break;
            states.emplace_back();
            if (text == "0000") {
                if (pos.checkers()) throw std::runtime_error("null fixture in check");
                pos.do_null_move(states.back());
            } else {
                const Move move = UCI::to_move(pos, text);
                if (move == MOVE_NONE) throw std::runtime_error("illegal fixture move: " + text);
                pos.do_move(move, states.back());
            }
            ++step;
        } while (true);
    }
}

void near_cycle(BoardFixture& b, bool third = true) {
    if (third)
        for (const char* m : {"g1f3", "g8f6", "f3g1", "f6g8"}) b.move(m);
    for (const char* m : {"g1f3", "g8f6", "f3g1"}) b.move(m);
}

void repetition_contracts() {
    UCI::Chess960 = false;
    for (bool mainSearch : {false, true}) {
        Value scores[2];
        for (bool enabled : {false, true}) {
            SearchFixture fixture; BoardFixture b(StartFEN); near_cycle(b);
            RepetitionGuard = enabled;
            RepetitionGuardHits[0] = RepetitionGuardHits[1] = 0;
            seed(b.pos, mainSearch ? Value(-120) : VALUE_NONE, Value(-120),
                 mainSearch ? BOUND_EXACT : BOUND_NONE, mainSearch ? 20 : DEPTH_NONE);
            scores[enabled] = mainSearch
                ? search<NonPV>(b.pos, fixture.ss, Value(-1), Value(0), 2, false)
                : qsearch<NonPV>(b.pos, fixture.ss, Value(-1), Value(0), 0);
            bool hit = false; const auto* entry = TT.probe(tt_key(b.pos), hit);
            if (enabled)
                result(mainSearch ? "repetition_main_no_tt_write" : "repetition_q_no_tt_write",
                       hit && entry->eval() == Value(-120)
                       && entry->value() == (mainSearch ? Value(-120) : VALUE_NONE),
                       "history-dependent early cutoff leaves seeded TT untouched");
            result((std::string("repetition_") + (mainSearch ? "main_" : "q_")
                    + (enabled ? "on" : "off")).c_str(),
                   scores[enabled] == (enabled ? Value(0) : Value(-120))
                   && RepetitionGuardHits[mainSearch ? 0 : 1] == uint64_t(enabled),
                   "known quiet third repetition; control=" + std::to_string(scores[enabled]));
        }
    }
    for (bool mainSearch : {false, true}) {
        SearchFixture fixture; BoardFixture b(StartFEN); near_cycle(b);
        RepetitionGuard = true; RepetitionGuardHits[0] = RepetitionGuardHits[1] = 0;
        seed(b.pos, mainSearch ? Value(-120) : VALUE_NONE, Value(-120),
             mainSearch ? BOUND_EXACT : BOUND_NONE, mainSearch ? 20 : DEPTH_NONE);
        const Value value = mainSearch
            ? search<NonPV>(b.pos, fixture.ss, Value(0), Value(1), 2, false)
            : qsearch<NonPV>(b.pos, fixture.ss, Value(0), Value(1), 0);
        result(mainSearch ? "repetition_main_positive_beta" : "repetition_q_positive_beta",
               value == Value(-120) && RepetitionGuardHits[0] + RepetitionGuardHits[1] == 0,
               "a draw does not close a positive window");
    }
    {
        SearchFixture fixture; BoardFixture b(StartFEN); near_cycle(b);
        RepetitionGuard = true; RepetitionGuardHits[0] = RepetitionGuardHits[1] = 0;
        fixture.ss->excludedMove = UCI::to_move(b.pos, "f6g8");
        seed(b.pos, VALUE_NONE, Value(-120));
        const Value value = qsearch<NonPV>(b.pos, fixture.ss, Value(-1), Value(0), 0);
        result("repetition_excluded_move_no_shortcut", value == Value(-120)
               && RepetitionGuardHits[1] == 0, "exclude the available repetition");
    }
    {
        SearchFixture fixture; BoardFixture b(StartFEN); near_cycle(b);
        RepetitionGuard = true; RepetitionGuardHits[0] = RepetitionGuardHits[1] = 0;
        Stopped.store(true);
        const Value value = search<NonPV>(b.pos, fixture.ss, Value(-1), Value(0), 2, false);
        result("repetition_stop_precedes_guard", value == 0
               && RepetitionGuardHits[0] + RepetitionGuardHits[1] == 0, "stop remains first");
    }
    {
        BoardFixture b(StartFEN); near_cycle(b, false);
        result("repetition_twofold_root_boundary", b.pos.repetition_move(0) == MOVE_NONE
               && b.pos.repetition_move(3) == MOVE_NONE
               && b.pos.repetition_move(4) != MOVE_NONE,
               "twofold at root is not a claim; wholly inside search is a cycle");
    }
    {
        SearchFixture fixture; BoardFixture b(StartFEN); near_cycle(b);
        RepetitionGuard = true; RepetitionGuardHits[0] = RepetitionGuardHits[1] = 0;
        seed(b.pos, VALUE_NONE, Value(-120));
        const Value value = qsearch<PV>(b.pos, fixture.ss, Value(-1000), Value(1000), 0);
        result("repetition_pv_not_declared_draw", value == Value(-120)
               && RepetitionGuardHits[1] == 0, "PV search has not been replaced by a draw label");
    }
    RepetitionGuard = true;
    terminal_tests();  // J48 guards still work with the new option enabled.
    RepetitionGuard = false;
}
} // namespace boring

int main(int argc, char** argv) {
    using namespace boring;
    try {
        if (argc == 4 && std::string(argv[2]) == "--repetition-file") {
            Bitboards::init(); Position::init(); Eval::init_psqt(); Search::init();
            repetition_file(argv[3]); return 0;
        }
        RepetitionGuard = false;
        const int rc = j48_contract_main(argc, argv);
        if (rc || argc != 2) return rc;
        repetition_contracts(); return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 2;
    }
}
