
#ifdef FORGE_NEXT_PROBE
#include "forge_next_probe.h"
#endif
// uci.cpp — the UCI protocol loop and the string conversions it needs.
//
// The loop runs on the main thread and the search runs on a worker, which is
// the whole reason `stop` and `ponderhit` can be honoured at all. An engine
// that searches inside its own read loop cannot answer anything until it is
// finished, no matter what the protocol says.

#include "uci.h"

#include <deque>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "bench.h"
#include "endgame.h"
#include "eval.h"
#include "movegen.h"
#include "movepick.h"
#include "position.h"
#include "search.h"
#include "timeman.h"
#include "tt.h"

namespace boring {

namespace {

// The game so far. The state list must outlive the position, since Position
// holds a pointer into it, and must never reallocate — hence deque.
Position              RootPos;
std::deque<StateInfo> States;

int HashMB = DEFAULT_HASH_MB;

void set_position(const std::string& fen, std::istringstream& is) {
    States.clear();
    States.emplace_back();
    RootPos.set(fen, &States.back(), UCI::Chess960);

    std::string token;
    while (is >> token) {
        const Move m = UCI::to_move(RootPos, token);
        if (m == MOVE_NONE)
            break;

        States.emplace_back();
        RootPos.do_move(m, States.back());
    }
}

void cmd_position(std::istringstream& is) {
    std::string token, fen;

    is >> token;

    if (token == "startpos") {
        fen = StartFEN;
        is >> token;  // consume the "moves" token if present
    } else if (token == "fen") {
        // Read FEN fields until the "moves" keyword or the end of the line.
        //
        // Deliberately not "consume exactly six tokens": four- and five-field
        // FENs are common, and a fixed count swallows the "moves" keyword and
        // silently drops the whole move list.
        while (is >> token && token != "moves")
            fen += token + " ";
    } else
        return;

    set_position(fen, is);
}

void cmd_go(std::istringstream& is) {
    if (!ForgeNext::loaded()) {
        std::cout << "info string error FORGE 2 model is not loaded\n"
                  << "bestmove 0000" << std::endl;
        return;
    }
    LimitsType  limits;
    std::string token;

    limits.startTime = now();

    while (is >> token) {
        if (token == "searchmoves") {
            while (is >> token) {
                const Move m = UCI::to_move(RootPos, token);
                if (m == MOVE_NONE)
                    break;
                limits.searchmoves.push_back(m);
            }
            if (!is)
                break;
        }

        if (token == "wtime")
            is >> limits.time[WHITE];
        else if (token == "btime")
            is >> limits.time[BLACK];
        else if (token == "winc")
            is >> limits.inc[WHITE];
        else if (token == "binc")
            is >> limits.inc[BLACK];
        else if (token == "movestogo")
            is >> limits.movestogo;
        else if (token == "depth")
            is >> limits.depth;
        else if (token == "nodes")
            is >> limits.nodes;
        else if (token == "movetime")
            is >> limits.movetime;
        else if (token == "mate")
            is >> limits.mate;
        else if (token == "infinite")
            limits.infinite = true;
        else if (token == "ponder")
            limits.ponder = true;
        else if (token == "perft") {
            Search::stop();
            int depth = 1;
            is >> depth;
            const uint64_t nodes = perft<true>(RootPos, depth);
            std::cout << "\nNodes searched: " << nodes << std::endl;
            return;
        }
    }

    Search::start(RootPos, limits);
}

void cmd_setoption(std::istringstream& is) {
    // Configuration must never race the search worker or replace a live model.
    Search::stop();
    std::string token, name, value;
    is >> token;
    if (token != "name") throw std::runtime_error("setoption requires name");
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;
    if (token == "value") {
        std::getline(is, value);
        if (!value.empty() && value.front() == ' ') value.erase(0, 1);
    }
    auto integer = [&](int low, int high) {
        std::size_t used = 0;
        const int n = std::stoi(value, &used);
        if (used != value.size() || n < low || n > high)
            throw std::runtime_error(name + " must be " + std::to_string(low) + ".." + std::to_string(high));
        return n;
    };
    auto boolean = [&]() {
        if (value != "true" && value != "false") throw std::runtime_error(name + " must be true or false");
        return value == "true";
    };
    if (name == "NetPath" || name == "Forge2File") {
        std::string error;
        if (!ForgeNext::load_model(value, error)) {
            std::cout << "info string error FORGE2 " << error << "; previous model retained" << std::endl;
            return;
        }
        Search::clear();
        ForgeNext::reset(RootPos);
        std::cout << "info string FORGE2 loaded " << ForgeNext::model_path() << std::endl;
    } else if (name == "QuietPolicy") {
        ForgeNext::QuietPolicy = boolean(); Search::clear();
    } else if (name == "QuietPolicyWeight") {
        ForgeNext::QuietPolicyWeight = integer(0,8192); Search::clear();
    } else if (name == "QuietPolicyMinDepth") {
        ForgeNext::QuietPolicyMinDepth = integer(1,32); Search::clear();
    } else if (name == "Hash") {
        HashMB = integer(1,32768); TT.resize(std::size_t(HashMB));
    } else if (name == "Threads") {
        (void) integer(1,1);
    } else if (name == "Clear Hash") {
        Search::clear();
    } else if (name == "MultiPV") {
        Search::MultiPV = integer(1,256);
    } else if (name == "Move Overhead") {
        Search::MoveOverhead = integer(0,5000);
    } else if (name == "Ponder") {
        Search::Ponder.store(boolean(),std::memory_order_relaxed);
    } else if (name == "UCI_Chess960") {
        UCI::Chess960 = boolean();
    } else if (name == "DebugStats") {
        ForgeNext::set_telemetry(boolean());
    } else if (name == "EvaluationAudit" || name == "ProbeAudit") {
        ForgeNext::set_audit(boolean());
    } else if (name == "AdaptiveTime") {
        Search::AdaptiveTime = boolean();
    } else if (name == "TimeManagerTelemetry") {
        Search::TimeManagerTelemetry = boolean();
    } else if (name == "CorrHistTelemetry") {
        Search::CorrHistTelemetry = boolean();
    } else if (name == "SpecialMoveSEE") {
        Search::SpecialMoveSEE = boolean(); Search::clear();
    } else if (name == "CorrHistMode") {
        if (value == "legacy") Search::CorrHistMode = Search::CORR_LEGACY;
        else if (value == "piece") Search::CorrHistMode = Search::CORR_PIECE;
        else if (value == "full") Search::CorrHistMode = Search::CORR_FULL;
        else throw std::runtime_error("invalid CorrHistMode");
        Search::clear();
    } else {
        struct Spin { const char* name; int* field; int low; int high; };
        const Spin options[] = {
            {"AspDelta", &Search::AspDelta,4,400},
            {"ProbCutSeeScale", &Search::ProbCutSeeScale,1,8},
            {"CorrHistScale", &Search::CorrHistScale,0,200},
            {"Rule50TTThreshold", &Search::Rule50TTThreshold,0,101},
            {"PruneScale", &Search::PruneScale,5,400},
            {"RfpScale", &Search::RfpScale,5,400},
            {"RazorScale", &Search::RazorScale,5,400},
            {"NmpScale", &Search::NmpScale,5,400},
            {"ProbCutScale", &Search::ProbCutScale,5,400},
            {"FutScale", &Search::FutScale,5,400},
            {"QDeltaScale", &Search::QDeltaScale,5,400}
        };
        for (const auto& option : options) if (name == option.name) {
            *option.field = integer(option.low,option.high); Search::clear(); return;
        }
        throw std::runtime_error("unknown option " + name);
    }
}

}  // namespace

// ------------------------------------------------------------ conversions

std::string UCI::square(Square s) {
    return std::string{char('a' + file_of(s)), char('1' + rank_of(s))};
}

bool UCI::Chess960 = false;

std::string UCI::move(Move m) {
    if (m == MOVE_NONE)
        return "(none)";
    if (m == MOVE_NULL)
        return "0000";

    Square from = from_sq(m);
    Square to   = to_sq(m);

    // Castling is held internally as king-takes-own-rook. Standard chess wants
    // the king's destination instead, which is unambiguous there.
    if (type_of(m) == CASTLING && !Chess960)
        to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));

    std::string s = square(from) + square(to);

    if (type_of(m) == PROMOTION)
        s += " pnbrqk"[promotion_type(m)];

    return s;
}

// Matched against the legal move list rather than decoded, so castling and
// promotion suffixes need no special handling, and a malformed or illegal
// string simply finds nothing.
Move UCI::to_move(const Position& pos, const std::string& str) {
    for (const ExtMove& m : MoveList<LEGAL>(pos))
        if (str == move(m))
            return m;

    // Some GUIs send castling in the other notation regardless of the option.
    // Try it, but only after the configured form has failed, so that a genuine
    // king move is never mistaken for a castle.
    const bool saved = Chess960;
    Chess960         = !saved;

    Move found = MOVE_NONE;
    for (const ExtMove& m : MoveList<LEGAL>(pos))
        if (type_of(Move(m)) == CASTLING && str == move(m)) {
            found = m;
            break;
        }

    Chess960 = saved;
    return found;
}

std::string UCI::value(Value v) {
    std::ostringstream ss;

    if (!is_decisive(v))
        ss << "cp " << v;
    else if (v > 0)
        // Mate in N *moves*, rounded up from the ply count.
        ss << "mate " << (VALUE_MATE - v + 1) / 2;
    else
        ss << "mate " << -(VALUE_MATE + v) / 2;

    return ss.str();
}

// ------------------------------------------------------------------- loop

void UCI::loop() {
    States.emplace_back();
    RootPos.set(StartFEN, &States.back(), UCI::Chess960);

    std::string line;

    while (std::getline(std::cin, line)) {
        std::istringstream is(line);
        std::string        token;

        is >> std::skipws >> token;

        if (token == "quit") {
            Search::stop();
            break;
        }

        if (token == "stop")
            Search::stop();

        else if (token == "uci") {
            std::cout << "id name FORGE Wide192 Reader64 (20261025)\n"
                      << "id author Remmy Victor (@vikta03)\n"
                      << "option name NetPath type string default " << ForgeNext::model_path() << "\n"
                      << "option name QuietPolicy type check default true\n"
                      << "option name QuietPolicyWeight type spin default 1024 min 0 max 8192\n"
                      << "option name QuietPolicyMinDepth type spin default 4 min 1 max 32\n"
                      << "option name AspDelta type spin default 6 min 4 max 400\n"
                      << "option name ProbCutSeeScale type spin default 4 min 1 max 8\n"
                      << "option name SpecialMoveSEE type check default false\n"
                      << "option name CorrHistScale type spin default 25 min 0 max 200\n"
                      << "option name CorrHistMode type combo default legacy var legacy var piece var full\n"
                      << "option name CorrHistTelemetry type check default false\n"
                      << "option name Rule50TTThreshold type spin default 80 min 0 max 101\n"
                      << "option name PruneScale type spin default 25 min 5 max 400\n"
                      << "option name RfpScale type spin default 100 min 5 max 400\n"
                      << "option name RazorScale type spin default 100 min 5 max 400\n"
                      << "option name NmpScale type spin default 100 min 5 max 400\n"
                      << "option name ProbCutScale type spin default 100 min 5 max 400\n"
                      << "option name FutScale type spin default 100 min 5 max 400\n"
                      << "option name QDeltaScale type spin default 100 min 5 max 400\n"
                      << "option name Hash type spin default 64 min 1 max 32768\n"
                      << "option name Clear Hash type button\n"
                      << "option name Move Overhead type spin default 10 min 0 max 5000\n"
                      << "option name MultiPV type spin default 1 min 1 max 256\n"
                      << "option name Ponder type check default false\n"
                      << "option name AdaptiveTime type check default false\n"
                      << "option name TimeManagerTelemetry type check default false\n"
                      << "option name UCI_Chess960 type check default false\n"

#ifdef FORGE_NEXT_PROBE
                      << "option name Threads type spin default 1 min 1 max 1\n"
                      << "option name DebugStats type check default false\n"
                      << "option name EvaluationAudit type check default false\n"
#endif
                      << "uciok" << std::endl;
        }

        else if (token == "isready")
            std::cout << "readyok" << std::endl;

        else if (token == "ucinewgame") {
            Search::stop(); Search::clear();
        }

        else if (token == "setoption") {
            try { cmd_setoption(is); }
            catch (const std::exception& e) {
                std::cout << "info string error " << e.what() << std::endl;
            }
        }

        else if (token == "position") {
            Search::stop();
            cmd_position(is);
        }

        else if (token == "go") {
            Search::stop(); cmd_go(is);
        }

        else if (token == "ponderhit")
            // The pondered move was actually played, so this turns into an
            // ordinary search against the clock that was already allocated.
            Search::ponderhit();

        // --- non-standard, for debugging ---

        else if (token == "d") {
            Search::stop(); std::cout << RootPos << std::endl;
        }

#ifdef FORGE_CACHE_AUDIT
        else if (token == "cacheaudit") {
            Search::stop();
            std::cout << "cacheaudit " << Search::EvalCacheAudit[0][0] << ' '
                      << Search::EvalCacheAudit[0][1] << ' '
                      << Search::EvalCacheAudit[1][0] << ' '
                      << Search::EvalCacheAudit[1][1] << std::endl;
        }
#endif


#ifdef FORGE_NEXT_PROBE
        else if (token == "forge2cost") {
            Search::stop(); unsigned count = 20000; is >> count;
            ForgeNext::cost(RootPos, count);
        }
        else if (token == "forge2inspect") {
            Search::stop(); ForgeNext::inspect(RootPos);
        }
        else if (token == "forge2order") {
            Search::stop();
            int depth = 4;
            is >> depth;
            if (depth < 0 || depth > 32)
                std::cout << "info string error forge2order depth must be 0..32" << std::endl;
            else MovePicker::inspect_quiets(RootPos, depth);
        }
        else if (token == "forge2selftest") {
            Search::stop();
            if (!ForgeNext::self_test())
                std::cout << "forge2-selftest {\"passed\":false}" << std::endl;
            Search::clear();
        }
#endif

        else if (token == "eval") {
            Search::stop();
            if (RootPos.checkers())
                std::cout << "info string error static evaluation is undefined in check" << std::endl;
            else
                std::cout << "static eval: " << Eval::evaluate(RootPos)
                          << " (side to move)" << std::endl;
        }

        else if (token == "perft") {
            Search::stop();
            int depth = 1;
            is >> depth;
            std::cout << "\nNodes searched: " << perft<true>(RootPos, depth) << std::endl;
        }

        else if (token == "perftsuite") { Search::stop(); perft_suite(); }

        else if (token == "test") { Search::stop(); run_tests(); }

        else if (token == "bench") { Search::stop(); bench(); }

        else if (!token.empty())
            std::cout << "unknown command: " << token << std::endl;
    }

    Search::stop();
}

}  // namespace boring
