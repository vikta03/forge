// Search audit fixtures. GPL-3.0. Compiled only by approved J47.
// Includes the actual search translation unit. Three documented observation
// hooks are inserted in a run-local copy; normal engine builds have no hooks.
#include <array>
#include <deque>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include "search.h"
#include "tt.h"
#include "bitboard.h"
#include "endgame.h"
#include "eval_terms.h"
#include "forge_next_probe.h"
#include "uci.h"

namespace boring {
void audit_before_extensions(Search::Stack*, TTEntry*, Key, Move, Move);
void audit_singular(Search::Stack*);
void audit_before_move(Search::Stack*, int);
void audit_child_return(const char*);
}
#include AUDIT_SEARCH_SOURCE

namespace boring {
struct Observation {
    int mode = 0, moves = 0, singular = 0, doubles = 0, leaks = 0;
    bool replaced = false;
} observation;
void audit_child_return(const char* kind) {
    if ((observation.mode == 3 && std::string(kind) == "null")
        || (observation.mode == 4 && std::string(kind) == "probcut"))
        Stopped.store(true, std::memory_order_relaxed);
}
void audit_before_extensions(Stack* ss, TTEntry* tte, Key key, Move move, Move ttMove) {
    if (observation.mode == 1 && ss->ply == 1 && !ss->excludedMove
        && move == ttMove && !observation.replaced) {
        // Model a recursive search replacing the same cluster slot. This is
        // deliberately adversarial, not an assertion about natural frequency.
        TT.save(tte, key ^ 1, Value(-7), false, BOUND_UPPER, 1, MOVE_NONE,
                Value(-7), TT.generation(), 0);
        observation.replaced = true;
    }
}
void audit_singular(Stack* ss) {
    if (observation.mode && ss->ply == 1) ++observation.singular;
}
void audit_before_move(Stack* ss, int extension) {
    if (!observation.mode || ss->ply != 1 || ss->excludedMove) return;
    ++observation.moves;
    observation.doubles += extension == 2;
    observation.leaks += ss->doubleExtensions != (ss - 1)->doubleExtensions + (extension == 2);
    if (observation.mode == 1 || observation.moves >= 2)
        Stopped.store(true, std::memory_order_relaxed);
}

struct BoardFixture {
    Position pos;
    std::deque<StateInfo> states;
    explicit BoardFixture(const std::string& fen) {
        states.emplace_back(); pos.set(fen, &states.back());
    }
    void move(const char* text) {
        const Move m = UCI::to_move(pos, text);
        if (m == MOVE_NONE) throw std::runtime_error(std::string("illegal fixture move ") + text);
        states.emplace_back(); pos.do_move(m, states.back());
    }
};
struct SearchFixture {
    std::array<Stack, MAX_PLY + 12> stack{};
    std::array<Move, MAX_PLY + 1> pv{};
    Stack* ss = stack.data() + 9;  // local search starts at ply one
    SearchFixture() {
        worker.clear(); TT.clear(); Limits = LimitsType{};
        Limits.startTime = now(); Limits.nodes = 65536;
        Time.init(Limits, WHITE, 0, 0);
        Stopped.store(false); worker.rootDepth = 8;
        CorrHistScale = 0; SpecialMoveSEE = false;
        observation = {};
        for (int i = 0; i < int(stack.size()); ++i) {
            stack[i].ply = i - 8;
            stack[i].continuationHistory = &worker.sentinelContinuation;
        }
        ss->pv = pv.data();
    }
};
void seed(Position& pos, Value value, Value eval, Bound bound = BOUND_NONE,
          int depth = DEPTH_NONE, Move move = MOVE_NONE, bool pv = false) {
    bool hit; const Key key = tt_key(pos); TTEntry* e = TT.probe(key, hit);
    TT.save(e, key, value, pv, bound, depth, move, eval, TT.generation(), pos.rule50_count());
}
void result(const char* name, bool passed, const std::string& detail) {
    std::cout << "{\"test\":" << std::quoted(name) << ",\"passed\":"
              << (passed ? "true" : "false") << ",\"detail\":" << std::quoted(detail) << "}\n";
}
void cycle(BoardFixture& b) {
    for (const char* m : {"g1f3", "g8h8", "f3g1", "h8g8"}) b.move(m);
}
void rules_tests() {
    for (bool pinned : {true, false}) {
        BoardFixture b(pinned ? "4r1k1/3p4/8/4P3/8/8/8/4K1N1 b - - 0 1"
                              : "4r1k1/3p4/8/4P3/8/8/8/K5N1 b - - 0 1");
        b.move("d7d5"); cycle(b); cycle(b);
        const bool draw = b.pos.is_draw(0);
        result(pinned ? "pinned_ep_third_occurrence" : "legal_ep_right_distinguishes_position",
               draw == pinned, std::string("draw=") + (draw ? "true" : "false"));
        cycle(b);
        result(pinned ? "pinned_ep_later_repeat" : "legal_ep_later_repeat", b.pos.is_draw(0), "third/fourth occurrence");
    }
    struct Dead { const char* name; const char* fen; bool dead; };
    for (const auto& c : {
        Dead{"dead_KK", "4k3/8/8/8/8/8/8/4K3 w - - 0 1", true},
        Dead{"dead_KBK", "4k3/8/8/8/8/8/8/4KB2 w - - 0 1", true},
        Dead{"dead_KNK", "4k3/8/8/8/8/8/8/4K1N1 w - - 0 1", true},
        Dead{"dead_same_colour_bishops", "2b1k3/8/8/8/8/8/8/4KB2 w - - 0 1", true},
        Dead{"not_dead_opposite_bishops", "1b2k3/8/8/8/8/8/8/4KB2 w - - 0 1", false},
        Dead{"not_dead_two_knights", "4k3/8/8/8/8/8/8/3NK1N1 w - - 0 1", false},
        Dead{"not_dead_bishop_knight", "4k3/8/8/8/8/8/8/3NK1B1 w - - 0 1", false},
        Dead{"not_dead_pawn", "4k3/8/8/8/8/8/P7/4K3 w - - 0 1", false}}) {
        BoardFixture b(c.fen); result(c.name, b.pos.is_draw(0) == c.dead, "rules classification");
    }
    BoardFixture b("4r1k1/8/8/3pP3/8/8/8/4K1N1 w - d6 0 1");
    const Key key = b.pos.key(); const std::string fen = b.pos.fen(); StateInfo nullState;
    b.pos.do_null_move(nullState); const bool ok = b.pos.pos_is_ok(); b.pos.undo_null_move();
    result("null_make_unmake_state", ok && b.pos.key() == key && b.pos.fen() == fen && b.pos.pos_is_ok(), "includes EP and repetition identity");
}
void clock_tests() {
    LimitsType l; l.startTime = now(); l.time[WHITE] = l.time[BLACK] = 60000;
    TimeManager t; t.init(l, WHITE, 0, 10);
    result("game_clock_remains_managed", t.optimum() > 0 && t.maximum() >= t.optimum(), "ordinary clock");
    l.movetime = 300; t.init(l, WHITE, 0, 10);
    result("movetime_has_no_soft_stop", t.optimum() == 0 && t.maximum() == 290,
           "optimum=" + std::to_string(t.optimum()) + " maximum=" + std::to_string(t.maximum()));
    l.movetime = 0;
    for (int kind = 0; kind < 4; ++kind) {
        l.depth = kind == 0 ? 4 : 0; l.nodes = kind == 1 ? 4096 : 0;
        l.infinite = kind == 2; l.mate = kind == 3 ? 2 : 0;
        t.init(l, WHITE, 0, 10);
        result((std::string("explicit_limit_ignores_soft_clock_") + std::to_string(kind)).c_str(),
               t.optimum() == 0 && t.maximum() == 0, "explicit bound with supplied clocks");
    }
}
void direct_search_tests() {
    {
        SearchFixture f; BoardFixture b(StartFEN); seed(b.pos, VALUE_NONE, Value(123));
        Stopped.store(true); const Value v = qsearch<NonPV>(b.pos, f.ss, Value(200), Value(201), 0);
        bool hit; auto* e = TT.probe(tt_key(b.pos), hit);
        result("stopped_qsearch_leaves_cache_untouched", v == 0 && e->depth() == DEPTH_NONE,
               "value=" + std::to_string(v) + " depth=" + std::to_string(e->depth()));
    }
    for (bool see : {false, true}) {
        SearchFixture f; SpecialMoveSEE = see;
        BoardFixture b("6k1/8/8/3pP3/8/8/8/4K1N1 w - d6 0 1");
        seed(b.pos, VALUE_NONE, Value(0));
        Move ep = UCI::to_move(b.pos, "e5d6"); StateInfo child;
        b.pos.do_move(ep, child); seed(b.pos, VALUE_NONE, Value(-100)); b.pos.undo_move(ep);
        Value v = qsearch<NonPV>(b.pos, f.ss, Value(60), Value(61), 0);
        result(see ? "ep_delta_real_see" : "ep_delta_default_see", v == 100 && worker.nodes > 0,
               "value=" + std::to_string(v) + " nodes=" + std::to_string(worker.nodes));
    }
    {
        SearchFixture f; BoardFixture b("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        seed(b.pos, VALUE_NONE, Value(123));
        const Value v = qsearch<PV>(b.pos, f.ss, Value(-200), Value(200), 0);
        result("dead_position_beats_neural_stand_pat", v == VALUE_DRAW, "seeded static evaluation 123; actual result=" + std::to_string(v));
    }
    {
        SearchFixture f; BoardFixture b("8/8/8/8/8/6k1/P4q2/7K w - - 0 1");
        seed(b.pos, VALUE_NONE, Value(0));
        (void)search<NonPV>(b.pos, f.ss, mated_in(4)-1, mated_in(4), 2, false);
        result("losing_mate_window_not_reverse_futility", worker.nodes > 0,
               "nodes=" + std::to_string(worker.nodes) + "; tests safeguard, not complete mate proof");
    }
    {
        SearchFixture f; BoardFixture b("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
        const Value v = search<NonPV>(b.pos, f.ss, Value(-1), Value(0), MAX_PLY + 10, false);
        bool hit; auto* e = TT.probe(tt_key(b.pos), hit);
        result("extended_depth_does_not_wrap_tt", v == mated_in(1) && hit && e->depth() == MAX_PLY - 1,
               "stored_depth=" + std::to_string(e->depth()));
    }
    for (int mode : {1, 2}) {
        SearchFixture f; BoardFixture b(StartFEN); observation.mode = mode;
        const Move m = UCI::to_move(b.pos, "e2e4");
        // All alternatives have a controlled exact child result. The test
        // exercises real recursion without depending on network preferences.
        for (Move childMove : MoveList<LEGAL>(b.pos)) {
            StateInfo child;
            b.pos.do_move(childMove, child);
            seed(b.pos, Value(0), Value(0), BOUND_EXACT, 20);
            b.pos.undo_move(childMove);
        }
        seed(b.pos, Value(200), Value(150), BOUND_LOWER, 8, m, true);
        (void)search<NonPV>(b.pos, f.ss, Value(209), Value(210), 6, false);
        const bool ok = mode == 1 ? observation.replaced && observation.singular > 0
                                 : observation.doubles > 0 && observation.moves >= 2 && observation.leaks == 0;
        result(mode == 1 ? "tt_decision_survives_slot_replacement" : "extension_budget_does_not_leak_to_siblings", ok,
               "singular=" + std::to_string(observation.singular) + " doubles=" + std::to_string(observation.doubles)
               + " moves=" + std::to_string(observation.moves) + " leaks=" + std::to_string(observation.leaks));
    }
    {
        SearchFixture f; BoardFixture b(StartFEN); observation.mode = 3;
        StateInfo child; b.pos.do_null_move(child);
        seed(b.pos, Value(200), Value(200), BOUND_EXACT, 20);
        b.pos.undo_null_move();
        seed(b.pos, Value(600), Value(600), BOUND_LOWER, 1, MOVE_NONE, true);
        const Value v = search<NonPV>(b.pos, f.ss, Value(-301), Value(-300), 6, false);
        result("stopped_null_child_is_not_a_result", Stopped.load() && v == 0,
               "returned=" + std::to_string(v));
    }
    {
        SearchFixture f; BoardFixture b("6k1/8/8/3q4/4P3/8/8/4K1N1 w - - 0 1");
        observation.mode = 4;
        const Move m = UCI::to_move(b.pos, "e4d5"); StateInfo child;
        b.pos.do_move(m, child); seed(b.pos, Value(-400), Value(-400), BOUND_EXACT, 20);
        b.pos.undo_move(m);
        seed(b.pos, VALUE_NONE, Value(0), BOUND_NONE, DEPTH_NONE, MOVE_NONE, true);
        const Value v = search<NonPV>(b.pos, f.ss, Value(99), Value(100), 6, false);
        bool hit; const TTEntry* e = TT.probe(tt_key(b.pos), hit);
        result("stopped_probcut_does_not_store_bound", Stopped.load() && v == 0 && e->value() == VALUE_NONE,
               "returned=" + std::to_string(v) + " stored=" + std::to_string(e->value()));
    }
    // Observation-only: a quiet TT move changes the qsearch move domain. This
    // candidate intentionally leaves that heuristic unchanged for a later ablation.
    {
        SearchFixture f; BoardFixture b(StartFEN);
        const Move m = UCI::to_move(b.pos, "g1f3");
        seed(b.pos, VALUE_NONE, Value(0), BOUND_NONE, DEPTH_NONE, m);
        (void)qsearch<PV>(b.pos, f.ss, Value(-300), Value(300), 0);
        std::cout << "{\"observation\":\"quiet_tt_qsearch\",\"nodes\":" << worker.nodes << "}\n";
    }
}

// Input is id<TAB>FEN<TAB>space-separated legal moves. The Python runner
// compares every state with python-chess, including the complete legal set.
void rules_file(const char* path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read rules fixtures");
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string id, fen, sequence;
        std::getline(row, id, '\t'); std::getline(row, fen, '\t'); std::getline(row, sequence);
        BoardFixture b(fen.c_str());
        std::istringstream moves(sequence); std::string move; int ply = 0;
        do {
            std::vector<std::string> legal;
            for (Move m : MoveList<LEGAL>(b.pos)) legal.push_back(UCI::move(m));
            std::sort(legal.begin(), legal.end());
            std::cout << "{\"id\":" << std::quoted(id) << ",\"ply\":" << ply
                      << ",\"draw\":" << (b.pos.is_draw(0) ? "true" : "false")
                      << ",\"key\":" << std::quoted(std::to_string(b.pos.key()))
                      << ",\"fen\":" << std::quoted(b.pos.fen()) << ",\"legal\":[";
            for (size_t i = 0; i < legal.size(); ++i)
                std::cout << (i ? "," : "") << std::quoted(legal[i]);
            std::cout << "]}\n";
            if (!(moves >> move)) break;
            b.move(move.c_str()); ++ply;
        } while (true);
    }
}
}  // namespace boring

int main(int argc, char** argv) {
    using namespace boring;
    try {
        if (argc != 2 && argc != 4) throw std::runtime_error("model path [--rules-file FILE] required");
        std::string error;
        if (!ForgeNext::load_model(argv[1], error)) throw std::runtime_error(error);
        Bitboards::init(); Position::init(); Eval::init_psqt(); Search::init();
        Endgames::Enabled = false; TT.resize(1);
        if (argc == 4) {
            if (std::string(argv[2]) != "--rules-file") throw std::runtime_error("unknown mode");
            rules_file(argv[3]); return 0;
        }
        rules_tests(); clock_tests(); direct_search_tests();
        return 0;  // Python requires every candidate test to pass; old failures retained.
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 2;
    }
}
