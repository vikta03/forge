#ifndef FORGE_FACTOR_RUNTIME_NET_H
#define FORGE_FACTOR_RUNTIME_NET_H

#include <cstdint>
#include <string>

#include "types.h"

namespace boring {

class Position;
namespace Eval {
struct Trace;
}

namespace ForgeFactor {

struct Prediction {
    Value score = VALUE_DRAW;
    Value searchScore = VALUE_DRAW;
    float advantage = 0.0f;
    float decisive = 0.0f;
    float win = 0.0f;
    float draw = 1.0f;
    float loss = 0.0f;
    int route = 0;
};

bool load(const std::string& path, std::string& error);
void clear();
bool ready();
const std::string& path();

// Search-side readout gain, not a model parameter: it multiplies the integer
// score only, leaving the W/D/L probabilities untouched.  Boring's pruning
// margins are literals in Boring's centipawn scale, and FORGE's calibrated
// readout is roughly 3.5x more compressed than that, so every margin is
// effectively that much too wide.  This exists to measure whether closing
// that gap is worth Elo.  1.0 reproduces the audited behaviour exactly.
void set_score_scale(float scale);
float score_scale();

// The probability the most recently evaluated position is decisive -- the
// model's own c, where the draw probability is 1 - c. The engine has always
// computed this and always thrown it away: the readout collapses it into one
// integer before search sees anything. Exposing it lets search know how much
// the evaluation it just received is worth trusting.
float last_decisive();

// Side-to-move-relative piece-square planes: own pieces occupy 0..383, enemy
// 384..767, square index flipped when Black moves. This must reproduce
// src_spatial/features.cpp::extract_plane_indices bit for bit -- that is what
// generated the training data, and it lives in a different codebase. Piece
// order P,N,B,R,Q,K was derived from the data, not from reading either source.
constexpr int PSQ_PLANES = 768;
constexpr int PSQ_MAX_PIECES = 32;
constexpr int PSQ_MAX_KING_BUCKETS = 32;
int extract_planes(const Position& pos, int* out);

// Plane indices depend on the side to move -- both the own/enemy split and the
// square flip -- and that alternates every move, so one accumulator cannot
// survive a do_move. Two perspectives are maintained instead and the right one
// is read at evaluation time, the same shape as an NNUE dual accumulator.
#ifndef FORGE_PSQ_WIDTH
#define FORGE_PSQ_WIDTH 16
#endif
constexpr int PSQ_WIDTH = FORGE_PSQ_WIDTH;
static_assert(PSQ_WIDTH == 8 || PSQ_WIDTH == 16,
              "PSQ_WIDTH must be 8 or 16: the accumulator update loads a"
              " 128-bit row and widens it to one 256-bit register");
struct PsqAccumulator {
    alignas(32) std::int16_t view[2][PSQ_WIDTH]{};   // [WHITE to move][BLACK to move]
    std::uint8_t kingBucket[2]{};  // own-king bucket used by each perspective
};

// coefficient is +1 to place a piece, -1 to remove it.
void psq_update(PsqAccumulator& accumulator, Piece piece, Square square, int coefficient);
void psq_refresh(PsqAccumulator& accumulator, const Position& pos);
// A king move normally updates both perspectives like any other move. If its
// own bucket changed, only that perspective is rebuilt; the opposite view's
// incrementally updated sum remains valid.
void psq_refresh_if_king_bucket_changed(PsqAccumulator& accumulator,
                                        const Position& pos, Color king);
int king_bucket_count();

Prediction predict(const Position& pos, Value boringScore, const Eval::Trace& trace,
                   const PsqAccumulator* accumulator = nullptr);
Value score(const Position& pos, Value boringScore, const Eval::Trace& trace,
            const PsqAccumulator* accumulator = nullptr);

}  // namespace ForgeFactor
}  // namespace boring

#endif
