// Public evaluation inspection. These helpers are never called by search.
#ifndef FORGE_TRACE_IO_H
#define FORGE_TRACE_IO_H
#include <algorithm>
#include <array>
#include <ostream>
#include "bitboard.h"
#include "eval.h"
#include "forge_factor_net.h"
#include "position.h"

namespace boring {
inline std::array<int16_t, 26> wide_trace(const Eval::TraceX& x, Color c) {
    const int values[] = {
        x.kingDanger[c], x.shelterMg[c], x.shelterEg[c], x.safeChecks[c],
        x.kingAttackers[c], x.flankAttack[c], x.flankDefense[c], x.hanging[c],
        x.restricted[c], x.threatSafePawn[c], x.threatPawnPush[c], x.weakQueenProt[c],
        x.passerCount[c], x.passerMaxRank[c], x.passerFree[c], x.spaceBonus[c],
        x.spaceWeight[c], x.connectedPassers[c], x.candidatePassers[c],
        x.rookOnSeventh[c], x.batteries[c], x.hangingMaterial[c],
        x.maxHangingVictim[c], x.blockaderMinor[c], x.bothFlanks[c], x.infiltration[c]};
    std::array<int16_t, 26> result{};
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = int16_t(std::clamp(values[i], -32768, 32767));
    return result;
}

inline void write_trace(std::ostream& out, const Position& pos, Value value,
                        const Eval::Trace& t) {
    out << "tracedump stm " << int(pos.side_to_move()) << " boring " << int(value)
        << " known " << t.knownEndgame << " phase " << pos.phase()
        << " rule50 " << pos.rule50_count() << " scale " << t.scale
        << " npm " << pos.non_pawn_material(WHITE) << ' ' << pos.non_pawn_material(BLACK)
        << " psqt " << t.psqt.mg << ' ' << t.psqt.eg
        << " space " << t.space[WHITE] << ' ' << t.space[BLACK] << " counts";
    for (Color c : {WHITE, BLACK})
        out << ' ' << pos.count<PAWN>(c) << ' ' << pos.count<KNIGHT>(c)
            << ' ' << pos.count<BISHOP>(c) << ' ' << pos.count<ROOK>(c)
            << ' ' << pos.count<QUEEN>(c);
    out << " bishops";
    for (Color c : {WHITE, BLACK})
        out << ' ' << bool(pos.pieces(c, BISHOP) & DARK_SQUARES)
            << ' ' << bool(pos.pieces(c, BISHOP) & ~DARK_SQUARES);
    out << " sections";
    for (int s = 0; s < Eval::TRACE_SECTION_NB; ++s)
        for (Color c : {WHITE, BLACK})
            out << ' ' << t.section[s][c].mg << ' ' << t.section[s][c].eg;
    out << " wide";
    for (Color c : {WHITE, BLACK})
        for (auto v : wide_trace(t.x, c)) out << ' ' << v;
    out << '\n';
    int planes[ForgeFactor::PSQ_MAX_PIECES];
    const int count = ForgeFactor::extract_planes(pos, planes);
    out << "planedump " << count;
    for (int i = 0; i < count; ++i) out << ' ' << planes[i];
    out << std::endl;
}
}  // namespace boring
#endif
