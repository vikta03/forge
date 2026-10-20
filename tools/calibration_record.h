#ifndef FORGE_CALIBRATION_RECORD_H
#define FORGE_CALIBRATION_RECORD_H

#include <cstdint>

namespace forge {

// Every count is stored relative to side to move.  Fields used by the
// convertibility path are subsequently reduced to symmetric totals, while the
// advantage path may retain signed differences.
struct CalibrationRecord {
    std::int16_t score_cp;
    std::int16_t non_pawn_delta;
    std::uint8_t phase;
    std::uint8_t rule50;
    std::uint8_t own_pawns;
    std::uint8_t enemy_pawns;
    std::uint8_t own_knights;
    std::uint8_t enemy_knights;
    std::uint8_t own_bishops;
    std::uint8_t enemy_bishops;
    std::uint8_t own_rooks;
    std::uint8_t enemy_rooks;
    std::uint8_t queens;  // low nibble ours, high nibble theirs
    std::uint8_t flags;
};

static_assert(sizeof(CalibrationRecord) == 16);

enum Flag : std::uint8_t {
    OppositeBishops = 1 << 0,
    PureBishopEnding = 1 << 1,
    OwnBishopPair = 1 << 2,
    EnemyBishopPair = 1 << 3,
    InCheck = 1 << 4,
    BlackToMove = 1 << 5,
};

}  // namespace forge

#endif
