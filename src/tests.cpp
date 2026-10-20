// tests.cpp — the engine's self-checks.
//
// Everything here is compiled in: no test data files, no framework, no network.
// `boring test` is the whole correctness net, and since the evaluation is hand
// written rather than fitted to data, the symmetry check at the bottom is the
// most valuable thing in this file.

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "bench.h"
#include "bitboard.h"
#include "eval.h"
#include "eval_terms.h"
#include "movegen.h"
#include "pawns.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "uci.h"

namespace boring {

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cout << "  FAIL  " << what << '\n';
    }
}

// A complete, comparable snapshot of everything a position exposes. Used to
// prove that undo_move() really is the exact inverse of do_move().
struct Snapshot {
    Bitboard byType[PIECE_TYPE_NB];
    Bitboard byColor[COLOR_NB];
    Piece    board[SQUARE_NB];
    Score    psq;
    int      phase;
    Key      key, pawnKey, materialKey;
    Square   ep;
    int      rule50, castling;
    Color    stm;
    Value    npm[COLOR_NB];

    explicit Snapshot(const Position& pos) {
        for (PieceType pt = ALL_PIECES; pt < PIECE_TYPE_NB; ++pt)
            byType[pt] = pos.pieces(pt);
        for (Color c : {WHITE, BLACK}) {
            byColor[c] = pos.pieces(c);
            npm[c]     = pos.non_pawn_material(c);
        }
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            board[s] = pos.piece_on(s);

        psq         = pos.psq_score();
        phase       = pos.phase();
        key         = pos.key();
        pawnKey     = pos.pawn_key();
        materialKey = pos.material_key();
        ep          = pos.ep_square();
        rule50      = pos.rule50_count();
        castling    = pos.castling_rights(WHITE) | pos.castling_rights(BLACK);
        stm         = pos.side_to_move();
    }

    bool operator==(const Snapshot& o) const {
        for (PieceType pt = ALL_PIECES; pt < PIECE_TYPE_NB; ++pt)
            if (byType[pt] != o.byType[pt])
                return false;
        for (Color c : {WHITE, BLACK})
            if (byColor[c] != o.byColor[c] || npm[c] != o.npm[c])
                return false;
        for (Square s = SQ_A1; s <= SQ_H8; ++s)
            if (board[s] != o.board[s])
                return false;

        return psq == o.psq && phase == o.phase && key == o.key && pawnKey == o.pawnKey
            && materialKey == o.materialKey && ep == o.ep && rule50 == o.rule50
            && castling == o.castling && stm == o.stm;
    }
};

// Mirror a position vertically and swap the colours. The result is the same
// position seen from the other side, so its evaluation must come out identical
// — the evaluation is always reported from the side to move's point of view,
// and flipping swaps who that is.
std::string flip_fen(const std::string& fen) {
    std::istringstream ss(fen);
    std::string        placement, stm, castling, ep, half = "0", full = "1";

    ss >> placement >> stm >> castling >> ep;
    ss >> half >> full;

    std::vector<std::string> ranks;
    std::string              cur;
    for (char c : placement) {
        if (c == '/') {
            ranks.push_back(cur);
            cur.clear();
        } else
            cur += c;
    }
    ranks.push_back(cur);
    std::reverse(ranks.begin(), ranks.end());

    std::string out;
    for (size_t i = 0; i < ranks.size(); ++i) {
        if (i)
            out += '/';
        for (char c : ranks[i])
            out += char(std::islower(uint8_t(c))   ? std::toupper(uint8_t(c))
                        : std::isupper(uint8_t(c)) ? std::tolower(uint8_t(c))
                                                   : c);
    }

    out += ' ';
    out += (stm == "w" ? 'b' : 'w');
    out += ' ';

    std::string flipped;
    for (char c : castling)
        flipped += char(std::islower(uint8_t(c))   ? std::toupper(uint8_t(c))
                        : std::isupper(uint8_t(c)) ? std::tolower(uint8_t(c))
                                                   : c);
    out += flipped.empty() ? "-" : flipped;
    out += ' ';

    // Rank 3 and rank 6 are each other's mirror.
    if (ep == "-" || ep.size() < 2)
        out += '-';
    else {
        out += ep[0];
        out += char('1' + ('8' - ep[1]));
    }

    out += ' ' + half + ' ' + full;
    return out;
}

// Positions used by several tests below. Deliberately varied: castling rights,
// en passant, passed pawns, opposite bishops, lone kings, king attacks.
const char* TEST_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
    "4rrk1/pp1n1pp1/1q2p2p/1P1bP3/3P4/P2NBP2/2Q2P1P/R4RK1 w - - 0 21",
    "6k1/6p1/6Pp/ppp5/3pP3/2P1P1P1/PP5P/K7 w - - 0 1",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
    "2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1",
    "8/8/8/8/8/6k1/6p1/6K1 w - - 0 1",
    "8/2k5/8/8/8/8/5PPP/6K1 w - - 0 1",
    "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
    "8/5k2/3b4/8/8/3B4/2K5/8 w - - 0 1",
    "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
    "r2q1rk1/pb1nbppp/1p2pn2/2pp4/2PP4/1PN1PN2/PB3PPP/R2QKB1R w KQ - 0 10",
};

// ------------------------------------------------------------------ tests

void test_make_unmake_impl(Position& pos, int depth);

// Chess960 positions, in Shredder notation (the rook files named explicitly).
const char* TEST_FENS_960[] = {
    "rnqnbbkr/pppppppp/8/8/8/8/PPPPPPPP/RNQNBBKR w HAha - 0 1",
    "rknnqbbr/pppppppp/8/8/8/8/PPPPPPPP/RKNNQBBR w HAha - 0 1",
    "qnnrbbkr/pppppppp/8/8/8/8/PPPPPPPP/QNNRBBKR w HDhd - 0 1",
    "rkbnrqnb/pppppppp/8/8/8/8/PPPPPPPP/RKBNRQNB w EAea - 0 1",
    "bnqbrkrn/pppppppp/8/8/8/8/PPPPPPPP/BNQBRKRN w GEge - 0 1",
    "rknn1bbr/ppp1p1pp/1q6/3p1p2/8/1PP5/PN1PPPPP/RK1NQBBR w HAha - 3 5",
};

// Castling is stored as king-takes-own-rook and printed either way. Both
// notations must parse back to exactly the same move, or a GUI speaking the
// other dialect would silently desynchronise from the engine.
void test_castling_notation() {
    Position  pos;
    StateInfo st;

    for (const char* fen : TEST_FENS_960) {
        pos.set(fen, &st, true);

        // Shredder FEN must survive a round trip in 960 mode.
        check(pos.fen() == std::string(fen),
              std::string("960 FEN round-trip: ") + fen + "\n         got: " + pos.fen());

        for (const ExtMove& em : MoveList<LEGAL>(pos)) {
            const Move m = em;
            if (type_of(m) != CASTLING)
                continue;

            UCI::Chess960             = true;
            const std::string asRook  = UCI::move(m);
            UCI::Chess960             = false;
            const std::string asKing  = UCI::move(m);
            UCI::Chess960             = true;

            // King-takes-rook names the rook's real square...
            check(asRook == UCI::square(from_sq(m)) + UCI::square(to_sq(m)),
                  "960 castling should print as king-takes-rook: " + asRook);

            // ...while the standard form always names g-file or c-file.
            const File kf = file_of(to_sq(UCI::to_move(pos, asKing)));
            (void) kf;
            check(asKing[2] == 'g' || asKing[2] == 'c',
                  "standard castling notation should name g or c: " + asKing);

            // Both spellings must resolve to the same move.
            check(UCI::to_move(pos, asRook) == m, "king-takes-rook did not parse back: " + asRook);
            check(UCI::to_move(pos, asKing) == m, "king destination did not parse back: " + asKing);
        }

        UCI::Chess960 = false;
    }
}

// The 960 positions must obey the same invariants as the standard ones.
void test_960_state() {
    Position  pos;
    StateInfo st;

    for (const char* fen : TEST_FENS_960) {
        pos.set(fen, &st, true);
        test_make_unmake_impl(pos, 3);
    }
}

void test_fen_roundtrip() {
    Position  pos;
    StateInfo st;

    for (const char* fen : TEST_FENS) {
        pos.set(fen, &st);
        check(pos.fen() == std::string(fen),
              std::string("FEN round-trip: ") + fen + "\n         got: " + pos.fen());
    }
}

// Walk a game tree and confirm that undo_move() restores every single piece of
// state exactly, including the incrementally maintained score, phase, and keys.
void test_make_unmake_impl(Position& pos, int depth) {
    if (depth == 0)
        return;

    for (const ExtMove& m : MoveList<LEGAL>(pos)) {
        const Snapshot before(pos);

        StateInfo st;
        pos.do_move(m, st);
        test_make_unmake_impl(pos, depth - 1);
        pos.undo_move(m);

        if (!(Snapshot(pos) == before))
            check(false, "make/unmake is not an exact inverse for " + UCI::move(m) + " in "
                             + pos.fen());
    }
}

struct SeeCase {
    const char* fen;
    const char* move;
    int         value;  // the exact SEE, in SIMPLE_PIECE_VALUE units
    const char* note;
};

void test_see() {
    const bool oldSpecialMoveSEE = Search::SpecialMoveSEE;
    Search::SpecialMoveSEE = true;
    // SIMPLE_PIECE_VALUE is {P=100, N=350, B=370, R=550, Q=1100}.
    const SeeCase cases[] = {
        {"4k3/8/8/3p4/8/8/8/3RK3 w - - 0 1", "d1d5", 100, "rook takes an undefended pawn"},

        {"4k3/8/2p5/3p4/8/8/8/3RK3 w - - 0 1", "d1d5", 100 - 550,
         "rook takes a pawn defended by a pawn"},

        {"4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1", "d1d5", 350, "rook takes an undefended knight"},

        {"4k3/8/2p5/3n4/8/8/8/3RK3 w - - 0 1", "d1d5", 350 - 550,
         "rook takes a knight defended by a pawn"},

        // The bishop must actually bear on d5 for the third capture to exist;
        // b3-c4-d5 does, f1 does not.
        {"4k3/8/2p5/3n4/8/1B6/8/3RK3 w - - 0 1", "d1d5", 350 - 550 + 100,
         "rook, pawn, then bishop: the full three-capture sequence"},

        {"4k3/2r5/8/3p4/8/8/3R4/3RK3 w - - 0 1", "d2d5", 100,
         "doubled rooks win the pawn through the x-ray"},

        {"4k3/8/8/8/8/8/4P3/4K3 w - - 0 1", "e2e4", 0, "a quiet move exchanges nothing"},
        {"k7/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7e8q", 1000,
         "quiet queen promotion counts pawn-to-queen gain"},
        {"k2r4/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7d8q", 1550,
         "capture promotion counts victim and promotion gain"},
        {"k7/4P3/8/8/8/8/8/4K3 w - - 0 1", "e7e8n", 250,
         "underpromotion uses the promoted occupant value"},
        {"k2r4/4Pn2/8/8/8/8/8/4K3 w - - 0 1", "e7d8q", 450,
         "a defender recaptures the promoted queen, not a pawn"},
        {"7k/8/8/3pP3/3r4/8/8/4K3 w - d6 0 1", "e5d6", 0,
         "en passant removal opens the victim-square rook x-ray"},
        {"7k/5n2/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 0,
         "en passant can lose the arriving pawn to a defender"},
    };

    Position  pos;
    StateInfo st;

    for (const SeeCase& c : cases) {
        pos.set(c.fen, &st);
        const Move m = UCI::to_move(pos, c.move);

        if (m == MOVE_NONE) {
            check(false, std::string("SEE case has an illegal move: ") + c.move);
            continue;
        }

        // see_ge is a threshold test, so pin the exact value by bracketing it.
        check(pos.see_ge(m, c.value),
              std::string("SEE(") + c.move + ") >= " + std::to_string(c.value) + " — " + c.note);
        check(!pos.see_ge(m, c.value + 1),
              std::string("SEE(") + c.move + ") < " + std::to_string(c.value + 1) + " — "
                  + c.note);
    }

    Search::SpecialMoveSEE = oldSpecialMoveSEE;
}

// The heart of the correctness net for a hand-written evaluation.
//
// A term that reads an absolute rank instead of a relative one, or that adds
// where it should subtract for Black, produces an evaluation that quietly
// favours one colour everywhere. Nothing else in the project would catch that;
// this does, immediately.
void test_eval_symmetry() {
    Position  pos, flipped;
    StateInfo st1, st2;

    for (const char* fen : TEST_FENS) {
        const std::string mirror = flip_fen(fen);

        pos.set(fen, &st1);
        flipped.set(mirror, &st2);

        if (pos.checkers() || flipped.checkers())
            continue;  // no static evaluation exists in check

        const Value a = Eval::evaluate(pos);
        const Value b = Eval::evaluate(flipped);

        check(a == b, "evaluation is not colour-symmetric: " + std::to_string(a) + " vs "
                          + std::to_string(b) + "\n         " + fen + "\n         " + mirror);
    }
}

// The evaluation must not depend on whether the pawn cache happened to be warm.
void test_pawn_cache_consistency() {
    Position  pos;
    StateInfo st;

    for (const char* fen : TEST_FENS) {
        pos.set(fen, &st);
        if (pos.checkers())
            continue;

        Pawns::clear();
        const Value cold = Eval::evaluate(pos);
        const Value warm = Eval::evaluate(pos);

        check(cold == warm, std::string("pawn cache changes the evaluation: ") + fen);
    }
}

}  // namespace

void test_eval_clock_cache() {
    TranspositionTable table;
    table.resize(1);
    check(table.storage_bytes() <= 1024 * 1024, "Hash includes clock tags");
    check(sizeof(TTEntry) == 10, "compact TT entry unchanged");
    bool hit = true;
    const Key key = 0x1234567812345678ULL;
    TTEntry* entry = table.probe(key, hit);
    check(!hit && table.eval_rule50(entry) == 255, "new slot has no evaluation clock");
    check(table.save(entry, key, Value(42), false, BOUND_EXACT, 20, MOVE_NONE,
                     Value(13), table.generation(), 3), "initial payload stored");
    check(table.eval_rule50(entry) == 3 && entry->eval() == Value(13), "clock follows evaluation");
    check(!table.save(entry, key, Value(99), false, BOUND_LOWER, 1, MOVE_NONE,
                      Value(66), table.generation(), 17), "shallow write rejected");
    check(table.eval_rule50(entry) == 3 && entry->eval() == Value(13),
          "rejected write preserves original clock and evaluation");
    auto* same = table.probe(key, hit);
    check(hit && same == entry && table.eval_rule50(same) == 3, "same-clock reuse");
    table.store_eval(entry, Value(24), 17);
    check(entry->eval() == Value(24) && table.eval_rule50(entry) == 17,
          "different-clock static evaluation replaced");
    check(entry->value() == Value(42) && entry->depth() == 20
          && entry->bound() == BOUND_EXACT, "static refresh preserves search bound");
    table.save(entry, key + 1, Value(11), false, BOUND_LOWER, 2, MOVE_NONE,
               Value(9), table.generation(), 61);
    check(entry->eval() == Value(9) && table.eval_rule50(entry) == 61,
          "replacement clock follows new key");
    table.save(entry, key + 1, Value(8), false, BOUND_EXACT, 3, MOVE_NONE,
               VALUE_NONE, table.generation(), 70);
    check(table.eval_rule50(entry) == 255, "missing static evaluation invalidates tag");
    for (int clock : {0, 79, 80, 100, 101, 254, 255, 300}) {
        table.store_eval(entry, Value(8), clock);
        check(table.eval_rule50(entry) == (clock < 255 ? clock : 255),
              "clock represented exactly or marked uncacheable");
    }
    // Exercise every slot across many clusters and retain the value/tag pairing.
    for (int i = 1; i <= 1024; ++i) {
        const Key k = Key(i) * 0x9e3779b97f4a7c15ULL;
        TTEntry* p = table.probe(k, hit);
        table.save(p, k, Value(0), false, BOUND_EXACT, 1, MOVE_NONE,
                   Value(i), table.generation(), i % 100);
        check(table.eval_rule50(p) == i % 100 && p->eval() == Value(i), "tag slot indexing");
    }
    table.clear();
    entry = table.probe(key, hit);
    check(!hit && table.eval_rule50(entry) == 255, "clear invalidates tags");
    table.resize(3);
    check(table.storage_bytes() <= 3 * 1024 * 1024, "resize includes clock storage");
    entry = table.probe(key, hit);
    check(!hit && table.eval_rule50(entry) == 255, "resize invalidates tags");
    table.resize(1);
    check(table.storage_bytes() <= 1024 * 1024, "shrink releases tag allocation");
    Position a, b;
    StateInfo sa, sb;
    a.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", &sa);
    b.set("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 19 1", &sb);
    check(a.key() == b.key() && a.rule50_count() != b.rule50_count(),
          "repetition identity is independent of halfmove clock");
}

bool run_tests() {
    failures = 0;

    std::cout << "FEN round-trip..." << std::endl;
    test_fen_roundtrip();

    std::cout << "static exchange evaluation..." << std::endl;
    test_see();

    std::cout << "make/unmake and incremental state..." << std::endl;
    {
        Position  pos;
        StateInfo st;
        for (const char* fen : TEST_FENS) {
            pos.set(fen, &st);
            test_make_unmake_impl(pos, 3);
        }
    }

    std::cout << "Chess960 castling notation..." << std::endl;
    test_castling_notation();

    std::cout << "Chess960 make/unmake..." << std::endl;
    test_960_state();

    std::cout << "evaluation colour symmetry..." << std::endl;
    test_eval_symmetry();

    std::cout << "pawn cache consistency..." << std::endl;
    test_pawn_cache_consistency();

    std::cout << "static evaluation clock cache..." << std::endl;
    test_eval_clock_cache();

    std::cout << "perft..." << std::endl;
    if (!perft_suite())
        ++failures;

    std::cout << "\n"
              << (failures ? "TESTS FAILED — " + std::to_string(failures) + " problem(s)"
                           : "all tests passed")
              << std::endl;

    return failures == 0;
}

}  // namespace boring
