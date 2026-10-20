#ifndef FORGE_SECTION_RECORD_H
#define FORGE_SECTION_RECORD_H

#include <cstdint>

#include "calibration_record.h"

namespace forge {

#pragma pack(push, 1)
struct PackedScore {
    std::int16_t mg;
    std::int16_t eg;
};

struct SectionRecord {
    CalibrationRecord context;
    PackedScore psqt;
    PackedScore section[12][2];
    std::int16_t space[2];
    std::uint8_t scale;
    std::uint8_t flags;
};
#pragma pack(pop)

enum SectionFlag : std::uint8_t {
    KnownEndgame = 1 << 0,
    ReplayMismatch = 1 << 1,
    EventMismatch = 1 << 2,
};

static_assert(sizeof(SectionRecord) == 122);

}  // namespace forge

#endif
