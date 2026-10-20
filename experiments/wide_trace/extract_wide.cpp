// Dump the widened trace for a corpus of FENs.
//
// Output: one row per KEPT position, 26 quantities x 2 colours, int16, little
// endian. The keep filter reproduces build_bin_cache.py exactly -- skip when the
// side to move is in check, or when Boring answers the position outright -- so
// the rows line up with the existing cache without any join key. RULES.md R2, R5.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bitboard.h"
#include "endgame.h"
#include "eval.h"
#include "eval_terms.h"
#include "pawns.h"
#include "position.h"

using namespace boring;

namespace {

constexpr int QUANTITIES = 26;

const char* const NAMES[QUANTITIES] = {
    "kingDanger", "shelterMg", "shelterEg", "safeChecks", "kingAttackers",
    "flankAttack", "flankDefense", "hanging", "restricted", "threatSafePawn",
    "threatPawnPush", "weakQueenProt", "passerCount", "passerMaxRank", "passerFree",
    "spaceBonus", "spaceWeight",
    "connectedPassers", "candidatePassers", "rookOnSeventh", "batteries",
    "hangingMaterial", "maxHangingVictim", "blockaderMinor", "bothFlanks", "infiltration"};

void gather(const Eval::TraceX& x, Color c, std::int16_t* out) {
    const int v[QUANTITIES] = {
        x.kingDanger[c], x.shelterMg[c], x.shelterEg[c], x.safeChecks[c],
        x.kingAttackers[c], x.flankAttack[c], x.flankDefense[c], x.hanging[c],
        x.restricted[c], x.threatSafePawn[c], x.threatPawnPush[c], x.weakQueenProt[c],
        x.passerCount[c], x.passerMaxRank[c], x.passerFree[c], x.spaceBonus[c],
        x.spaceWeight[c], x.connectedPassers[c], x.candidatePassers[c],
        x.rookOnSeventh[c], x.batteries[c], x.hangingMaterial[c],
        x.maxHangingVictim[c], x.blockaderMinor[c], x.bothFlanks[c], x.infiltration[c]};
    for (int i = 0; i < QUANTITIES; ++i) {
        const int clipped = v[i] > 32767 ? 32767 : (v[i] < -32768 ? -32768 : v[i]);
        out[i] = std::int16_t(clipped);
    }
}

}  // namespace

int main(int argc, char** argv) {
    Bitboards::init();
    Position::init();
    Eval::init_psqt();
    Endgames::init();

    if (argc == 3 && std::strcmp(argv[1], "--show") == 0) {
        Position position; StateInfo state;
        position.set(argv[2], &state);
        if (position.checkers()) { std::printf("in check — skipped\n"); return 0; }
        Eval::Trace trace;
        const Value v = Eval::evaluate_trace(position, trace);
        if (trace.knownEndgame) { std::printf("known ending — skipped\n"); return 0; }
        std::int16_t w[QUANTITIES], b[QUANTITIES];
        gather(trace.x, WHITE, w); gather(trace.x, BLACK, b);
        std::printf("boring %d\n%-20s %8s %8s\n", int(v), "quantity", "White", "Black");
        for (int i = 0; i < QUANTITIES; ++i)
            std::printf("%-20s %8d %8d%s\n", NAMES[i], w[i], b[i], i == 17 ? "" : "");
        return 0;
    }
    if (argc < 3) {
        std::cerr << "usage: extract_wide FENS OUTPUT [ROWS]\n"
                  << "       extract_wide --show FEN\n";
        return 2;
    }
    const std::int64_t wanted = argc >= 4 ? std::atoll(argv[3]) : -1;

    std::ifstream input(argv[1]);
    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!input || !output) { std::cerr << "cannot open\n"; return 2; }

    std::string fen;
    std::int64_t seen = 0, kept = 0, inCheck = 0, knownEnd = 0;
    std::vector<std::int16_t> buffer;
    buffer.reserve(1 << 16);

    while ((wanted < 0 || seen < wanted) && std::getline(input, fen)) {
        if (fen.empty()) continue;
        if (fen.rfind("fen ", 0) == 0) fen = fen.substr(4);
        ++seen;
        Position position; StateInfo state;
        position.set(fen, &state);
        if (position.checkers()) { ++inCheck; continue; }
        Eval::Trace trace;
        Eval::evaluate_trace(position, trace);
        if (trace.knownEndgame) { ++knownEnd; continue; }
        std::int16_t row[2 * QUANTITIES];
        gather(trace.x, WHITE, row);
        gather(trace.x, BLACK, row + QUANTITIES);
        buffer.insert(buffer.end(), row, row + 2 * QUANTITIES);
        ++kept;
        if (buffer.size() >= (1 << 16)) {
            output.write(reinterpret_cast<const char*>(buffer.data()),
                         std::streamsize(buffer.size() * sizeof(std::int16_t)));
            buffer.clear();
        }
    }
    if (!buffer.empty())
        output.write(reinterpret_cast<const char*>(buffer.data()),
                     std::streamsize(buffer.size() * sizeof(std::int16_t)));
    output.close();
    std::fprintf(stderr, "seen %lld  kept %lld  in-check %lld  known-ending %lld\n",
                 (long long) seen, (long long) kept, (long long) inCheck, (long long) knownEnd);
    std::printf("%lld\n", (long long) kept);
    return 0;
}
