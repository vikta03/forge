// uci.cpp — the UCI protocol loop and the string conversions it needs.
//
// The loop runs on the main thread and the search runs on a worker, which is
// the whole reason `stop` and `ponderhit` can be honoured at all. An engine
// that searches inside its own read loop cannot answer anything until it is
// finished, no matter what the protocol says.

#include "uci.h"

#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "bench.h"
#include "endgame.h"
#include "eval.h"
#include "forge_factor_net.h"
#include "movegen.h"
#include "position.h"
#include "search.h"
#include "timeman.h"
#include "tt.h"
#include "trace_io.h"

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
    if (!ForgeFactor::ready()) {
        std::cout << "info string error FORGE NetPath is not loaded\n"
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
    std::string token, name, value;

    is >> token;  // "name"

    // Option names can contain spaces, so accumulate until "value".
    while (is >> token && token != "value")
        name += (name.empty() ? "" : " ") + token;

    while (is >> token)
        value += (value.empty() ? "" : " ") + token;

    if (name == "NetPath") {
        Search::stop();
        std::string error;
        if (ForgeFactor::load(value, error)) {
            // the accumulator holds sums of the old net's embedding rows
            RootPos.refresh_psq_planes();
            Search::clear();
            std::cout << "info string FORGE factor net loaded "
                      << ForgeFactor::path() << std::endl;
        } else
            std::cout << "info string error " << error << std::endl;
    } else if (name == "ScoreScale") {
        // Percent, so a GUI spin can express it. Search-side gain only.
        ForgeFactor::set_score_scale(float(std::stoi(value)) / 100.0f);
        Search::clear();
    } else if (name == "PruneScale") {
        Search::PruneScale = std::stoi(value);
    } else if (name == "RfpScale") {
        Search::RfpScale = std::stoi(value);
    } else if (name == "RazorScale") {
        Search::RazorScale = std::stoi(value);
    } else if (name == "NmpScale") {
        Search::NmpScale = std::stoi(value);
    } else if (name == "ProbCutScale") {
        Search::ProbCutScale = std::stoi(value);
    } else if (name == "FutScale") {
        Search::FutScale = std::stoi(value);
    } else if (name == "QDeltaScale") {
        Search::QDeltaScale = std::stoi(value);
    } else if (name == "Rule50TTThreshold") {
        Search::Rule50TTThreshold = std::stoi(value);
        Search::clear();
    } else if (name == "CorrHistScale") {
        Search::CorrHistScale = std::stoi(value);
        Search::clear();
    } else if (name == "CorrHistMode") {
        if (value == "legacy")
            Search::CorrHistMode = Search::CORR_LEGACY;
        else if (value == "piece")
            Search::CorrHistMode = Search::CORR_PIECE;
        else if (value == "full")
            Search::CorrHistMode = Search::CORR_FULL;
        else {
            std::cout << "info string error invalid CorrHistMode " << value << std::endl;
            return;
        }
        Search::clear();
    } else if (name == "CorrHistTelemetry") {
        Search::CorrHistTelemetry = (value == "true");
    } else if (name == "SpecialMoveSEE") {
        Search::SpecialMoveSEE = (value == "true");
        Search::clear();
    } else if (name == "ProbCutSeeScale") {
        Search::ProbCutSeeScale = std::stoi(value);
    } else if (name == "AspDelta") {
        Search::AspDelta = std::stoi(value);
    } else if (name == "SyzygyPath") {
        Search::stop();
        const int largest = Endgames::init_syzygy(value.c_str());
        if (largest > 0)
            std::cout << "info string Syzygy loaded, up to " << largest
                      << " pieces, from " << value << std::endl;
        else
            std::cout << "info string error no Syzygy tablebases at "
                      << value << std::endl;
        Search::clear();
    } else if (name == "EndgameProbe") {
        Endgames::Enabled = (value == "true");
        Search::clear();
    } else if (name == "Hash") {
        HashMB = std::stoi(value);
        TT.resize(size_t(HashMB));
    } else if (name == "Clear Hash")
        Search::clear();
    else if (name == "Move Overhead")
        Search::MoveOverhead = std::stoi(value);
    else if (name == "MultiPV")
        Search::MultiPV = std::stoi(value);
    else if (name == "Ponder")
        Search::Ponder.store(value == "true", std::memory_order_relaxed);
    else if (name == "AdaptiveTime") {
        Search::stop();
        Search::AdaptiveTime = (value == "true");
    } else if (name == "TimeManagerTelemetry") {
        Search::stop();
        Search::TimeManagerTelemetry = (value == "true");
    } else if (name == "UCI_Chess960")
        UCI::Chess960 = (value == "true");
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
            std::cout << "id name FORGE netB 1.0.0\n"
                      << "id author Remmy Victor (@vikta03)\n"
                      << "option name NetPath type string default " << ForgeFactor::path() << "\n"
                      << "option name ScoreScale type spin default 100 min 25 max 800\n"
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
                      << "option name SyzygyPath type string default \n"
                      << "option name EndgameProbe type check default true\n"
                      << "option name Hash type spin default 64 min 1 max 32768\n"
                      << "option name Clear Hash type button\n"
                      << "option name Move Overhead type spin default 10 min 0 max 5000\n"
                      << "option name MultiPV type spin default 1 min 1 max 256\n"
                      << "option name Ponder type check default false\n"
                      << "option name AdaptiveTime type check default false\n"
                      << "option name TimeManagerTelemetry type check default false\n"
                      << "option name UCI_Chess960 type check default false\n"
                      << "uciok" << std::endl;
        }

        else if (token == "isready")
            std::cout << "readyok" << std::endl;

        else if (token == "ucinewgame") {
            Search::clear();
        }

        else if (token == "setoption")
            cmd_setoption(is);

        else if (token == "position") {
            Search::stop();
            cmd_position(is);
        }

        else if (token == "go")
            cmd_go(is);

        else if (token == "ponderhit")
            // The pondered move was actually played, so this turns into an
            // ordinary search against the clock that was already allocated.
            Search::ponderhit();

        // --- non-standard, for debugging ---

        else if (token == "d")
            std::cout << RootPos << std::endl;

#ifdef FORGE_CACHE_AUDIT
        else if (token == "cacheaudit") {
            Search::wait();
            std::cout << "cacheaudit " << Search::EvalCacheAudit[0][0] << ' '
                      << Search::EvalCacheAudit[0][1] << ' '
                      << Search::EvalCacheAudit[1][0] << ' '
                      << Search::EvalCacheAudit[1][1] << std::endl;
        }
#endif

        else if (token == "eval") {
            if (!ForgeFactor::ready())
                std::cout << "info string error FORGE NetPath is not loaded" << std::endl;
            else
                std::cout << "static eval: " << Eval::evaluate(RootPos)
                          << " (side to move)" << std::endl;
        }

        else if (token == "forgeeval" || token == "forgeinspect") {
            Search::stop();
            if (!ForgeFactor::ready()) {
                std::cout << "info string error FORGE NetPath is not loaded" << std::endl;
            } else if (RootPos.checkers()) {
                std::cout << "info string error no static evaluation while in check" << std::endl;
            } else {
                Eval::Trace trace;
                const Value boringValue = Eval::evaluate_trace(RootPos, trace);
                if (token == "forgeinspect")
                    write_trace(std::cout, RootPos, boringValue, trace);
                if (trace.knownEndgame) {
                    std::cout << "forge exact 1 score " << boringValue << std::endl;
                } else {
                    const ForgeFactor::Prediction prediction = ForgeFactor::predict(
                        RootPos, boringValue, trace, &RootPos.psq_planes());
                    std::cout << std::setprecision(9)
                              << "forge exact 0 boring " << boringValue
                              << " score " << prediction.score
                              << " searchscore " << prediction.searchScore
                              << " advantage " << prediction.advantage
                              << " decisive " << prediction.decisive
                              << " wdl " << prediction.win << ' '
                              << prediction.draw << ' ' << prediction.loss
                              << " route " << prediction.route << std::endl;
                }
            }
        }

        else if (token == "perft") {
            int depth = 1;
            is >> depth;
            std::cout << "\nNodes searched: " << perft<true>(RootPos, depth) << std::endl;
        }

        else if (token == "perftsuite")
            perft_suite();

        else if (token == "test")
            run_tests();

        else if (token == "bench")
            bench();

        else if (!token.empty())
            std::cout << "unknown command: " << token << std::endl;
    }

    Search::stop();
}

}  // namespace boring
