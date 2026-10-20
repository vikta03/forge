#ifndef FORGE_RECORD_POSITION_H
#define FORGE_RECORD_POSITION_H

#include <algorithm>
#include <cstdint>
#include <limits>

#include "bitboard.h"
#include "eval.h"
#include "position.h"

#include "calibration_record.h"

namespace forge {

template<typename T>
T clipped(int value) {
    return static_cast<T>(std::clamp(
        value, int(std::numeric_limits<T>::min()), int(std::numeric_limits<T>::max())));
}

inline CalibrationRecord calibration_record(
    const boring::Position& pos, int evaluated_score) {
    using namespace boring;
    const Color us = pos.side_to_move();
    const Color them = ~us;

    CalibrationRecord row{};
    row.score_cp = clipped<std::int16_t>(evaluated_score);
    row.non_pawn_delta = clipped<std::int16_t>(
        pos.non_pawn_material(us) - pos.non_pawn_material(them));
    row.phase = clipped<std::uint8_t>(pos.phase());
    row.rule50 = clipped<std::uint8_t>(pos.rule50_count());
    row.own_pawns = clipped<std::uint8_t>(pos.count<PAWN>(us));
    row.enemy_pawns = clipped<std::uint8_t>(pos.count<PAWN>(them));
    row.own_knights = clipped<std::uint8_t>(pos.count<KNIGHT>(us));
    row.enemy_knights = clipped<std::uint8_t>(pos.count<KNIGHT>(them));
    row.own_bishops = clipped<std::uint8_t>(pos.count<BISHOP>(us));
    row.enemy_bishops = clipped<std::uint8_t>(pos.count<BISHOP>(them));
    row.own_rooks = clipped<std::uint8_t>(pos.count<ROOK>(us));
    row.enemy_rooks = clipped<std::uint8_t>(pos.count<ROOK>(them));
    row.queens = std::uint8_t(std::min(pos.count<QUEEN>(us), 15)
                           | (std::min(pos.count<QUEEN>(them), 15) << 4));

    const bool own_dark = bool(pos.pieces(us, BISHOP) & DARK_SQUARES);
    const bool own_light = bool(pos.pieces(us, BISHOP) & ~DARK_SQUARES);
    const bool enemy_dark = bool(pos.pieces(them, BISHOP) & DARK_SQUARES);
    const bool enemy_light = bool(pos.pieces(them, BISHOP) & ~DARK_SQUARES);
    const bool opposite = row.own_bishops == 1 && row.enemy_bishops == 1
                       && (own_dark != enemy_dark);
    const bool pure_bishops = opposite && row.own_knights == 0 && row.enemy_knights == 0
                           && row.own_rooks == 0 && row.enemy_rooks == 0
                           && (row.queens & 0x0f) == 0 && (row.queens >> 4) == 0;
    row.flags = (opposite ? OppositeBishops : 0)
              | (pure_bishops ? PureBishopEnding : 0)
              | (own_dark && own_light ? OwnBishopPair : 0)
              | (enemy_dark && enemy_light ? EnemyBishopPair : 0)
              | (pos.checkers() ? InCheck : 0)
              | (pos.side_to_move() == BLACK ? BlackToMove : 0);
    return row;
}

}  // namespace forge

#endif
