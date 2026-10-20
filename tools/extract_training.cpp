// One ordered pass produces the three aligned inputs used by netB training.
// Linux x86-64 little-endian output; WDL labels are supplied independently.
#include <fstream>
#include <iostream>
#include <limits>
#include "bitboard.h"
#include "endgame.h"
#include "eval_terms.h"
#include "trace_io.h"
#include "record_position.h"
#include "section_record.h"
using namespace boring;

// Position::see_ge references this search option; extraction does not run a
// search, and retains the shipped false default.
namespace boring::Search { bool SpecialMoveSEE = false; }

template<class T> void write(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: forge-extract validated.fens OUTPUT_PREFIX\n";
        return 2;
    }
    Bitboards::init(); Position::init(); Eval::init_psqt(); Endgames::init();
    std::ifstream input(argv[1]);
    const std::string prefix = argv[2];
    std::ofstream sections(prefix + ".sections", std::ios::binary);
    std::ofstream sparse(prefix + ".sparse", std::ios::binary);
    std::ofstream wide(prefix + ".wide", std::ios::binary);
    if (!input || !sections || !sparse || !wide) return 2;
    sparse.write("SPRS", 4);
    for (uint32_t value : {1u, 0u, 32u, 768u}) write(sparse, value);
    uint32_t rows = 0;
    std::string fen;
    while (std::getline(input, fen)) {
        // Reject blank rows so external labels cannot silently become misaligned.
        if (fen.empty() || rows == std::numeric_limits<uint32_t>::max()) return 3;
        Position pos; StateInfo st; pos.set(fen, &st);
        Eval::Trace trace;
        const auto value = Eval::evaluate_trace(pos, trace);
        forge::SectionRecord record{};
        record.context = forge::calibration_record(pos, value);
        auto pack = [](Score s) {
            return forge::PackedScore{forge::clipped<int16_t>(s.mg), forge::clipped<int16_t>(s.eg)};
        };
        record.psqt = pack(trace.psqt);
        for (int s = 0; s < 12; ++s)
            for (Color c : {WHITE, BLACK}) record.section[s][c] = pack(trace.section[s][c]);
        for (Color c : {WHITE, BLACK}) record.space[c] = forge::clipped<int16_t>(trace.space[c]);
        record.scale = forge::clipped<uint8_t>(trace.scale);
        record.flags = trace.knownEndgame ? forge::KnownEndgame : 0;
        write(sections, record);
        int planes[32]; uint16_t indices[32]{};
        const int count = ForgeFactor::extract_planes(pos, planes);
        for (int i = 0; i < count; ++i) indices[i] = uint16_t(planes[i]);
        write(sparse, uint16_t(count)); write(sparse, indices);
        if (!pos.checkers() && !trace.knownEndgame)
            for (Color c : {WHITE, BLACK}) write(wide, wide_trace(trace.x, c));
        if (!sections || !sparse || !wide) return 4;
        ++rows;
    }
    sparse.seekp(8); write(sparse, rows);
    if (!rows || input.bad() || !sparse) return 5;
    std::cout << "extracted " << rows << " ordered rows\n";
}
