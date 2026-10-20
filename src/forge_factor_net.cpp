#include "forge_factor_net.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <immintrin.h>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "bitboard.h"
#include "eval.h"
#include "position.h"
#ifdef FORGE_SIEGE
#include "forge_siege_net.h"
#endif

namespace boring::ForgeFactor {
namespace {

constexpr char MAGIC[8] = {'F', 'O', 'R', 'G', 'E', 'F', '0', '8'};
// 2: the header now carries a digest of the bin layout. Version 1 files
// are indistinguishable in shape but were trained against different bin
// edges, and loading one produced a silently wrong evaluator.
// 4: the routing rule moved into the file. It used to be this function and a
// numpy function in the trainer, two hand-written copies of one cascade with
// nothing checking that they agreed. A v4 file carries the (phase, pawns)
// tables it was trained under, one per head, so there is only one copy.
// 5: the final four header bytes explicitly carry the own-king bucket count.
// The PSQ embedding becomes bucket x plane x channel; every other tensor and
// both output heads are unchanged. Version 4 remains a strict one-bucket net.
constexpr std::uint32_t LEGACY_VERSION = 4;
constexpr std::uint32_t KING_VERSION = 5;
// 6: the fixed tanh(odd)*sigmoid(even) head and the psq crossgate are
// replaced by a small per-route MLP over the pooled state. A v6 file is a
// v5 file with that head appended; the hidden width rides in flags 8..15.
constexpr std::uint32_t MLP_VERSION = 6;
constexpr std::uint32_t HEADER_BYTES = 64;
constexpr int ROUTES = 8;
// What the extractor produces. The evaluator may consume a subset.
constexpr int FULL_ODD = 34;
constexpr int FULL_EVEN = 69;

#if (defined(FORGE_WIDE_NETA) + defined(FORGE_WIDE_NETB) + defined(FORGE_WIDE_NETC)) > 1
#error "select at most one wide-trace arm"
#endif
#if defined(FORGE_WIDE_NETA) || defined(FORGE_WIDE_NETB) || defined(FORGE_WIDE_NETC)
#define FORGE_WIDE_RUNTIME 1
#ifndef FORGE_MINI
#error "wide-trace nets extend the FORGE-mini feature set"
#endif
constexpr int WIDE_Q = 26;
#if defined(FORGE_WIDE_NETA)
constexpr bool WIDE_KEEP[WIDE_Q] = {
    true, true, true, true, true, true, true, true, true, true, true, true, true,
    true, true, true, true, false, false, false, false, false, false, false, false, false
};
#elif defined(FORGE_WIDE_NETB)
constexpr bool WIDE_KEEP[WIDE_Q] = {
    true, true, true, true, true, true, true, true, true, true, true, true, true,
    true, true, true, true, true, true, true, true, true, true, true, true, true
};
#else
constexpr bool WIDE_KEEP[WIDE_Q] = {
    false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, true, true, true, true, true, true,
    true, true, true
};
#endif
#endif

#if (defined(FORGE_MINI) + defined(FORGE_LEAN) + defined(FORGE_NO_ABSDIFF)) > 1
#error "FORGE_MINI, FORGE_LEAN and FORGE_NO_ABSDIFF each select a feature set; pick one"
#endif

#ifdef FORGE_LEAN
// FORGE-lean pairs with a handcrafted evaluation that skips pieces<>(), which
// is 98.6 ns of its 187.8 and bundles the four piece bonuses with the four
// mobility terms. King safety, threats and passers are kept: per-term pricing
// put them at 43, 34 and 14 ns for aggregate impacts of 144, 79 and 77, against
// 50 for the whole of pieces<>(). Nothing derived from the dropped sections
// exists to read, so those features go with them: 36 features, 72 lookups.
constexpr bool ODD_KEEP[FULL_ODD] = {
    true , true , true , true , true , true , true , true , true , true , true , false,
    false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, true , true , true , true , true , true , true
};
constexpr bool EVEN_KEEP[FULL_EVEN] = {
    true , true , true , true , true , false, false, true , false, true , false, false,
    true , false, true , true , false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, true ,
    true , true , false, true , true , true , false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, true , false,
    false, false, false, true , false, false, false, false, false
};
#elif defined(FORGE_MINI)
// FORGE-mini. Every feature costs one direct lookup and one latent gather, and
// they are not worth the same. Ranked by how much each moves the evaluation on
// the bins real positions occupy, the 54 features left out here hold 1.44% of
// the total contribution variance between them and cost half of all lookups.
// The cut lands almost entirely on the even side: the advantage head keeps 31
// of 34 inputs, while the decisiveness head turns out to need score magnitude,
// phase, rule50, endgame scale, a little material, king safety, threats,
// passers and pawn asymmetry, and nothing else.
constexpr bool ODD_KEEP[FULL_ODD] = {
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , false, false, false, true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true 
};
constexpr bool EVEN_KEEP[FULL_EVEN] = {
    true , true , true , true , true , false, false, true , false, true , false, false,
    true , false, true , true , false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, true ,
    true , true , false, true , true , true , false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, true , false,
    false, false, false, true , false, false, false, false, false
};
#else
constexpr bool ODD_KEEP[FULL_ODD] = {
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true 
};
#ifdef FORGE_NO_ABSDIFF
// The 24 per-section |W-B| features re-encode a quantity the matching odd
// feature already carries as sign+bin. Dropping them removes 48 of the 206
// table lookups; measured cost was +0.000613 validation CE.
constexpr bool EVEN_KEEP[FULL_EVEN] = {
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, false, true , true , true , true , true 
};
#else
constexpr bool EVEN_KEEP[FULL_EVEN] = {
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true , true , true , true ,
    true , true , true , true , true , true , true , true , true 
};
#endif
#endif

constexpr int kept_count(const bool* mask, int size) {
    int total = 0;
    for (int index = 0; index < size; ++index) total += mask[index];
    return total;
}
constexpr int BASE_ODD = kept_count(ODD_KEEP, FULL_ODD);
constexpr int BASE_EVEN = kept_count(EVEN_KEEP, FULL_EVEN);
#ifdef FORGE_WIDE_RUNTIME
constexpr int WIDE = kept_count(WIDE_KEEP, WIDE_Q);
#else
constexpr int WIDE = 0;
#endif
constexpr int ODD = BASE_ODD + WIDE;
constexpr int EVEN = BASE_EVEN + WIDE;
constexpr int BINS = 129;
// phase 0..24 x total pawns 0..16, the router's input space
constexpr int ROUTE_PHASES = 25;
constexpr int ROUTE_PAWNS = 17;
constexpr int ROUTE_CELLS = ROUTE_PHASES * ROUTE_PAWNS;
#ifndef FORGE_FACTOR_WIDTH
#define FORGE_FACTOR_WIDTH 8
#endif
constexpr int WIDTH = FORGE_FACTOR_WIDTH;
// pooled mixer input: odd | even | placement, plus tanh(odd)*sigmoid(even)
// when the net keeps the multiplicative coupling (flags bit 1).
constexpr int MLP_Z_PLAIN = 2 * WIDTH + PSQ_WIDTH;
constexpr int MLP_Z_PRODUCT = 3 * WIDTH + PSQ_WIDTH;
constexpr int MLP_Z_MAX = MLP_Z_PRODUCT;
constexpr float MLP_CLIP = 8.0f;
static_assert(WIDTH == 4 || WIDTH == 8, "FORGE factor width must be 4 or 8");
constexpr int EVAL_LIMIT = 12000;
constexpr int PSQ_W = PSQ_WIDTH;
constexpr int LUT_SIZE = 4097;
constexpr float ATANH_LIMIT = 0.98f;
constexpr std::uint64_t PAYLOAD_BYTES_ONE_BUCKET =
    std::uint64_t(ROUTES) * ODD * BINS * sizeof(std::int16_t)
  + std::uint64_t(ROUTES) * EVEN * BINS * sizeof(std::int16_t)
  + std::uint64_t(ODD) * BINS * WIDTH * sizeof(std::int8_t)
  + std::uint64_t(EVEN) * BINS * WIDTH * sizeof(std::int8_t)
  // advantageScale, decisiveScale and decisiveBias are per route; oddScale and
  // evenScale are per channel. Writing that as 5*ROUTES was only ever right
  // because ROUTES and WIDTH are both 8.
  + std::uint64_t(3 * ROUTES + 2 * WIDTH + 2 * ROUTES * WIDTH) * sizeof(float)
  // piece-square branch: int8 embedding, per-channel scale, route-local heads
  + std::uint64_t(PSQ_PLANES) * PSQ_W * sizeof(std::int8_t)
  + std::uint64_t(PSQ_W + 2 * ROUTES * PSQ_W) * sizeof(float)
  // one route table per head
  + std::uint64_t(2 * ROUTE_CELLS);

constexpr std::uint64_t payload_bytes(int kingBuckets, int mlpHidden = 0,
                                     int mlpZ = MLP_Z_PLAIN) {
    return PAYLOAD_BYTES_ONE_BUCKET
         + std::uint64_t(kingBuckets - 1) * PSQ_PLANES * PSQ_W * sizeof(std::int8_t)
         // per-route [z x hidden] mixer, its bias, two output vectors, 3 scales
         + std::uint64_t(mlpHidden)
             * (std::uint64_t(ROUTES) * mlpZ + 3 * std::uint64_t(ROUTES))
             * sizeof(float)
         + (mlpHidden ? 3 * sizeof(float) : 0);
}

#pragma pack(push, 1)
struct FileHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t headerBytes;
    std::uint32_t routes;
    std::uint32_t oddFeatures;
    std::uint32_t evenFeatures;
    std::uint32_t bins;
    std::uint32_t width;
    float readoutScale;
    std::uint64_t payloadBytes;
    std::uint32_t payloadCrc32;
    std::uint32_t flags;
    std::uint8_t reserved[8];
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == HEADER_BYTES);
// The known layouts, spelled out so a change to any shape above breaks the
// build rather than the loader. A feature set not listed here is still valid;
// PAYLOAD_BYTES is derived, and the header carries the counts.
static_assert(WIDTH != 8 || ODD != 34 || EVEN != 69 || PSQ_WIDTH != 16 || PAYLOAD_BYTES_ONE_BUCKET == 333786);
static_assert(WIDTH != 8 || ODD != 34 || EVEN != 69 || PSQ_WIDTH != 8  || PAYLOAD_BYTES_ONE_BUCKET == 327098);
static_assert(WIDTH != 8 || ODD != 34 || EVEN != 45 || PSQ_WIDTH != 16 || PAYLOAD_BYTES_ONE_BUCKET == 259482);
static_assert(WIDTH != 8 || ODD != 34 || EVEN != 45 || PSQ_WIDTH != 8  || PAYLOAD_BYTES_ONE_BUCKET == 252794);
static_assert(WIDTH != 8 || ODD != 31 || EVEN != 18 || PSQ_WIDTH != 16 || PAYLOAD_BYTES_ONE_BUCKET == 166602);
static_assert(WIDTH != 8 || ODD != 31 || EVEN != 18 || PSQ_WIDTH != 8  || PAYLOAD_BYTES_ONE_BUCKET == 159914);
static_assert(WIDTH != 4 || ODD != 31 || EVEN != 18 || PSQ_WIDTH != 16 || PAYLOAD_BYTES_ONE_BUCKET == 141030);
static_assert(WIDTH != 4 || ODD != 31 || EVEN != 18 || PSQ_WIDTH != 8  || PAYLOAD_BYTES_ONE_BUCKET == 134342);
static_assert(std::endian::native == std::endian::little,
              "FORGE factor format currently requires little-endian hardware");

std::uint32_t crc32(const std::uint8_t* data, std::size_t size) {
    std::uint32_t result = 0xffff'ffffu;
    for (std::size_t index = 0; index < size; ++index) {
        result ^= data[index];
        for (int bit = 0; bit < 8; ++bit)
            result = (result >> 1) ^ (0xedb8'8320u & (0u - (result & 1u)));
    }
    return ~result;
}

double sigmoid(double value) {
    if (value >= 0.0) {
        const double z = std::exp(-value);
        return 1.0 / (1.0 + z);
    }
    const double z = std::exp(value);
    return z / (1.0 + z);
}

double softplus(double value) {
    return value >= 0.0 ? value + std::log1p(std::exp(-value))
                        : std::log1p(std::exp(value));
}

double logaddexp(double lhs, double rhs) {
    const double maximum = std::max(lhs, rhs);
    return maximum + std::log1p(std::exp(-std::abs(lhs - rhs)));
}

double expected_logit(double advantage, double decisive) {
    constexpr double LOG_TWO = 0.693147180559945309417232121458176568;
    const double onePlusAdvantage = softplus(advantage);
    return logaddexp(onePlusAdvantage, LOG_TWO + advantage + decisive)
         - logaddexp(onePlusAdvantage, LOG_TWO + decisive);
}

// Round-half-to-even, matching numpy's rint on the training side. The divisor
// is always a power of two so this stays a shift and a mask: reading it from a
// table instead of folding a literal at each call site would otherwise turn
// every feature into an integer division, which measured +197 ns per
// evaluation -- more than half the entire learned layer.
int round_ratio(int value, int numerator, int shift) {
    const int scaled = value * numerator;
    const int magnitude = std::abs(scaled);
    const int quotient = magnitude >> shift;
    const int remainder = magnitude & ((1 << shift) - 1);
    const int half = 1 << (shift - 1);
    const int rounded = quotient
                      + (remainder > half
                         || (remainder == half && (quotient & 1)));
    return scaled < 0 ? -rounded : rounded;
}

// Per-feature bin resolution. A single (1,32)/(1,16) divisor left 99% of real
// positions inside 9.4 of the 129 bins, with 76 of 103 features carrying four
// times more distinct raw states than bins they reached. These are power-of-two
// rescalings of that measurement: round_ratio stays a shift, so the instruction
// count per evaluation is unchanged and only the resolution moves.
struct Ratio {
    int numerator;
    int shift;      // divisor is 1 << shift, always >= 2
};

constexpr Ratio ODD_BIN_FULL[FULL_ODD] = {
    {1, 4}, {1, 4}, {4, 1}, {4, 1}, {4, 1}, {4, 1},
    {4, 1}, {1, 4}, {1, 5}, {1, 1}, {1, 1}, {4, 1},
    {4, 1}, {2, 1}, {1, 1}, {2, 1}, {8, 1}, {1, 4},
    {1, 4}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {4, 1},
    {1, 1}, {4, 1}, {2, 1}, {1, 4}, {1, 1}, {1, 2},
    {1, 2}, {1, 1}, {1, 3}, {1, 1}
};

constexpr Ratio EVEN_BIN_FULL[FULL_EVEN] = {
    {1, 5}, {4, 1}, {13, 5}, {1, 1}, {4, 1}, {4, 1},
    {4, 1}, {4, 1}, {4, 1}, {4, 1}, {4, 1}, {4, 1},
    {4, 1}, {4, 1}, {1, 5}, {1, 5}, {1, 3}, {1, 3},
    {1, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 1}, {2, 1},
    {1, 5}, {1, 5}, {1, 1}, {1, 1}, {1, 2}, {1, 2},
    {1, 1}, {1, 3}, {1, 1}, {1, 2}, {1, 5}, {1, 3},
    {1, 4}, {1, 4}, {1, 3}, {1, 5}, {1, 2}, {1, 2},
    {2, 1}, {2, 1}, {1, 1}, {1, 2}, {1, 1}, {4, 1},
    {1, 5}, {1, 5}, {1, 1}, {1, 1}, {1, 1}, {1, 1},
    {2, 1}, {1, 2}, {2, 1}, {1, 1}, {1, 5}, {1, 2},
    {1, 3}, {1, 3}, {1, 2}, {1, 4}, {1, 3}, {1, 2},
    {4, 1}, {4, 1}, {4, 1}
};

#ifdef FORGE_WIDE_RUNTIME
// Appended in TraceX/extract_wide order. These are the exact power-of-two
// ratios used to pre-bin the training sidecar (wide_ratios.json).
constexpr Ratio WIDE_ODD_BIN_FULL[WIDE_Q] = {
    {1, 6}, {1, 2}, {2, 1}, {16, 1}, {32, 1}, {8, 1}, {16, 1},
    {64, 1}, {16, 1}, {128, 1}, {128, 1}, {64, 1}, {64, 1},
    {32, 1}, {128, 1}, {16, 1}, {64, 1}, {64, 1}, {64, 1},
    {128, 1}, {64, 1}, {1, 4}, {1, 4}, {256, 1}, {256, 1}, {64, 1}
};
constexpr Ratio WIDE_EVEN_BIN_FULL[WIDE_Q] = {
    {1, 6}, {1, 4}, {1, 2}, {8, 1}, {16, 1}, {2, 1}, {2, 1},
    {16, 1}, {2, 1}, {64, 1}, {32, 1}, {32, 1}, {16, 1}, {8, 1},
    {32, 1}, {4, 1}, {2, 1}, {16, 1}, {16, 1}, {32, 1}, {16, 1},
    {1, 6}, {1, 5}, {64, 1}, {64, 1}, {16, 1}
};
#endif

// Selecting the kept ratios at compile time keeps one table in the source and
// one mask deciding what is read, rather than a hand-maintained table per
// feature-set variant that could drift from the trainer's.
template <int Kept, int Full>
struct KeptRatios {
    Ratio entry[Kept]{};
    constexpr KeptRatios(const Ratio (&full)[Full], const bool (&keep)[Full]) {
        int out = 0;
        for (int index = 0; index < Full; ++index)
            if (keep[index]) entry[out++] = full[index];
    }
};
#ifdef FORGE_WIDE_RUNTIME
template <int Kept, int Full, int Extra>
struct WideRatios {
    Ratio entry[Kept]{};
    constexpr WideRatios(const Ratio (&base)[Full], const bool (&baseKeep)[Full],
                         const Ratio (&wide)[Extra], const bool (&wideKeep)[Extra]) {
        int out = 0;
        for (int index = 0; index < Full; ++index)
            if (baseKeep[index]) entry[out++] = base[index];
        for (int index = 0; index < Extra; ++index)
            if (wideKeep[index]) entry[out++] = wide[index];
    }
};
constexpr WideRatios<ODD, FULL_ODD, WIDE_Q> ODD_BIN{
    ODD_BIN_FULL, ODD_KEEP, WIDE_ODD_BIN_FULL, WIDE_KEEP};
constexpr WideRatios<EVEN, FULL_EVEN, WIDE_Q> EVEN_BIN{
    EVEN_BIN_FULL, EVEN_KEEP, WIDE_EVEN_BIN_FULL, WIDE_KEEP};
#else
constexpr KeptRatios<ODD, FULL_ODD> ODD_BIN{ODD_BIN_FULL, ODD_KEEP};
constexpr KeptRatios<EVEN, FULL_EVEN> EVEN_BIN{EVEN_BIN_FULL, EVEN_KEEP};
#endif

// round_ratio computes 1 << (shift - 1), so shift 0 would be undefined.
// Ratios needing it are stored doubled, e.g. (2,1) as {4, shift 1}. The
// generator guarantees this; assert it so the build breaks rather than the
// engine if a feature is later added with a divisor of one.
constexpr bool all_shifts_valid() {
    for (const Ratio& ratio : ODD_BIN.entry)
        if (ratio.shift < 1 || ratio.shift > 30) return false;
    for (const Ratio& ratio : EVEN_BIN.entry)
        if (ratio.shift < 1 || ratio.shift > 30) return false;
    return true;
}
static_assert(all_shifts_valid(), "bin ratios must have shift >= 1");

int clamp_bin(int value) { return std::clamp(value, 0, BINS - 1); }
int centered_bin(int value) { return std::clamp(value + 64, 0, BINS - 1); }

struct NonlinearTables {
    std::array<float, LUT_SIZE> tanhValues{};
    std::array<float, LUT_SIZE> sigmoidValues{};
    std::array<float, LUT_SIZE> atanhValues{};

    NonlinearTables() {
        for (int index = 0; index < LUT_SIZE; ++index) {
            const float x = -8.0f + 16.0f * index / (LUT_SIZE - 1);
            tanhValues[index] = std::tanh(x);
            const float sigmoidX = -12.0f + 24.0f * index / (LUT_SIZE - 1);
            sigmoidValues[index] = 1.0f / (1.0f + std::exp(-sigmoidX));
            const float atanhX = -ATANH_LIMIT
                               + 2.0f * ATANH_LIMIT * index / (LUT_SIZE - 1);
            atanhValues[index] = std::atanh(atanhX);
        }
    }

    float lookup(const std::array<float, LUT_SIZE>& table, float value,
                 float lowerBound, float upperBound) const {
        const float position = std::clamp(
            (value - lowerBound) * ((LUT_SIZE - 1) / (upperBound - lowerBound)),
            0.0f, float(LUT_SIZE - 1));
        const int lower = int(position);
        if (lower == LUT_SIZE - 1) return table.back();
        const float fraction = position - lower;
        return table[lower] + fraction * (table[lower + 1] - table[lower]);
    }

    float tanh(float value) const { return lookup(tanhValues, value, -8.0f, 8.0f); }
    float sigmoid(float value) const {
        // Full-validation even latents reach 9.82 (99.99% are below 9.11).
        // Cover their real range rather than pinning them at sigmoid(8).
        return lookup(sigmoidValues, value, -12.0f, 12.0f);
    }
    float atanh(float value) const {
        return lookup(atanhValues, value, -ATANH_LIMIT, ATANH_LIMIT);
    }
};

const NonlinearTables NONLINEAR;

// On by default: measured bit-identical to the table path on all 554,772
// validation positions, through both score() and predict(), and 84 ns cheaper
// in the learned layer. A wider random-legal-position audit found rare +/-1
// score differences (and two +/-2 cases), so "bit-identical" only describes
// that validation corpus. -DFORGE_TABLE_NONLINEAR restores the reference path.
#ifndef FORGE_TABLE_NONLINEAR
// The head's cost is dominated by those tables, not by arithmetic: at width 8
// and sixteen piece-square channels it takes about fifty interpolated lookups
// per evaluation, each two scattered loads into 48 KB. Measured, the
// piece-square head alone is 112 ns for sixteen channels -- 7 ns a channel for
// four multiplies and two lookups.
//
// This is the Pade [7/8] for tanh, which is accurate to about 1e-7 over the
// range it is clamped to and needs no table at all. Eight lanes at a time, the
// polynomial costs under two operations per channel where the table cost two
// potential cache misses. sigmoid comes out of the same function, since
// sigmoid(x) = (1 + tanh(x/2)) / 2.
inline __m256 tanh8(__m256 x) {
    // The approximation diverges past its fitting interval, so the input is
    // clamped rather than the output: that keeps it continuous, at the price of
    // saturating at tanh(3.5) = 0.998178 instead of 1.
    const __m256 limit = _mm256_set1_ps(3.5f);
    const __m256 v = _mm256_min_ps(_mm256_max_ps(x, _mm256_set1_ps(-3.5f)), limit);
    const __m256 v2 = _mm256_mul_ps(v, v);
    __m256 num = _mm256_add_ps(v2, _mm256_set1_ps(378.0f));
    num = _mm256_add_ps(_mm256_mul_ps(num, v2), _mm256_set1_ps(17325.0f));
    num = _mm256_add_ps(_mm256_mul_ps(num, v2), _mm256_set1_ps(135135.0f));
    num = _mm256_mul_ps(num, v);
    __m256 den = _mm256_add_ps(_mm256_mul_ps(v2, _mm256_set1_ps(28.0f)),
                               _mm256_set1_ps(3150.0f));
    den = _mm256_add_ps(_mm256_mul_ps(den, v2), _mm256_set1_ps(62370.0f));
    den = _mm256_add_ps(_mm256_mul_ps(den, v2), _mm256_set1_ps(135135.0f));
    return _mm256_div_ps(num, den);
}

inline __m256 sigmoid8(__m256 x) {
    return _mm256_add_ps(_mm256_set1_ps(0.5f),
                         _mm256_mul_ps(_mm256_set1_ps(0.5f),
                                       tanh8(_mm256_mul_ps(x, _mm256_set1_ps(0.5f)))));
}

inline __m128 tanh4(__m128 x) {
    const __m128 limit = _mm_set1_ps(3.5f);
    const __m128 v = _mm_min_ps(_mm_max_ps(x, _mm_set1_ps(-3.5f)), limit);
    const __m128 v2 = _mm_mul_ps(v, v);
    __m128 num = _mm_add_ps(v2, _mm_set1_ps(378.0f));
    num = _mm_add_ps(_mm_mul_ps(num, v2), _mm_set1_ps(17325.0f));
    num = _mm_add_ps(_mm_mul_ps(num, v2), _mm_set1_ps(135135.0f));
    num = _mm_mul_ps(num, v);
    __m128 den = _mm_add_ps(_mm_mul_ps(v2, _mm_set1_ps(28.0f)),
                            _mm_set1_ps(3150.0f));
    den = _mm_add_ps(_mm_mul_ps(den, v2), _mm_set1_ps(62370.0f));
    den = _mm_add_ps(_mm_mul_ps(den, v2), _mm_set1_ps(135135.0f));
    return _mm_div_ps(num, den);
}

inline __m128 sigmoid4(__m128 x) {
    return _mm_add_ps(_mm_set1_ps(0.5f),
                      _mm_mul_ps(_mm_set1_ps(0.5f),
                                 tanh4(_mm_mul_ps(x, _mm_set1_ps(0.5f)))));
}

inline float reduce4(__m128 v) {
    v = _mm_add_ps(v, _mm_movehl_ps(v, v));
    v = _mm_add_ss(v, _mm_shuffle_ps(v, v, 1));
    return _mm_cvtss_f32(v);
}

inline float reduce8(__m256 v) {
    const __m128 hi = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(_mm256_castps256_ps128(v), hi);
    sum = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
    sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));
    return _mm_cvtss_f32(sum);
}
#endif

// Multiplies the readout on the way to an integer score.  Deliberately not a
// member of Model: it is a search knob, so reloading a net must not reset it.
float ScoreScale = 1.0f;

#ifdef FORGE_DECISIVE_SEARCH
// Read by the optional decisive-pruning experiment. Ordinary builds must not
// pay its extra sigmoid lookup at every node; the experiment measured null and
// remains available only when explicitly compiled in.
float LastDecisive = 0.5f;
#endif

double fast_expected_logit(float advantage, float decisive) {
    // E = 1/2 + c(q-1/2), with 2q-1 = tanh(A/2), hence the exact identity
    // logit(E) = 2*atanh(sigmoid(C)*tanh(A/2)).  The interpolated middle is
    // much cheaper than exp/log; unusual tails retain the stable exact path.
    if (std::abs(advantage) > 16.0f || std::abs(decisive) > 8.0f)
        return expected_logit(advantage, decisive);
    const float centered = NONLINEAR.sigmoid(decisive)
                         * NONLINEAR.tanh(0.5f * advantage);
    if (std::abs(centered) >= ATANH_LIMIT)
        return expected_logit(advantage, decisive);
    return 2.0 * NONLINEAR.atanh(centered);
}

// A net's tables are meaningless under different bin edges, and nothing else in
// the header notices: shape, payload size and CRC all still match. Loading a
// pre-rebin net into the rebinned engine moved the start position from 12 to 8
// centipawns with no diagnostic, so the layout is part of the contract now.
std::uint32_t bin_layout_digest() {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(std::size_t(ODD + EVEN) * 8);
    const auto push = [&bytes](int value) {
        for (int byte = 0; byte < 4; ++byte)
            bytes.push_back(std::uint8_t((value >> (8 * byte)) & 0xff));
    };
    for (const Ratio& ratio : ODD_BIN.entry)  { push(ratio.numerator); push(ratio.shift); }
    for (const Ratio& ratio : EVEN_BIN.entry) { push(ratio.numerator); push(ratio.shift); }
#if defined(FORGE_MINI) || defined(FORGE_LEAN)
    // Two different masks could in principle select the same ratio sequence.
    // Digesting the mask as well means a net trained against a different
    // feature set is rejected rather than silently misread.
    for (bool keep : ODD_KEEP) push(keep);
#ifdef FORGE_WIDE_RUNTIME
    for (bool keep : WIDE_KEEP) push(keep);
#endif
    for (bool keep : EVEN_KEEP) push(keep);
#ifdef FORGE_WIDE_RUNTIME
    for (bool keep : WIDE_KEEP) push(keep);
#endif
#endif
    return crc32(bytes.data(), bytes.size());
}

int own_king_bucket(Color perspective, Square absoluteSquare, int bucketCount) {
    if (bucketCount == 1)
        return 0;
    const int relative = perspective == WHITE
        ? int(absoluteSquare) : (int(absoluteSquare) ^ 56);
    const int rankBand = relative / 16;  // relative rank / 2, range 0..3
    const int file = relative & 7;
    return bucketCount == 32 ? rankBand * 8 + file
                             : rankBand * 4 + file / 2;
}

struct Model {
    std::vector<std::int16_t> advantage;
    std::vector<std::int16_t> decisive;
    std::vector<std::int8_t> oddLatent;
    std::vector<std::int8_t> evenLatent;
    std::array<float, ROUTES> advantageScale{};
    std::array<float, ROUTES> decisiveScale{};
    std::array<float, WIDTH> oddScale{};
    std::array<float, WIDTH> evenScale{};
    std::array<float, ROUTES> decisiveBias{};
    std::array<float, ROUTES * WIDTH> advantageOut{};
    std::array<float, ROUTES * WIDTH> decisiveOut{};
    std::vector<std::int8_t> psqEmbed;
    std::array<float, PSQ_W> psqScale{};
    std::array<float, ROUTES * PSQ_W> psqAdvantageOut{};
    std::array<float, ROUTES * PSQ_W> psqDecisiveOut{};
    std::array<std::uint8_t, ROUTE_CELLS> routeLutAdvantage{};
    std::array<std::uint8_t, ROUTE_CELLS> routeLutDecisive{};
    std::vector<float> mlpW1;        // [route][z][hidden], hidden contiguous
    std::vector<float> mlpB1;        // [route][hidden]
    std::vector<float> mlpAdv;       // [route][hidden]
    std::vector<float> mlpDec;       // [route][hidden]
    std::array<float, 3> mlpScale{{1.0f, 1.0f, 1.0f}};
    int mlpHidden = 0;
    bool mlpProduct = false;
    int mlp_z() const { return mlpProduct ? MLP_Z_PRODUCT : MLP_Z_PLAIN; }
    int kingBuckets = 1;
    float readoutScale = 187.0f;
    bool crossGated = false;
    std::string loadedPath;

    bool decode(const std::vector<std::uint8_t>& payload, std::string& error) {
        std::size_t offset = 0;
        auto take = [&](void* destination, std::size_t bytes, const char* name) {
            if (bytes > payload.size() - offset) {
                error = std::string("truncated FORGE factor payload at ") + name;
                return false;
            }
            std::memcpy(destination, payload.data() + offset, bytes);
            offset += bytes;
            return true;
        };
        auto take_vector = [&](auto& destination, std::size_t count, const char* name) {
            destination.resize(count);
            return take(destination.data(), destination.size() * sizeof(destination[0]), name);
        };
        auto take_array = [&](auto& destination, const char* name) {
            return take(destination.data(), destination.size() * sizeof(destination[0]), name);
        };
        if (!take_vector(advantage, ROUTES * ODD * BINS, "advantage")
            || !take_vector(decisive, ROUTES * EVEN * BINS, "decisive")
            || !take_vector(oddLatent, ODD * BINS * WIDTH, "odd latent")
            || !take_vector(evenLatent, EVEN * BINS * WIDTH, "even latent")
            || !take_array(advantageScale, "advantage scales")
            || !take_array(decisiveScale, "decisive scales")
            || !take_array(oddScale, "odd scales")
            || !take_array(evenScale, "even scales")
            || !take_array(decisiveBias, "decisive biases")
            || !take_array(advantageOut, "advantage output")
            || !take_array(decisiveOut, "decisive output")
            || !take_vector(psqEmbed, std::size_t(kingBuckets) * PSQ_PLANES * PSQ_W,
                            "king-conditioned piece-square embedding")
            || !take_array(psqScale, "piece-square scales")
            || !take_array(psqAdvantageOut, "piece-square advantage output")
            || !take_array(psqDecisiveOut, "piece-square decisive output")
            || !take_array(routeLutAdvantage, "advantage route table")
            || !take_array(routeLutDecisive, "decisive route table"))
            return false;
        if (mlpHidden) {
            const std::size_t h = std::size_t(mlpHidden);
            if (!take_vector(mlpW1, std::size_t(ROUTES) * mlp_z() * h, "mlp w1")
                || !take_vector(mlpB1, std::size_t(ROUTES) * h, "mlp b1")
                || !take_vector(mlpAdv, std::size_t(ROUTES) * h, "mlp advantage out")
                || !take_vector(mlpDec, std::size_t(ROUTES) * h, "mlp decisive out")
                || !take_array(mlpScale, "mlp block scales"))
                return false;
            for (float value : mlpW1)
                if (!std::isfinite(value)) {
                    error = "FORGE factor mlp weights contain a non-finite value";
                    return false;
                }
        }
        for (std::size_t cell = 0; cell < ROUTE_CELLS; ++cell)
            if (routeLutAdvantage[cell] >= ROUTES || routeLutDecisive[cell] >= ROUTES) {
                error = "FORGE factor route table names a route that does not exist";
                return false;
            }
        if (offset != payload.size()) {
            error = "FORGE factor payload has trailing data";
            return false;
        }
        // Validate the decoded float members directly. Scanning a trailing byte
        // range instead was wrong once the int8 piece-square embedding landed
        // between the two float regions: at width 16 the misread bytes happened
        // to look finite and the bug stayed hidden, at width 8 they did not.
        const auto finite_range = [&error](const float* first, std::size_t count) {
            for (std::size_t i = 0; i < count; ++i)
                if (!std::isfinite(first[i])) {
                    error = "FORGE factor net contains a non-finite parameter";
                    return false;
                }
            return true;
        };
        return finite_range(advantageScale.data(), advantageScale.size())
            && finite_range(decisiveScale.data(), decisiveScale.size())
            && finite_range(oddScale.data(), oddScale.size())
            && finite_range(evenScale.data(), evenScale.size())
            && finite_range(decisiveBias.data(), decisiveBias.size())
            && finite_range(advantageOut.data(), advantageOut.size())
            && finite_range(decisiveOut.data(), decisiveOut.size())
            && finite_range(psqScale.data(), psqScale.size())
            && finite_range(psqAdvantageOut.data(), psqAdvantageOut.size())
            && finite_range(psqDecisiveOut.data(), psqDecisiveOut.size());
    }

    // Both inputs are already computed for the feature vector, and both change
    // only on a capture or a promotion, so the index costs a multiply and a
    // load off values that are in registers anyway.
    static int route_cell(int phase, int pawns) {
        return (phase < ROUTE_PHASES ? phase : ROUTE_PHASES - 1) * ROUTE_PAWNS + pawns;
    }

    Prediction evaluate(const Position& pos, Value boringScore,
                        const Eval::Trace& trace, bool full,
                        const PsqAccumulator* accumulator) const {
        std::array<std::int8_t, ODD> oddSign;
        std::array<std::uint8_t, ODD> oddBin;
        std::array<std::uint8_t, EVEN> evenBin;
        int oi = 0;
        int ei = 0;
#if defined(FORGE_MINI) || defined(FORGE_LEAN)
        int oiRaw = 0;
        int eiRaw = 0;
#endif
        const Color us = pos.side_to_move();
        const Color them = ~us;
        const int orient = us == WHITE ? 1 : -1;
        const int ownQueens = pos.count<QUEEN>(us);
        const int enemyQueens = pos.count<QUEEN>(them);
        const int signedCounts[5] = {
            pos.count<PAWN>(us) - pos.count<PAWN>(them),
            pos.count<KNIGHT>(us) - pos.count<KNIGHT>(them),
            pos.count<BISHOP>(us) - pos.count<BISHOP>(them),
            pos.count<ROOK>(us) - pos.count<ROOK>(them),
            ownQueens - enemyQueens,
        };
        auto appendOdd = [&](int raw) {
            const Ratio scale = ODD_BIN.entry[oi];
            oddSign[oi] = std::int8_t((raw > 0) - (raw < 0));
            oddBin[oi++] = std::uint8_t(clamp_bin(
                round_ratio(std::abs(raw), scale.numerator, scale.shift)));
        };
        auto odd = [&](int raw) {
#if defined(FORGE_MINI) || defined(FORGE_LEAN)
            if (!ODD_KEEP[oiRaw++]) return;
#endif
            appendOdd(raw);
        };
        auto appendEven = [&](int raw) {
            const Ratio scale = EVEN_BIN.entry[ei];
            evenBin[ei++] = std::uint8_t(centered_bin(
                round_ratio(raw, scale.numerator, scale.shift)));
        };
        auto even = [&](int raw) {
#if defined(FORGE_MINI) || defined(FORGE_LEAN)
            if (!EVEN_KEEP[eiRaw++]) return;
#endif
            appendEven(raw);
        };

        odd(int(boringScore));
        odd(pos.non_pawn_material(us) - pos.non_pawn_material(them));
        for (int count : signedCounts) odd(count);
        odd(trace.psqt.mg * orient);
        odd(trace.psqt.eg * orient);
        for (int section = 0; section < Eval::TRACE_SECTION_NB; ++section) {
            odd((trace.section[section][WHITE].mg
               - trace.section[section][BLACK].mg) * orient);
            odd((trace.section[section][WHITE].eg
               - trace.section[section][BLACK].eg) * orient);
        }
        odd((trace.space[WHITE] - trace.space[BLACK]) * orient);

#ifdef FORGE_WIDE_RUNTIME
        // Training extracted each colour through an int16 sidecar. Preserve
        // that clipping before forming W-B so runtime bins are exactly the
        // bins the model saw, including pathological king-danger tails.
        const auto clipWide = [](int value) {
            return std::clamp(value, -32768, 32767);
        };
        const int wideWhite[WIDE_Q] = {
            trace.x.kingDanger[WHITE], trace.x.shelterMg[WHITE],
            trace.x.shelterEg[WHITE], trace.x.safeChecks[WHITE],
            trace.x.kingAttackers[WHITE], trace.x.flankAttack[WHITE],
            trace.x.flankDefense[WHITE], trace.x.hanging[WHITE],
            trace.x.restricted[WHITE], trace.x.threatSafePawn[WHITE],
            trace.x.threatPawnPush[WHITE], trace.x.weakQueenProt[WHITE],
            trace.x.passerCount[WHITE], trace.x.passerMaxRank[WHITE],
            trace.x.passerFree[WHITE], trace.x.spaceBonus[WHITE],
            trace.x.spaceWeight[WHITE], trace.x.connectedPassers[WHITE],
            trace.x.candidatePassers[WHITE], trace.x.rookOnSeventh[WHITE],
            trace.x.batteries[WHITE], trace.x.hangingMaterial[WHITE],
            trace.x.maxHangingVictim[WHITE], trace.x.blockaderMinor[WHITE],
            trace.x.bothFlanks[WHITE], trace.x.infiltration[WHITE]
        };
        const int wideBlack[WIDE_Q] = {
            trace.x.kingDanger[BLACK], trace.x.shelterMg[BLACK],
            trace.x.shelterEg[BLACK], trace.x.safeChecks[BLACK],
            trace.x.kingAttackers[BLACK], trace.x.flankAttack[BLACK],
            trace.x.flankDefense[BLACK], trace.x.hanging[BLACK],
            trace.x.restricted[BLACK], trace.x.threatSafePawn[BLACK],
            trace.x.threatPawnPush[BLACK], trace.x.weakQueenProt[BLACK],
            trace.x.passerCount[BLACK], trace.x.passerMaxRank[BLACK],
            trace.x.passerFree[BLACK], trace.x.spaceBonus[BLACK],
            trace.x.spaceWeight[BLACK], trace.x.connectedPassers[BLACK],
            trace.x.candidatePassers[BLACK], trace.x.rookOnSeventh[BLACK],
            trace.x.batteries[BLACK], trace.x.hangingMaterial[BLACK],
            trace.x.maxHangingVictim[BLACK], trace.x.blockaderMinor[BLACK],
            trace.x.bothFlanks[BLACK], trace.x.infiltration[BLACK]
        };
        for (int index = 0; index < WIDE_Q; ++index)
            if (WIDE_KEEP[index])
                appendOdd((clipWide(wideWhite[index]) - clipWide(wideBlack[index])) * orient);
#endif

        const int phaseValue = pos.phase();
        even(std::abs(int(boringScore)));
        even(phaseValue);
        even(pos.rule50_count());
        even(trace.scale);
        const int totals[5] = {
            pos.count<PAWN>(WHITE) + pos.count<PAWN>(BLACK),
            pos.count<KNIGHT>(WHITE) + pos.count<KNIGHT>(BLACK),
            pos.count<BISHOP>(WHITE) + pos.count<BISHOP>(BLACK),
            pos.count<ROOK>(WHITE) + pos.count<ROOK>(BLACK),
            ownQueens + enemyQueens,
        };
        for (int total : totals) even(total);
        for (int count : signedCounts) even(std::abs(count));
        even(std::abs(trace.psqt.mg));
        even(std::abs(trace.psqt.eg));
        for (int section = 0; section < Eval::TRACE_SECTION_NB; ++section) {
            even(trace.section[section][WHITE].mg
               + trace.section[section][BLACK].mg);
            even(trace.section[section][WHITE].eg
               + trace.section[section][BLACK].eg);
        }
#ifndef FORGE_NO_ABSDIFF
        for (int section = 0; section < Eval::TRACE_SECTION_NB; ++section) {
            even(std::abs(trace.section[section][WHITE].mg
                        - trace.section[section][BLACK].mg));
            even(std::abs(trace.section[section][WHITE].eg
                        - trace.section[section][BLACK].eg));
        }
#endif
        even(trace.space[WHITE] + trace.space[BLACK]);
        even(std::abs(trace.space[WHITE] - trace.space[BLACK]));
        const bool ownDark = bool(pos.pieces(us, BISHOP) & DARK_SQUARES);
        const bool ownLight = bool(pos.pieces(us, BISHOP) & ~DARK_SQUARES);
        const bool enemyDark = bool(pos.pieces(them, BISHOP) & DARK_SQUARES);
        const bool enemyLight = bool(pos.pieces(them, BISHOP) & ~DARK_SQUARES);
        const bool opposite = pos.count<BISHOP>(us) == 1
                           && pos.count<BISHOP>(them) == 1
                           && ownDark != enemyDark;
        const bool pureBishops = opposite && pos.count<KNIGHT>(us) == 0
                              && pos.count<KNIGHT>(them) == 0
                              && pos.count<ROOK>(us) == 0
                              && pos.count<ROOK>(them) == 0
                              && ownQueens == 0 && enemyQueens == 0;
        even(opposite);
        even(pureBishops);
        even(int(ownDark && ownLight) + int(enemyDark && enemyLight));

#ifdef FORGE_WIDE_RUNTIME
        for (int index = 0; index < WIDE_Q; ++index)
            if (WIDE_KEEP[index])
                appendEven(clipWide(wideWhite[index]) + clipWide(wideBlack[index]));
#endif

        std::array<std::int32_t, PSQ_W> psqSum{};
        if (accumulator) {
            const auto* view = accumulator->view[pos.side_to_move()];
            for (int channel = 0; channel < PSQ_W; ++channel)
                psqSum[channel] = view[channel];
        } else {
            int planes[PSQ_MAX_PIECES];
            const int planeCount = extract_planes(pos, planes);
            const int kingBucket = own_king_bucket(
                pos.side_to_move(), pos.king_square(pos.side_to_move()), kingBuckets);
            for (int piece = 0; piece < planeCount; ++piece) {
                const std::size_t rowIndex = std::size_t(kingBucket) * PSQ_PLANES
                                           + std::size_t(planes[piece]);
                const auto* row = &psqEmbed[rowIndex * PSQ_W];
                for (int channel = 0; channel < PSQ_W; ++channel)
                    psqSum[channel] += row[channel];
            }
        }

        // The advantage head answers who is better and the decisiveness head how
        // sharply; they are separate tables, so they need not be conditioned on
        // the same partition. Whether they differ is a property of the file.
        const int cell = route_cell(phaseValue, totals[0]);
        const int routeA = routeLutAdvantage[cell];
        const int routeC = routeLutDecisive[cell];
#ifdef FORGE_SIEGE
        // Generate the sparse residual once. `head` may run a fast LUT pass
        // and then an exact boundary pass; extraction must not be repeated.
        const ForgeSiege::Residual siege =
            ForgeSiege::predict(pos, trace, routeA, routeC);
#endif
        std::array<std::int32_t, WIDTH> oddSum{};
        std::array<std::int32_t, WIDTH> evenSum{};
        std::int32_t directAdvantage = 0;
        std::int32_t directDecisive = 0;
        for (int feature = 0; feature < ODD; ++feature) {
            const int bin = oddBin[feature];
            const int sign = oddSign[feature];
            directAdvantage += sign * advantage[
                (routeA * ODD + feature) * BINS + bin];
            const auto* values = &oddLatent[(feature * BINS + bin) * WIDTH];
            for (int channel = 0; channel < WIDTH; ++channel)
                oddSum[channel] += sign * values[channel];
        }
        for (int feature = 0; feature < EVEN; ++feature) {
            const int bin = evenBin[feature];
            directDecisive += decisive[
                (routeC * EVEN + feature) * BINS + bin];
            const auto* values = &evenLatent[(feature * BINS + bin) * WIDTH];
            for (int channel = 0; channel < WIDTH; ++channel)
                evenSum[channel] += values[channel];
        }
        const float directAdvantageLogit =
            directAdvantage * advantageScale[routeA];
        const float directDecisiveLogit = decisiveBias[routeC]
                                        + directDecisive * decisiveScale[routeC];
        auto head = [&](bool exact) {
            std::array<float, 2> logits{
                directAdvantageLogit, directDecisiveLogit};
#ifdef FORGE_SIEGE
            logits[0] += siege.advantage;
            logits[1] += siege.decisive;
#endif
            if (mlpHidden) {
                // Replacement hybrid. The tables have already done the
                // expensive part; all that remains is mixing 32 pooled values.
                // Broadcast-accumulate keeps the hidden units in registers, so
                // there is no per-output horizontal reduction: MLP_Z iterations
                // of (hidden/8) FMAs and nothing else.
                (void) exact;
                const int H = mlpHidden;
                const int Z = mlp_z();
                alignas(32) float z[MLP_Z_MAX];
                for (int c = 0; c < WIDTH; ++c)
                    z[c] = float(oddSum[c]) * oddScale[c] * mlpScale[0];
                for (int c = 0; c < WIDTH; ++c)
                    z[WIDTH + c] = float(evenSum[c]) * evenScale[c] * mlpScale[1];
                for (int c = 0; c < PSQ_W; ++c)
                    z[2 * WIDTH + c] = float(psqSum[c]) * psqScale[c] * mlpScale[2];
                if (mlpProduct) {
                    // The coupling the fixed head used, handed to the mixer as
                    // input rather than replaced by it. Scales are the raw
                    // latents', not mlpScale: the trainer forms this product
                    // from the unscaled pooled sums.
                    for (int c = 0; c < WIDTH; ++c) {
                        const float o = float(oddSum[c]) * oddScale[c];
                        const float e = float(evenSum[c]) * evenScale[c];
                        z[2 * WIDTH + PSQ_W + c] =
                            std::tanh(o) * (1.0f / (1.0f + std::exp(-e)));
                    }
                }

                alignas(32) float hidden[64];
                const float* w1 = &mlpW1[std::size_t(routeA) * Z * H];
                const float* b1 = &mlpB1[std::size_t(routeA) * H];
                for (int j = 0; j < H; ++j) hidden[j] = b1[j];
                for (int i = 0; i < Z; ++i) {
                    const float zi = z[i];
                    const float* row = &w1[std::size_t(i) * H];
                    for (int j = 0; j < H; ++j) hidden[j] += zi * row[j];
                }
                const float* wa = &mlpAdv[std::size_t(routeA) * H];
                const float* wc = &mlpDec[std::size_t(routeC) * H];
                float advantageSum = 0.0f, decisiveSum = 0.0f;
                for (int j = 0; j < H; ++j) {
                    const float activated = std::min(std::max(hidden[j], 0.0f), MLP_CLIP);
                    advantageSum += activated * wa[j];
                    decisiveSum += activated * wc[j];
                }
                logits[0] += advantageSum;
                logits[1] += decisiveSum;
                return logits;
            }
            std::array<float, WIDTH> evenGate{};
#ifndef FORGE_TABLE_NONLINEAR
            if (!exact) {
                static_assert(PSQ_W % 8 == 0, "piece-square width must be a multiple of eight");
                __m256 cross;
                __m256 psqAcc0;
                __m256 psqAcc1;
                if constexpr (WIDTH == 8) {
                    const __m256 oddValue = _mm256_mul_ps(
                        _mm256_cvtepi32_ps(_mm256_loadu_si256(
                            reinterpret_cast<const __m256i*>(oddSum.data()))),
                        _mm256_loadu_ps(oddScale.data()));
                    const __m256 evenValue = _mm256_mul_ps(
                        _mm256_cvtepi32_ps(_mm256_loadu_si256(
                            reinterpret_cast<const __m256i*>(evenSum.data()))),
                        _mm256_loadu_ps(evenScale.data()));
                    const __m256 gate = sigmoid8(evenValue);
                    _mm256_storeu_ps(evenGate.data(), gate);
                    psqAcc0 = _mm256_mul_ps(
                        _mm256_mul_ps(_mm256_loadu_ps(&advantageOut[routeA * WIDTH]),
                                      tanh8(oddValue)), gate);
                    psqAcc1 = _mm256_mul_ps(
                        _mm256_mul_ps(_mm256_loadu_ps(&decisiveOut[routeC * WIDTH]),
                                      evenValue), gate);
                    cross = crossGated ? gate : _mm256_set1_ps(1.0f);
                } else {
                    // Four latent channels are a first-class runtime, not an
                    // eight-wide net with four zero lanes.  The section head
                    // stays in one SSE register; its gate is repeated only
                    // when the eight-channel piece-square head consumes it.
                    const __m128 oddValue = _mm_mul_ps(
                        _mm_cvtepi32_ps(_mm_loadu_si128(
                            reinterpret_cast<const __m128i*>(oddSum.data()))),
                        _mm_loadu_ps(oddScale.data()));
                    const __m128 evenValue = _mm_mul_ps(
                        _mm_cvtepi32_ps(_mm_loadu_si128(
                            reinterpret_cast<const __m128i*>(evenSum.data()))),
                        _mm_loadu_ps(evenScale.data()));
                    const __m128 gate = sigmoid4(evenValue);
                    _mm_storeu_ps(evenGate.data(), gate);
                    const __m128 acc0 = _mm_mul_ps(
                        _mm_mul_ps(_mm_loadu_ps(&advantageOut[routeA * WIDTH]),
                                   tanh4(oddValue)), gate);
                    const __m128 acc1 = _mm_mul_ps(
                        _mm_mul_ps(_mm_loadu_ps(&decisiveOut[routeC * WIDTH]),
                                   evenValue), gate);
                    logits[0] += reduce4(acc0);
                    logits[1] += reduce4(acc1);
                    cross = crossGated ? _mm256_set_m128(gate, gate)
                                       : _mm256_set1_ps(1.0f);
                    psqAcc0 = _mm256_setzero_ps();
                    psqAcc1 = _mm256_setzero_ps();
                }
                // Piece-square channel k gates on even latent channel k mod
                // WIDTH. The vector above is either the native eight gates or
                // the four-gate sequence repeated twice.
                for (int block = 0; block < PSQ_W; block += 8) {
                    const __m256 value = _mm256_mul_ps(
                        _mm256_cvtepi32_ps(_mm256_loadu_si256(
                            reinterpret_cast<const __m256i*>(&psqSum[block]))),
                        _mm256_loadu_ps(&psqScale[block]));
                    psqAcc0 = _mm256_add_ps(psqAcc0, _mm256_mul_ps(
                        _mm256_mul_ps(_mm256_loadu_ps(&psqAdvantageOut[routeA * PSQ_W + block]),
                                      tanh8(value)), cross));
                    psqAcc1 = _mm256_add_ps(psqAcc1, _mm256_mul_ps(
                        _mm256_mul_ps(_mm256_loadu_ps(&psqDecisiveOut[routeC * PSQ_W + block]),
                                      _mm256_mul_ps(value, sigmoid8(value))), cross));
                }
                logits[0] += reduce8(psqAcc0);
                logits[1] += reduce8(psqAcc1);
                return logits;
            }
#endif
            for (int channel = 0; channel < WIDTH; ++channel) {
                const float oddValue = oddSum[channel] * oddScale[channel];
                const float evenValue = evenSum[channel] * evenScale[channel];
                const float gate = exact
                    ? float(sigmoid(evenValue)) : NONLINEAR.sigmoid(evenValue);
                evenGate[channel] = gate;
                const float oddActivation = exact
                    ? std::tanh(oddValue) : NONLINEAR.tanh(oddValue);
                logits[0] += advantageOut[routeA * WIDTH + channel]
                           * oddActivation * gate;
                logits[1] += decisiveOut[routeC * WIDTH + channel]
                           * evenValue * gate;
            }
            for (int channel = 0; channel < PSQ_W; ++channel) {
                const float value = psqSum[channel] * psqScale[channel];
                const float squash = exact ? std::tanh(value) : NONLINEAR.tanh(value);
                const float self = exact ? float(sigmoid(value))
                                         : NONLINEAR.sigmoid(value);
                // Cross-gated nets pair psq channel k with even latent channel
                // k mod WIDTH, so placement multiplies against activity instead
                // of sitting beside it. The gate is already computed above, so
                // this is one extra multiply per channel. Same file layout as a
                // self-gated net, which is why the header flag has to say which.
                const float cross = crossGated ? evenGate[channel % WIDTH] : 1.0f;
                logits[0] += psqAdvantageOut[routeA * PSQ_W + channel]
                           * squash * cross;
                logits[1] += psqDecisiveOut[routeC * PSQ_W + channel]
                           * value * self * cross;
            }
            return logits;
        };

        const auto fastLogits = head(false);
        float advantageLogit = fastLogits[0];
        float decisiveLogit = fastLogits[1];

        const double outputScale = double(readoutScale) * double(ScoreScale);
        const double fastRawScore = outputScale
                                  * fast_expected_logit(advantageLogit, decisiveLogit);
        double searchRawScore = fastRawScore;
        // LUT interpolation error is tiny, but a value equally close to two
        // integers can still change the discrete search score. Only positions
        // within 0.05 cp of a rounding boundary pay for the exact tail-safe
        // formula. This preserves integer identity at negligible average cost.
        // The interpolation error is fixed in logit space, so the band has to
        // widen with the gain or a larger ScoreScale would let it through.
        const double guard = std::min(0.05 * double(ScoreScale), 0.5);
        const double fraction = fastRawScore - std::floor(fastRawScore);
        if (std::abs(fraction - 0.5) < guard) {
            const auto exactLogits = head(true);
            searchRawScore = outputScale
                           * expected_logit(exactLogits[0], exactLogits[1]);
        }
        double rawScore = searchRawScore;
        if (full) {
            const auto exactLogits = head(true);
            advantageLogit = exactLogits[0];
            decisiveLogit = exactLogits[1];
            rawScore = outputScale
                     * expected_logit(advantageLogit, decisiveLogit);
        }
        Prediction result;
        result.score = std::isfinite(rawScore)
                     ? Value(std::clamp<long long>(
                           std::llround(rawScore), -EVAL_LIMIT, EVAL_LIMIT))
                     : VALUE_DRAW;
        result.searchScore = std::isfinite(searchRawScore)
                           ? Value(std::clamp<long long>(
                                 std::llround(searchRawScore), -EVAL_LIMIT, EVAL_LIMIT))
                           : VALUE_DRAW;
        result.advantage = advantageLogit;
        result.decisive = decisiveLogit;
        if (full) {
            const double q = sigmoid(advantageLogit);
            const double c = sigmoid(decisiveLogit);
            result.win = float(c * q);
            result.draw = float(1.0 - c);
            result.loss = float(c * (1.0 - q));
        }
        result.route = routeA;
        return result;
    }
};

Model Active;
bool Available = false;

}  // namespace

bool load(const std::string& requestedPath, std::string& error) {
    Available = false;
    Active = Model{};
    if (requestedPath.empty()) {
        error = "NetPath is empty";
        return false;
    }
    std::ifstream input(requestedPath, std::ios::binary | std::ios::ate);
    if (!input) {
        error = "cannot open " + requestedPath;
        return false;
    }
    const std::streamoff fileSize = input.tellg();
    if (fileSize < std::streamoff(HEADER_BYTES)) {
        error = "truncated FORGE factor network header";
        return false;
    }
    input.seekg(0);
    FileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || std::memcmp(header.magic, MAGIC, sizeof(MAGIC)) != 0) {
        error = "not a compatible FORGE factor network";
        return false;
    }
    if ((header.version != LEGACY_VERSION && header.version != KING_VERSION
         && header.version != MLP_VERSION)
        || header.headerBytes != HEADER_BYTES
        || header.routes != ROUTES || header.oddFeatures != ODD
        || header.evenFeatures != EVEN || header.bins != BINS
        || header.width != WIDTH
        || (header.flags & ~0xff03u) != 0 || !std::isfinite(header.readoutScale)
        || header.readoutScale <= 0.0f) {
        error = "incompatible FORGE factor network header";
        return false;
    }
    std::uint32_t fileDigest = 0;
    for (int byte = 0; byte < 4; ++byte)
        fileDigest |= std::uint32_t(header.reserved[byte]) << (8 * byte);
    std::uint32_t fileKingBuckets = 0;
    for (int byte = 4; byte < 8; ++byte)
        fileKingBuckets |= std::uint32_t(header.reserved[byte]) << (8 * (byte - 4));
    int kingBuckets = 1;
    const int fileMlpHidden = int((header.flags >> 8) & 0xffu);
    const bool fileMlpProduct = (header.flags & 2u) != 0;
    if (fileMlpProduct && !fileMlpHidden) {
        error = "FORGE factor product flag set without an mlp head";
        return false;
    }
    if (fileMlpHidden && header.version != MLP_VERSION) {
        error = "FORGE factor mlp head requires a v6 network";
        return false;
    }
    if (header.version == MLP_VERSION && !fileMlpHidden) {
        error = "FORGE v6 network declares no mlp hidden width";
        return false;
    }
    if (header.version == LEGACY_VERSION) {
        if (fileKingBuckets != 0) {
            error = "unsupported FORGE factor network extension";
            return false;
        }
    } else {
        if (fileKingBuckets != 16 && fileKingBuckets != 32) {
            error = "FORGE v5 king bucket count must be 16 or 32";
            return false;
        }
        kingBuckets = int(fileKingBuckets);
    }
    const std::uint64_t expectedPayload = payload_bytes(
        kingBuckets, fileMlpHidden,
        fileMlpProduct ? MLP_Z_PRODUCT : MLP_Z_PLAIN);
    if (header.payloadBytes != expectedPayload
        || fileSize != std::streamoff(HEADER_BYTES + expectedPayload)) {
        error = "wrong FORGE factor net size for " + std::to_string(kingBuckets)
              + " king bucket(s): expected "
              + std::to_string(HEADER_BYTES + expectedPayload) + ", found "
              + std::to_string(fileSize);
        return false;
    }
    if (fileDigest != bin_layout_digest()) {
        char detail[40];
        std::snprintf(detail, sizeof(detail), "%08x, engine has %08x",
                      fileDigest, bin_layout_digest());
        error = std::string("FORGE factor net was trained for a different bin "
                            "layout (") + detail
              + "); it would evaluate silently wrong. Re-export it.";
        return false;
    }
    std::vector<std::uint8_t> payload(std::size_t(header.payloadBytes));
    input.read(reinterpret_cast<char*>(payload.data()), std::streamsize(payload.size()));
    if (!input || input.peek() != std::ifstream::traits_type::eof()) {
        error = "truncated or overlong FORGE factor network";
        return false;
    }
    if (crc32(payload.data(), payload.size()) != header.payloadCrc32) {
        error = "FORGE factor network checksum mismatch";
        return false;
    }
    Model candidate;
    candidate.kingBuckets = kingBuckets;
    candidate.readoutScale = header.readoutScale;
    candidate.crossGated = (header.flags & 1u) != 0;
    candidate.mlpHidden = fileMlpHidden;
    candidate.mlpProduct = fileMlpProduct;
    candidate.loadedPath = requestedPath;
    if (!candidate.decode(payload, error)) return false;
    Active = std::move(candidate);
    Available = true;
    return true;
}

void clear() {
    Available = false;
    Active = Model{};
}

bool ready() { return Available; }
const std::string& path() { return Active.loadedPath; }

void set_score_scale(float scale) {
    // A non-finite or non-positive gain would silently turn every evaluation
    // into VALUE_DRAW, so refuse it rather than accept a dead engine.
    if (std::isfinite(scale) && scale > 0.0f)
        ScoreScale = std::clamp(scale, 0.05f, 20.0f);
}

float score_scale() { return ScoreScale; }

int extract_planes(const Position& pos, int* out) {
    int n = 0;
    const Color us = pos.side_to_move();
    const bool flip = us == BLACK;
    for (PieceType pt = PAWN; pt <= KING; ++pt) {
        const int offset = int(pt) - int(PAWN);
        for (int side = 0; side < 2; ++side) {
            Bitboard remaining = pos.pieces(side == 0 ? us : ~us, pt);
            const int base = (side == 0 ? offset : 6 + offset) * 64;
            while (remaining) {
                const Square square = pop_lsb(remaining);
                out[n++] = base + (flip ? int(square) ^ 56 : int(square));
            }
        }
    }
    return n;
}

namespace {

void psq_apply_view(PsqAccumulator& accumulator, Color perspective,
                    Piece piece, Square square, int coefficient) {
    const int offset = int(type_of(piece)) - int(PAWN);
    const Color colour = color_of(piece);
    const bool own = colour == perspective;
    const int orientedSquare = perspective == WHITE ? int(square) : (int(square) ^ 56);
    const int plane = ((own ? offset : 6 + offset) * 64) + orientedSquare;
    const std::size_t rowIndex = std::size_t(accumulator.kingBucket[perspective])
                               * PSQ_PLANES + std::size_t(plane);
    const auto* row = &Active.psqEmbed[rowIndex * PSQ_W];
    const auto widen = [](const std::int8_t* row) {
        if constexpr (PSQ_WIDTH == 16)
            return _mm256_cvtepi8_epi16(
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(row)));
        else
            return _mm_cvtepi8_epi16(
                _mm_loadl_epi64(reinterpret_cast<const __m128i*>(row)));
    };
    const auto apply = [coefficient](void* slot, auto wide) {
        if constexpr (PSQ_WIDTH == 16) {
            auto* p = reinterpret_cast<__m256i*>(slot);
            _mm256_store_si256(p, coefficient > 0
                ? _mm256_add_epi16(_mm256_load_si256(p), wide)
                : _mm256_sub_epi16(_mm256_load_si256(p), wide));
        } else {
            auto* p = reinterpret_cast<__m128i*>(slot);
            _mm_store_si128(p, coefficient > 0
                ? _mm_add_epi16(_mm_load_si128(p), wide)
                : _mm_sub_epi16(_mm_load_si128(p), wide));
        }
    };
    apply(accumulator.view[perspective], widen(row));
}

void psq_refresh_view(PsqAccumulator& accumulator, const Position& pos, Color perspective) {
    std::fill(std::begin(accumulator.view[perspective]),
              std::end(accumulator.view[perspective]), std::int16_t(0));
    accumulator.kingBucket[perspective] = std::uint8_t(own_king_bucket(
        perspective, pos.king_square(perspective), Active.kingBuckets));
    for (Bitboard remaining = pos.pieces(); remaining;) {
        const Square square = pop_lsb(remaining);
        psq_apply_view(accumulator, perspective, pos.piece_on(square), square, 1);
    }
}

}  // namespace

void psq_update(PsqAccumulator& accumulator, Piece piece, Square square, int coefficient) {
    if (!Available || piece == NO_PIECE || coefficient == 0) return;
    // Normal moves update both perspectives. If an own king crossed a bucket
    // boundary, Position calls psq_refresh_if_king_bucket_changed afterwards;
    // that overwrites just the temporarily old-bucket view.
    psq_apply_view(accumulator, WHITE, piece, square, coefficient);
    psq_apply_view(accumulator, BLACK, piece, square, coefficient);
}

void psq_refresh(PsqAccumulator& accumulator, const Position& pos) {
    accumulator = PsqAccumulator{};
    if (!Available) return;
    psq_refresh_view(accumulator, pos, WHITE);
    psq_refresh_view(accumulator, pos, BLACK);
}

void psq_refresh_if_king_bucket_changed(PsqAccumulator& accumulator,
                                        const Position& pos, Color king) {
    if (!Available || Active.kingBuckets == 1)
        return;
    const int wanted = own_king_bucket(king, pos.king_square(king), Active.kingBuckets);
    if (wanted != accumulator.kingBucket[king])
        psq_refresh_view(accumulator, pos, king);
}

int king_bucket_count() { return Available ? Active.kingBuckets : 0; }

Prediction predict(const Position& pos, Value boringScore, const Eval::Trace& trace,
                   const PsqAccumulator* accumulator) {
    if (!Available) return Prediction{};
    return Active.evaluate(pos, boringScore, trace, true, accumulator);
}

float last_decisive() {
#ifdef FORGE_DECISIVE_SEARCH
    return LastDecisive;
#else
    return 0.5f;
#endif
}

Value score(const Position& pos, Value boringScore, const Eval::Trace& trace,
            const PsqAccumulator* accumulator) {
    if (!Available) return VALUE_DRAW;
    const Prediction prediction =
        Active.evaluate(pos, boringScore, trace, false, accumulator);
#ifdef FORGE_DECISIVE_SEARCH
    // The interpolated sigmoid, not std::exp: this still runs at every node in
    // the experiment and is deliberately absent from release builds.
    LastDecisive = NONLINEAR.sigmoid(prediction.decisive);
#endif
    return prediction.score;
}

}  // namespace boring::ForgeFactor
