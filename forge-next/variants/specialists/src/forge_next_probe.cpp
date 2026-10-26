// FORGE 2 learning pilot. GPL-3.0-or-later.
// Synthetic parameters are confined to self-tests unless a model is loaded.
#include "forge_next_probe.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <immintrin.h>
#include "bitboard.h"
#include "movegen.h"
#include "position.h"
#include "uci.h"

namespace boring::ForgeNext {
// UCI changes are serialized with Search::stop(), like existing search options.
bool QuietPolicy = true;
int QuietPolicyWeight = 1024;
int QuietPolicyMinDepth = 4;
namespace {
#ifndef FORGE_NEXT_OPT
#define FORGE_NEXT_OPT 0
#endif
// Cumulative engineering arms; dimensions and synthetic parameters stay fixed.
// 0 reference, 1 packed dense kernel, 2 exact edge differences, 3 head cache.
constexpr int P = 128, W = 16, R = 32, H = 32, Q = 16;
constexpr int CONTACT_ROWS = 12 * 12 * 16 * 7 * 2;  // 32,256, no hashing
constexpr int INPUTS = 2 * P + 2 * R + 8;
constexpr int PAD_INPUTS = ((INPUTS + 31) / 32) * 32;
constexpr std::uint64_t SEED = 0x464f524745320033ULL;
int Mode = 0;   // UCI mutations are serialized with Search::stop().
bool Audit = false;
int LoadedKind = 0;
std::string ModelPath;
bool Telemetry = false;

std::uint64_t mix(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
int clip(int x) { return std::clamp(x, 0, 127); }
int code(Piece p, Color view) {
    return int(type_of(p)) - 1 + 6 * (color_of(p) != view);
}
int square(int s, Color view) { return s ^ (view == BLACK ? 56 : 0); }
int plane(Piece p, int s, Color view) { return code(p, view) * 64 + square(s, view); }
int bucket(int king, Color view) {
    const int s = square(king, view);
    return (s / 8) * 4 + std::min(s % 8, 7 - s % 8);
}
using Board = std::array<Piece, 64>;
struct Edge { std::uint8_t to; Piece blocker; };
struct Edges {
    std::array<Edge, 16> entries;
    unsigned size = 0;
    void add(int to, Piece blocker = NO_PIECE) {
        if (size == entries.size()) throw std::runtime_error("contact overflow");
        entries[size++] = {std::uint8_t(to), blocker};
    }
};

// Direction uses 8 queen directions and 8 knight displacements. Coordinates
// are canonicalized vertically for each colour, as in the placement encoder.
constexpr int DX[16] = {1,1,0,-1,-1,-1,0,1, 2,1,-1,-2,-2,-1,1,2};
constexpr int DY[16] = {0,1,1,1,0,-1,-1,-1, 1,2,2,1,-1,-2,-2,-1};
int edge_row(Piece src, int s, Piece dst, int t, bool blocked, Color view) {
    const int a = square(s, view), b = square(t, view);
    const int dx = b % 8 - a % 8, dy = b / 8 - a / 8;
    const int distance = std::max(std::abs(dx), std::abs(dy));
    const bool knight = std::abs(dx) * std::abs(dy) == 2;
    const int nx = knight ? dx : (dx > 0) - (dx < 0);
    const int ny = knight ? dy : (dy > 0) - (dy < 0);
    int dir = knight ? 8 : 0;
    const int end = knight ? 16 : 8;
    for (; dir < end; ++dir) if (DX[dir] == nx && DY[dir] == ny) break;
    if (dir == end || distance < 1 || distance > 7)
        throw std::runtime_error("invalid contact geometry");
    return ((((code(src, view) * 12 + code(dst, view)) * 16 + dir) * 7
             + distance - 1) * 2 + int(blocked));
}

struct ExtraHead {
    std::array<std::int8_t, INPUTS * H> hidden{};
    std::array<std::int8_t, H * 2> value{};
    std::array<std::int8_t, H * Q> query{};
#if FORGE_NEXT_OPT >= 1
    alignas(32) std::array<std::int8_t, PAD_INPUTS * H> packed{};
#endif
};
struct Weights {
    int head_count = 1;
    std::array<ExtraHead, 3> extra{};
    const std::int8_t* hidden_for(int h) const { return h ? extra[h-1].hidden.data() : hidden.data(); }
    const std::int8_t* value_for(int h) const { return h ? extra[h-1].value.data() : value.data(); }
    const std::int8_t* query_for(int h) const { return h ? extra[h-1].query.data() : query.data(); }
#if FORGE_NEXT_OPT >= 1
    const std::int8_t* packed_for(int h) const { return h ? extra[h-1].packed.data() : packed_hidden.data(); }
#endif
    std::vector<std::int8_t> placement, contacts;
    std::array<std::int8_t, 768 * 2 * W> local{};
    std::array<std::int8_t, 12 * 2 * W> blocker{};
    std::array<std::int8_t, INPUTS * H> hidden{};
#if FORGE_NEXT_OPT >= 1
    alignas(32) std::array<std::int8_t, PAD_INPUTS * H> packed_hidden{};
#endif
    std::array<std::int8_t, H * 2> value{};
    std::array<std::int8_t, H * Q> query{};
    std::array<std::int8_t, 768 * Q> from{}, to{};
    std::array<std::int8_t, 8 * Q> special{};
    std::uint64_t fingerprint = 0;
    std::size_t bytes = 0;
    Weights() : placement(32 * 768 * P), contacts(CONTACT_ROWS * 2 * W) {
        std::uint64_t rng = SEED;
        auto fill = [&](auto& a) {
            bytes += a.size();
            for (auto& x : a) {
                rng += 0x9e3779b97f4a7c15ULL;
                x = std::int8_t(int(mix(rng) % 17) - 8);
                fingerprint = mix(fingerprint ^ std::uint8_t(x));
            }
        };
        fill(placement); fill(contacts); fill(local); fill(blocker); fill(hidden);
        fill(value); fill(query); fill(from); fill(to); fill(special);
        pack();
    }
    void pack() {
#if FORGE_NEXT_OPT >= 1
        // Derived packing, not new parameters; zero padding never contributes.
        for (int j = 0; j < H; ++j) for (int i = 0; i < INPUTS; ++i)
            packed_hidden[j * PAD_INPUTS + i] = hidden[i * H + j];
        for (auto& e : extra) for (int j = 0; j < H; ++j) for (int i = 0; i < INPUTS; ++i)
            e.packed[j * PAD_INPUTS + i] = e.hidden[i * H + j];
#endif
    }
};
std::unique_ptr<Weights> Model;
const Weights& weights() {
    if (!Model) Model = std::make_unique<Weights>();
    return *Model;
}

struct State {
    Board board{};
    Bitboard occupied = 0;
    int king[2]{};
    alignas(32) std::int16_t placement[2][P]{};
    alignas(32) std::int16_t incoming[2][64][W]{};
    alignas(32) std::int16_t outgoing[2][64][W]{};
    alignas(32) std::int32_t relations[2][R]{};
    std::array<Edges, 64> edges{};
};
struct Stats {
    std::uint64_t marks = 0, syncs = 0, empty_syncs = 0, rebuilds = 0;
    std::uint64_t fallbacks = 0, king_refreshes = 0, heads = 0, value_calls = 0;
    std::uint64_t head_reuses = 0, policies = 0, candidates = 0, audits = 0;
    std::uint64_t edge_adds = 0, edge_removes = 0, checksum = 0;
    std::uint64_t orderings = 0, ordered_candidates = 0, ordering_changes = 0;
    std::uint64_t ordering_score_changes = 0, ordering_head_refreshes = 0;
    std::array<std::uint64_t, 65> dirty_squares{}, dirty_sources{}, dirty_nodes{};
    std::array<std::uint64_t, 65> sampled_sync_ns_log2{};
};
struct Context {
    const Position* owner = nullptr;
    bool valid = false, head_valid = false;
    Bitboard pending = ~Bitboard(0);
    int clock = -1;
    Key key = 0;
    State state{};
    alignas(32) std::int32_t hidden[H]{};
    int score = 0;
#if FORGE_NEXT_OPT >= 3
    struct HeadEntry {
        Key key = 0;
        int clock = -1, score = 0;
        bool valid = false;
        std::array<std::int32_t, H> hidden{};
    };
    // Game-ply slots retain ancestors through ordinary child searches. Null
    // moves share a slot, but key/clock tags prevent a false hit. Reset with C.
    std::array<HeadEntry, 256> head_cache{};
#endif
    Stats stats{};
};
thread_local Context C;

// Sample 1/64 syncs during the cost screen, not the expensive audit. Bins give
// ranges [2^n, 2^(n+1)) ns; the receipt does not pretend these are exact p95s.
struct SyncSample {
    bool active;
    std::chrono::steady_clock::time_point start;
    SyncSample() : active(!Audit && C.stats.syncs % 64 == 0) {
        if (active) start = std::chrono::steady_clock::now();
    }
    ~SyncSample() {
        if (active) {
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();
            const unsigned bin = std::bit_width(std::uint64_t(std::max<std::int64_t>(ns, 1))) - 1;
            ++C.stats.sampled_sync_ns_log2[std::min(bin, 64u)];
        }
    }
};

Board read_board(const Position& pos) {
    Board b;
    for (int s = 0; s < 64; ++s) b[s] = pos.piece_on(Square(s));
    return b;
}
Edges fast_edges(const Board& b, int s) {
    Edges out;
    const Piece pc = b[s];
    if (!pc) return out;
    const auto pt = type_of(pc);
    if (pt == BISHOP || pt == ROOK || pt == QUEEN) {
        for (int d = 0; d < 8; ++d) {
            const bool diagonal = DX[d] && DY[d];
            if ((pt == BISHOP && !diagonal) || (pt == ROOK && diagonal)) continue;
            int x = s % 8 + DX[d], y = s / 8 + DY[d];
            Piece first = NO_PIECE;
            while (x >= 0 && x < 8 && y >= 0 && y < 8) {
                const int t = y * 8 + x;
                if (b[t]) {
                    out.add(t, first);
                    if (first) break;
                    first = b[t];
                }
                x += DX[d]; y += DY[d];
            }
        }
    } else {
        const int start = pt == KNIGHT ? 8 : 0;
        const int end = pt == KNIGHT ? 16 : 8;
        for (int d = start; d < end; ++d) {
            if (pt == PAWN && !(std::abs(DX[d]) == 1
                 && DY[d] == (color_of(pc) == WHITE ? 1 : -1))) continue;
            const int x = s % 8 + DX[d], y = s / 8 + DY[d];
            if (x >= 0 && x < 8 && y >= 0 && y < 8 && b[y * 8 + x])
                out.add(y * 8 + x);
        }
    }
    return out;
}

// Independent reference: test every occupied source/target pair, then count
// intervening blockers. It does not use the incremental source invalidation or
// the ray-walking extractor above. Shared weight indexing is checked separately.
Edges reference_edges(const Board& b, int s) {
    Edges out;
    if (!b[s]) return out;
    const auto pt = type_of(b[s]);
    for (int t = 0; t < 64; ++t) {
        if (t == s || !b[t]) continue;
        const int dx = t % 8 - s % 8, dy = t / 8 - s / 8;
        const int ax = std::abs(dx), ay = std::abs(dy);
        bool hit = false;
        if (pt == PAWN) hit = ax == 1 && dy == (color_of(b[s]) == WHITE ? 1 : -1);
        if (pt == KNIGHT) hit = ax * ay == 2;
        if (pt == KING) hit = std::max(ax, ay) == 1;
        if (hit) { out.add(t); continue; }
        const bool ray = (pt == BISHOP && ax == ay)
            || (pt == ROOK && (!ax || !ay))
            || (pt == QUEEN && (ax == ay || !ax || !ay));
        if (!ray) continue;
        const int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
        int x = s % 8 + sx, y = s / 8 + sy, count = 0;
        Piece blocker = NO_PIECE;
        while (x != t % 8 || y != t / 8) {
            if (b[y * 8 + x]) { ++count; blocker = b[y * 8 + x]; }
            x += sx; y += sy;
        }
        if (count <= 1) out.add(t, blocker);
    }
    return out;
}
void pool(State& s, int node, int sign) {
    if (!s.board[node]) return;
    for (Color v : {WHITE, BLACK}) for (int k = 0; k < W; ++k) {
        const int a = clip(s.incoming[v][node][k] / 2);
        const int b = clip(s.outgoing[v][node][k] / 2);
        s.relations[v][k] += sign * a;
        s.relations[v][W + k] += sign * ((a * b) / 128);
    }
}
void base_local(State& s, Piece pc, int node, int sign) {
    if (!pc) return;
    const auto& m = weights();
    for (Color v : {WHITE, BLACK}) {
        const auto* row = &m.local[plane(pc, node, v) * 2 * W];
        for (int k = 0; k < W; ++k) {
            s.incoming[v][node][k] += sign * row[k];
            s.outgoing[v][node][k] += sign * row[W + k];
        }
    }
}
void apply_edge(State& s, const Board& board, int src, Edge e, int sign) {
    const auto& m = weights();
    for (Color v : {WHITE, BLACK}) {
        const auto* row = &m.contacts[edge_row(board[src], src, board[e.to], e.to,
                                             e.blocker != NO_PIECE, v) * 2 * W];
        const auto* blocker = e.blocker ? &m.blocker[code(e.blocker, v) * 2 * W] : nullptr;
        for (int k = 0; k < W; ++k) {
            s.incoming[v][e.to][k] += sign * (row[k] + (blocker ? blocker[k] : 0));
            s.outgoing[v][src][k] += sign * (row[W+k] + (blocker ? blocker[W+k] : 0));
        }
    }
}
void place(State& s, Piece pc, int sq, Color v, int sign) {
    if (!pc) return;
    const auto* row = &weights().placement[(s.king[v] * 768 + plane(pc, sq, v)) * P];
    for (int k = 0; k < P; ++k) s.placement[v][k] += sign * row[k];
}
void refresh_view(State& s, Color v) {
    std::fill(std::begin(s.placement[v]), std::end(s.placement[v]), 0);
    for (Bitboard b = s.occupied; b;) {
        const int sq = pop_lsb(b); place(s, s.board[sq], sq, v, 1);
    }
}
State full(const Position& pos, bool reference) {
    State s;
    s.board = read_board(pos); s.occupied = pos.pieces();
    for (Color v : {WHITE, BLACK}) {
        s.king[v] = bucket(pos.king_square(v), v); refresh_view(s, v);
    }
    if (Mode < 2) return s;
    for (Bitboard b = s.occupied; b;) {
        const int sq = pop_lsb(b);
        base_local(s, s.board[sq], sq, 1);
        s.edges[sq] = reference ? reference_edges(s.board, sq) : fast_edges(s.board, sq);
        for (unsigned j = 0; j < s.edges[sq].size; ++j)
            apply_edge(s, s.board, sq, s.edges[sq].entries[j], 1);
    }
    for (Bitboard b = s.occupied; b;) pool(s, pop_lsb(b), 1);
    return s;
}
void check(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(std::string("FORGE2 probe: ") + why);
}
void compare(const Position& pos) {
    const State ref = full(pos, true);
    const auto& s = C.state;
    check(s.board == ref.board && s.occupied == ref.occupied, "board mismatch");
    for (Color v : {WHITE, BLACK}) {
        check(s.king[v] == ref.king[v], "king bucket mismatch");
        for (int k = 0; k < P; ++k) check(s.placement[v][k] == ref.placement[v][k], "placement mismatch");
        for (int k = 0; k < R; ++k) check(s.relations[v][k] == ref.relations[v][k], "relation pool mismatch");
        for (int n = 0; n < 64; ++n) for (int k = 0; k < W; ++k) {
            check(s.incoming[v][n][k] == ref.incoming[v][n][k], "incoming mismatch");
            check(s.outgoing[v][n][k] == ref.outgoing[v][n][k], "outgoing mismatch");
        }
    }
    if (Mode >= 2) for (int n = 0; n < 64; ++n) {
        check(s.edges[n].size == ref.edges[n].size, "edge count mismatch");
        for (unsigned i = 0; i < s.edges[n].size; ++i) {
            auto a = s.edges[n].entries[i]; bool found = false;
            for (unsigned j = 0; j < ref.edges[n].size; ++j) {
                auto b = ref.edges[n].entries[j];
                found |= a.to == b.to && a.blocker == b.blocker;
            }
            check(found, "edge mismatch");
        }
    }
    ++C.stats.audits;
}

// Dependencies are deliberately conservative: all sliders geometrically aligned
// with a changed square, plus leapers/pawns that touch it. This includes x-rays
// through a changed blocker and target-identity changes on occupied squares.
Bitboard sources(const State& old, const Position& pos, Bitboard changed) {
    const Bitboard candidates = old.occupied | pos.pieces();
    Bitboard out = changed;
    for (Bitboard b = candidates; b;) {
        const Square sq = pop_lsb(b);
        for (Piece pc : {old.board[sq], pos.piece_on(sq)}) {
            if (!pc) continue;
            Bitboard deps = 0;
            switch (type_of(pc)) {
            case PAWN: deps = pawn_attacks_bb(color_of(pc), sq); break;
            case KNIGHT: deps = attacks_bb<KNIGHT>(sq); break;
            case KING: deps = attacks_bb<KING>(sq); break;
            case BISHOP: deps = attacks_bb<BISHOP>(sq, 0); break;
            case ROOK: deps = attacks_bb<ROOK>(sq, 0); break;
            case QUEEN: deps = attacks_bb<BISHOP>(sq, 0) | attacks_bb<ROOK>(sq, 0); break;
            default: break;
            }
            if (deps & changed) out |= square_bb(sq);
        }
    }
    return out;
}
#if FORGE_NEXT_OPT >= 2
bool matching_edge(const Board& old, const Board& next, int src, Edge a,
                   const Edges& other) {
    if (old[src] != next[src] || old[a.to] != next[a.to]) return false;
    for (unsigned j = 0; j < other.size; ++j)
        if (a.to == other.entries[j].to && a.blocker == other.entries[j].blocker)
            return true;
    return false;
}
#endif
void sync(const Position& pos) {
    ++C.stats.syncs;
    SyncSample sample;
    if (!C.valid || C.owner != &pos) {
        C.state = full(pos, false); C.owner = &pos; C.valid = true;
        C.pending = 0; C.head_valid = false; ++C.stats.rebuilds;
        if (Audit) compare(pos);
        return;
    }
    auto& s = C.state;
    Bitboard changed = 0;
    for (Bitboard b = C.pending; b;) {
        const Square sq = pop_lsb(b);
        if (s.board[sq] != pos.piece_on(sq)) changed |= square_bb(sq);
    }
    C.pending = 0;
    ++C.stats.dirty_squares[std::popcount(changed)];
    if (!changed) { ++C.stats.empty_syncs; if (Audit) compare(pos); return; }
    C.head_valid = false;
    const Bitboard srcs = Mode >= 2 ? sources(s, pos, changed) : 0;
    ++C.stats.dirty_sources[std::popcount(srcs)];
    if (std::popcount(changed) > 16 || std::popcount(srcs) > 24) {
        s = full(pos, false); ++C.stats.fallbacks;
        if (Audit) compare(pos);
        return;
    }
    Board next = s.board;
    for (Bitboard b = changed; b;) { const Square sq = pop_lsb(b); next[sq] = pos.piece_on(sq); }
    std::array<Edges, 64> next_edges; // only srcs are initialized/read below
#if FORGE_NEXT_OPT >= 2
    std::array<unsigned, 64> removed, added; // masks are initialized for srcs
    Bitboard affected = changed;
#else
    Bitboard affected = changed | srcs;
#endif
    if (Mode >= 2) {
        for (Bitboard b = srcs; b;) {
            const int sq = pop_lsb(b);
            next_edges[sq] = fast_edges(next, sq);
#if FORGE_NEXT_OPT >= 2
            removed[sq] = added[sq] = 0;
            for (unsigned j = 0; j < s.edges[sq].size; ++j) {
                const auto e = s.edges[sq].entries[j];
                if (!matching_edge(s.board, next, sq, e, next_edges[sq])) {
                    removed[sq] |= 1u << j;
                    affected |= square_bb(Square(sq)) | square_bb(Square(e.to));
                }
            }
            for (unsigned j = 0; j < next_edges[sq].size; ++j) {
                const auto e = next_edges[sq].entries[j];
                if (!matching_edge(next, s.board, sq, e, s.edges[sq])) {
                    added[sq] |= 1u << j;
                    affected |= square_bb(Square(sq)) | square_bb(Square(e.to));
                }
            }
#else
            for (unsigned j = 0; j < s.edges[sq].size; ++j) affected |= square_bb(Square(s.edges[sq].entries[j].to));
            for (unsigned j = 0; j < next_edges[sq].size; ++j) affected |= square_bb(Square(next_edges[sq].entries[j].to));
#endif
        }
        ++C.stats.dirty_nodes[std::popcount(affected)];
        for (Bitboard b = affected; b;) pool(s, pop_lsb(b), -1);
        for (Bitboard b = srcs; b;) {
            const int sq = pop_lsb(b);
            for (unsigned j = 0; j < s.edges[sq].size; ++j) {
#if FORGE_NEXT_OPT >= 2
                if (!(removed[sq] & (1u << j))) continue;
#endif
                apply_edge(s, s.board, sq, s.edges[sq].entries[j], -1); ++C.stats.edge_removes;
            }
        }
        for (Bitboard b = changed; b;) {
            const int sq = pop_lsb(b); base_local(s, s.board[sq], sq, -1); base_local(s, next[sq], sq, 1);
        }
    }
    for (Color v : {WHITE, BLACK}) {
        const int wanted = bucket(pos.king_square(v), v);
        if (wanted == s.king[v]) for (Bitboard b = changed; b;) {
            const int sq = pop_lsb(b); place(s, s.board[sq], sq, v, -1); place(s, next[sq], sq, v, 1);
        }
    }
    s.board = next; s.occupied = pos.pieces();
    for (Color v : {WHITE, BLACK}) {
        const int wanted = bucket(pos.king_square(v), v);
        if (wanted != s.king[v]) { s.king[v] = wanted; refresh_view(s, v); ++C.stats.king_refreshes; }
    }
    if (Mode >= 2) {
        for (Bitboard b = srcs; b;) {
            const int sq = pop_lsb(b); s.edges[sq] = next_edges[sq];
            for (unsigned j = 0; j < s.edges[sq].size; ++j) {
#if FORGE_NEXT_OPT >= 2
                if (!(added[sq] & (1u << j))) continue;
#endif
                apply_edge(s, s.board, sq, s.edges[sq].entries[j], 1); ++C.stats.edge_adds;
            }
        }
        for (Bitboard b = affected; b;) pool(s, pop_lsb(b), 1);
    }
    if (Audit) compare(pos);
}

void scalar_dense(const std::int32_t* x, const std::int8_t* weights,
                  std::int32_t* out) {
    std::fill(out, out + H, 0);
    for (int i = 0; i < INPUTS; ++i) for (int j = 0; j < H; ++j)
        out[j] += x[i] * weights[i * H + j];
}
#if FORGE_NEXT_OPT >= 1
template<bool SkipReserved = false>
void packed_dense(const std::int32_t* x, const std::int8_t* packed,
                  std::int32_t* out) {
    alignas(32) std::uint8_t bytes[PAD_INPUTS]{};
    for (int i = 0; i < INPUTS; ++i) {
        // Defensive scalar fallback keeps arbitrary metadata inputs exact.
        // Supported chess features are normally in [0,127].
        if (x[i] < 0 || x[i] > 127) {
            for (int j = 0; j < H; ++j) {
                out[j] = 0;
                for (int k = 0; k < INPUTS; ++k)
                    out[j] += x[k] * packed[j * PAD_INPUTS + k];
            }
            return;
        }
        bytes[i] = std::uint8_t(x[i]);
    }
    const __m256i ones = _mm256_set1_epi16(1);
    for (int j = 0; j < H; ++j) {
        __m256i sum = _mm256_setzero_si256();
        // Keep packing/range checks unchanged. Only the multiply loop skips
        // blocks 8/9, whose inputs are zero in placement-only Mode 1.
        static_assert(P == 128 && R == 32 && INPUTS == 328 && PAD_INPUTS == 352);
        constexpr int Blocks = PAD_INPUTS / 32 - (SkipReserved ? 2 : 0);
        for (int block = 0; block < Blocks; ++block) {
            const int i = 32 * (block + (SkipReserved && block >= 8 ? 2 : 0));
            const auto a = _mm256_load_si256(reinterpret_cast<const __m256i*>(bytes + i));
            const auto b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(packed + j * PAD_INPUTS + i));
            // Max pair: 2*127*127=32258; min: 2*127*(-128)=-32512.
            // Both fit int16, so maddubs saturation cannot alter the result.
            const auto pair = _mm256_maddubs_epi16(a, b);
            sum = _mm256_add_epi32(sum, _mm256_madd_epi16(pair, ones));
        }
        alignas(32) std::int32_t lanes[8];
        _mm256_store_si256(reinterpret_cast<__m256i*>(lanes), sum);
        out[j] = 0;
        for (int lane : lanes) out[j] += lane;
    }
}
#endif
int selected_head(const Position& pos) {
    return weights().head_count == 1 ? 0 : std::clamp((std::popcount(pos.pieces()) - 1) / 8, 0, 3);
}
void compute_hidden(const Position& pos, std::int32_t* hidden) {
    const auto& m = weights();
    alignas(32) std::int32_t x[INPUTS]{};
    const Color us = pos.side_to_move();
    for (int k = 0; k < P; ++k) {
        x[k] = clip(C.state.placement[us][k] / 2 + 32);
        x[P + k] = clip(C.state.placement[~us][k] / 2 + 32);
    }
    for (int k = 0; k < R; ++k) {
        x[2*P+k] = clip(C.state.relations[us][k] / 8);
        x[2*P+R+k] = clip(C.state.relations[~us][k] / 8);
    }
    int z = 2*P+2*R;
    x[z++] = std::clamp(pos.rule50_count(), 0, 255) / 2;
    x[z++] = pos.phase();
    x[z++] = (pos.castling_rights(us) >> (2 * int(us))) * 16;
    x[z++] = (pos.castling_rights(~us) >> (2 * int(~us))) * 16;
    x[z++] = pos.ep_square() == SQ_NONE ? 0 : square(pos.ep_square(), us) + 1;
    x[z++] = std::popcount(pos.pieces());
    x[z++] = pos.checkers() != 0;
    x[z++] = 32;
#if FORGE_NEXT_OPT >= 1
    if (Mode == 1) {
        if (Audit) for (int i = 2*P; i < 2*P+2*R; ++i)
            check(x[i] == 0, "nonzero placement-only reserved input");
        packed_dense<true>(x, m.packed_for(selected_head(pos)), hidden);
    } else {
        // Future relational models must retain all of their input columns.
        packed_dense(x, m.packed_for(selected_head(pos)), hidden);
    }
    if (Audit) {
        std::int32_t reference[H]; scalar_dense(x, m.hidden_for(selected_head(pos)), reference);
        for (int j = 0; j < H; ++j) check(hidden[j] == reference[j], "dense kernel mismatch");
    }
#else
    scalar_dense(x, m.hidden_for(selected_head(pos)), hidden);
#endif
    for (int j = 0; j < H; ++j) hidden[j] = clip(hidden[j] / 64);
}
int head_score(const std::int32_t* hidden, int selected) {
    const auto& m = weights();
    const auto* value_weights = m.value_for(selected);
    int logits[2]{};
    for (int j = 0; j < H; ++j) for (int k = 0; k < 2; ++k)
        logits[k] += hidden[j] * value_weights[j * 2 + k];
    // Same expected-score logit and 187 scale as netB, evaluated stably in
    // the tails. Do not cap probabilities at 1e-12 as the synthetic probe did.
    auto logadd = [](double a, double b) {
        return std::max(a,b) + std::log1p(std::exp(-std::abs(a-b)));
    };
    const double a = double(logits[0])/256.0, c = double(logits[1])/256.0;
    const double t = logadd(0.0,a), logtwo = std::log(2.0);
    const double score = 187.0 * (logadd(t,logtwo+a+c) - logadd(t,logtwo+c));
    return int(std::clamp<long>(std::lround(score), -12000, 12000));
}
void head(const Position& pos) {
    if (C.owner != &pos) reset(pos);
#if FORGE_NEXT_OPT >= 3
    auto& entry = C.head_cache[unsigned(pos.game_ply()) & 255u];
    if (entry.valid && entry.key == pos.key() && entry.clock == pos.rule50_count()) {
        std::copy(entry.hidden.begin(), entry.hidden.end(), C.hidden); C.score = entry.score;
        // The working board may still describe a child: do not let the old
        // single-entry validity tag describe this newly copied parent head.
        C.head_valid = false;
        if (Audit) {
            sync(pos);
            std::int32_t reference[H]; compute_hidden(pos, reference);
            for (int j = 0; j < H; ++j) check(C.hidden[j] == reference[j], "cached hidden mismatch");
            check(C.score == head_score(reference, selected_head(pos)), "cached score mismatch");
        }
        ++C.stats.head_reuses; return;
    }
#endif
    sync(pos);
    if (C.head_valid && C.key == pos.key() && C.clock == pos.rule50_count()) {
        ++C.stats.head_reuses; return;
    }
    compute_hidden(pos, C.hidden); C.score = head_score(C.hidden, selected_head(pos));
    C.key = pos.key(); C.clock = pos.rule50_count(); C.head_valid = true; ++C.stats.heads;
#if FORGE_NEXT_OPT >= 3
    entry.valid = true; entry.key = C.key; entry.clock = C.clock; entry.score = C.score;
    std::copy(std::begin(C.hidden), std::end(C.hidden), entry.hidden.begin());
#endif
}
void histogram(const std::array<std::uint64_t,65>& h) {
    std::cout << '[';
    for (int i = 0; i < 65; ++i) std::cout << (i ? "," : "") << h[i];
    std::cout << ']';
}
} // namespace

void set_mode(int mode) {
    if (mode < 0 || mode > 3) throw std::runtime_error("ProbeMode must be 0..3");
    if (LoadedKind && mode != LoadedKind) throw std::runtime_error("Model variant is fixed by its header");
    Mode = mode; C = Context{}; if (Mode) (void)weights();
}
bool loaded() { return LoadedKind != 0; }
const std::string& model_path() { return ModelPath; }
void set_telemetry(bool enabled) { Telemetry = enabled; }
bool telemetry() { return Telemetry; }
void unload() { LoadedKind = Mode = 0; Model.reset(); C = Context{}; }
bool load_model(const std::string& path, std::string& error) {
    // Validate everything before replacing the live model. Caller stops search
    // and clears the TT; iterative_deepening resets its thread-local state.
    try {
        constexpr std::size_t N = 4238656, EXTRA = 3 * (INPUTS*H + H*2 + H*Q);
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) throw std::runtime_error("cannot open model");
        const auto file_size = f.tellg();
        if (file_size != std::streamoff(N+32) && file_size != std::streamoff(N+EXTRA+32))
            throw std::runtime_error("wrong model size");
        f.seekg(0); std::vector<unsigned char> bytes(static_cast<std::size_t>(file_size));
        if (!f.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
            throw std::runtime_error("incomplete model read");
        const unsigned char magic[8] = {'F','G','2','N','E','T',bytes[6],0};
        if (bytes[6] != '1' && bytes[6] != '2') throw std::runtime_error("wrong model version");
        if (!std::equal(std::begin(magic), std::end(magic), bytes.begin()))
            throw std::runtime_error("wrong model magic");
        auto u32 = [&](int p) { return std::uint32_t(bytes[p]) | (std::uint32_t(bytes[p+1]) << 8)
            | (std::uint32_t(bytes[p+2]) << 16) | (std::uint32_t(bytes[p+3]) << 24); };
        const int kind = int(u32(12));
        const bool multiple = bytes[6] == '2';
        const std::size_t expected = N + (multiple ? EXTRA : 0);
        if (u32(8) != (multiple ? 2u : 1u) || kind != 1 || u32(16) != P ||
            u32(20) != expected || bytes.size() != expected+32 || u32(28) != (multiple ? 4u : 1u))
            throw std::runtime_error("unsupported model layout/algebra");
        std::uint32_t crc = 0xffffffffu;
        for (std::size_t i = 32; i < bytes.size(); ++i) {
            crc ^= bytes[i];
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
        if ((crc ^ 0xffffffffu) != u32(24)) throw std::runtime_error("model CRC mismatch");
        auto next = std::make_unique<Weights>();
        std::size_t offset = 32; next->fingerprint = 0;
        auto copy = [&](auto& a) {
            for (auto& x : a) {
                x = std::bit_cast<std::int8_t>(bytes[offset++]);
                next->fingerprint = mix(next->fingerprint ^ std::uint8_t(x));
            }
        };
        copy(next->placement); copy(next->contacts); copy(next->local); copy(next->blocker);
        copy(next->hidden); copy(next->value); copy(next->query); copy(next->from); copy(next->to); copy(next->special);
        next->head_count = multiple ? 4 : 1;
        if (multiple) for (auto& e : next->extra) { copy(e.hidden); copy(e.value); copy(e.query); }
        next->bytes = expected;
        if (offset != bytes.size()) throw std::runtime_error("internal model layout disagreement");
        next->pack(); Model = std::move(next); LoadedKind = Mode = kind; ModelPath = path; C = Context{};
        error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
Value evaluate(const Position& pos) {
    if (!loaded()) throw std::runtime_error("FORGE2 model missing");
    observe_value(pos); return Value(C.score);
}
void cost(const Position& pos, unsigned iterations) {
    if (!loaded() || iterations < 100 || iterations > 1000000) throw std::runtime_error("invalid forge2cost");
    sync(pos); const int selected = selected_head(pos); std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < iterations; ++i) {
        asm volatile("" ::: "memory"); // benchmark only: prevent loop-invariant elimination
        std::int32_t h[H]; compute_hidden(pos,h);
        checksum += std::uint64_t(std::int64_t(head_score(h,selected)));
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::cout << "forge2cost {\"head\":" << selected << ",\"iterations\":" << iterations
      << ",\"seconds\":" << seconds << ",\"checksum\":" << checksum << "}" << std::endl;
}
void inspect(const Position& pos) {
    if (!loaded()) { std::cout << "info string error FORGE2 model missing" << std::endl; return; }
    head(pos);
    const auto& m = weights();
    const int selected = selected_head(pos);
    const auto* value_weights = m.value_for(selected);
    const auto* query_weights = m.query_for(selected);
    int logits[2]{}; std::int32_t query[Q]{};
    for (int j = 0; j < H; ++j) {
        for (int k = 0; k < 2; ++k) logits[k] += C.hidden[j] * value_weights[j*2+k];
        for (int k = 0; k < Q; ++k) query[k] += C.hidden[j] * query_weights[j*Q+k];
    }
    for (auto& q : query) q = std::clamp(q/64, -127, 127);
    std::cout << "forge2inspect {\"kind\":" << LoadedKind << ",\"score\":" << C.score << ",\"hidden\":[";
    for (int j = 0; j < H; ++j) std::cout << (j ? "," : "") << C.hidden[j];
    std::cout << "],\"raw_value_logits\":[" << logits[0] << ',' << logits[1] << "],\"quiet_policy\":{";
    bool first = true; const Color us = pos.side_to_move();
    for (const ExtMove& e : MoveList<LEGAL>(pos)) {
        const Move move = e;
        if (type_of(move) == EN_PASSANT || (type_of(move) != CASTLING && pos.piece_on(to_sq(move)))) continue;
        const Piece pc = pos.piece_on(from_sq(move));
        const int kind = type_of(move) == PROMOTION ? 2 + int(promotion_type(move)) : int(type_of(move)) >> 14;
        int logit = 0;
        for (int k = 0; k < Q; ++k) logit += query[k] * (m.from[plane(pc,from_sq(move),us)*Q+k]
            + m.to[plane(pc,to_sq(move),us)*Q+k] + m.special[kind*Q+k]);
        std::string uci = UCI::square(from_sq(move)) + UCI::square(to_sq(move));
        if (type_of(move) == PROMOTION) uci += " pnbrqk"[promotion_type(move)];
        std::cout << (first ? "" : ",") << '"' << uci << "\":" << logit; first = false;
    }
    std::cout << "}}" << std::endl;
}
void set_audit(bool enabled) { Audit = enabled; }
void reset(const Position& pos) { if (Mode) { C = Context{}; C.owner = &pos; } }
void mark_move(const Position& pos, Move m) {
    if (!Mode) return;
    if (C.owner != &pos) reset(pos);
    const Square from = from_sq(m), to = to_sq(m);
    Bitboard dirty = square_bb(from) | square_bb(to);
    const Color us = pos.side_to_move();
    if (type_of(m) == EN_PASSANT) dirty |= square_bb(to - pawn_push(us));
    if (type_of(m) == CASTLING) {
        dirty |= square_bb(castling_king_to(us, to > from));
        dirty |= square_bb(castling_rook_to(us, to > from));
    }
    C.pending |= dirty; ++C.stats.marks;
}
void observe_value(const Position& pos) {
    if (!Mode) return;
    ++C.stats.value_calls; head(pos);
    // Consume outputs once per request, independent of cache/kernel strategy.
    C.stats.checksum = mix(C.stats.checksum ^ std::uint64_t(std::int64_t(C.score)) ^ pos.key());
}
void observe_quiets(const Position& pos, int depth, const ExtMove* begin, const ExtMove* end) {
    if (Mode != 3 || depth < 4 || pos.checkers() || begin == end) return;
    head(pos);
    const auto& m = weights();
    const auto* query_weights = m.query_for(selected_head(pos));
    alignas(32) std::int32_t query[Q]{};
    for (int j = 0; j < H; ++j) for (int k = 0; k < Q; ++k)
        query[k] += C.hidden[j] * query_weights[j * Q + k];
    for (auto& q : query) q = std::clamp(q / 64, -127, 127);
    const Color us = pos.side_to_move();
    for (auto it = begin; it != end; ++it) {
        const Move move = *it;
        const Piece pc = pos.piece_on(from_sq(move));
        const auto* a = &m.from[plane(pc, from_sq(move), us) * Q];
        const auto* b = &m.to[plane(pc, to_sq(move), us) * Q];
        const int kind = type_of(move) == PROMOTION ? 2 + int(promotion_type(move))
                                                  : int(type_of(move)) >> 14;
        const auto* special = &m.special[kind * Q];
        int logit = 0;
        for (int k = 0; k < Q; ++k) logit += query[k] * (a[k] + b[k] + special[k]);
        C.stats.checksum = mix(C.stats.checksum ^ std::uint64_t(std::int64_t(logit)) ^ std::uint64_t(move));
        ++C.stats.candidates;
    }
    ++C.stats.policies;
}
void order_quiets(const Position& pos, int depth, ExtMove* begin, ExtMove* end) {
    // Training targets exclude checks and captures. This is called lazily at
    // QUIET_INIT, so TT/capture/refutation cutoffs incur no policy computation.
    if (!QuietPolicy || !QuietPolicyWeight || !loaded() || depth < QuietPolicyMinDepth
        || pos.checkers() || end - begin < 2) return;
    const auto heads_before = C.stats.heads;
    // Other move stages may already have searched children. Refresh/synchronize
    // using the actual parent board and halfmove clock; never reuse C.hidden blindly.
    head(pos);
    C.stats.ordering_head_refreshes += C.stats.heads - heads_before;
    const auto& m = weights();
    const auto* query_weights = m.query_for(selected_head(pos));
    std::int32_t query[Q]{};
    for (int j = 0; j < H; ++j) for (int k = 0; k < Q; ++k)
        query[k] += C.hidden[j] * query_weights[j * Q + k];
    for (auto& q : query) q = std::clamp(q / 64, -127, 127);
    std::int32_t logits[MAX_MOVES];
    std::int64_t sum = 0;
    const Color us = pos.side_to_move();
    const Move old_first = std::max_element(begin, end)->move;
    for (auto it = begin; it != end; ++it) {
        const Move move = *it;
        const Piece pc = pos.moved_piece(move);
        const int kind = type_of(move) == PROMOTION ? 2 + int(promotion_type(move))
                                                  : int(type_of(move)) >> 14;
        int raw = 0;
        for (int k = 0; k < Q; ++k)
            raw += query[k] * (m.from[plane(pc, from_sq(move), us) * Q + k]
                            + m.to[plane(pc, to_sq(move), us) * Q + k]
                            + m.special[kind * Q + k]);
        logits[it - begin] = raw;
        sum += raw;
    }
    // Raw logits / 256 were used during training. Centering removes their
    // arbitrary shared offset. A cheap softsign bounds the influence without
    // softmax or exponentials, while retaining confidence differences.
    const std::int64_t mean = sum / (end - begin);
    for (auto it = begin; it != end; ++it) {
        const std::int64_t d = logits[it - begin] - mean;
        const int bonus = int(std::int64_t(QuietPolicyWeight) * d / (256 + std::abs(d)));
        it->value += bonus;
        C.stats.ordering_score_changes += bonus != 0;
    }
    ++C.stats.orderings;
    C.stats.ordered_candidates += end - begin;
    C.stats.ordering_changes += std::max_element(begin, end)->move != old_first;
}
void report() {
    if (!Mode || !Telemetry) return;
    const auto& s = C.stats;
    std::cout << "info string forge2probe {\"mode\":" << Mode
      << ",\"optimization_arm\":" << FORGE_NEXT_OPT
      << ",\"synthetic\":" << (loaded() ? "false" : "true") << ",\"audit\":" << (Audit ? "true" : "false")
      << ",\"weight_bytes\":" << weights().bytes << ",\"weight_fingerprint\":" << weights().fingerprint
      << ",\"derived_packing_bytes\":" << (FORGE_NEXT_OPT >= 1 ? 4 * PAD_INPUTS * H : 0)
      << ",\"context_bytes\":" << sizeof(Context) << ",\"state_bytes\":" << sizeof(State)
      << ",\"marks\":" << s.marks << ",\"syncs\":" << s.syncs << ",\"empty_syncs\":" << s.empty_syncs
      << ",\"rebuilds\":" << s.rebuilds << ",\"fallbacks\":" << s.fallbacks
      << ",\"king_refreshes\":" << s.king_refreshes << ",\"value_calls\":" << s.value_calls
      << ",\"heads\":" << s.heads << ",\"head_reuses\":" << s.head_reuses
      << ",\"policies\":" << s.policies << ",\"candidates\":" << s.candidates
      << ",\"quiet_policy_calls\":" << s.orderings
      << ",\"quiet_policy_candidates\":" << s.ordered_candidates
      << ",\"quiet_policy_first_changes\":" << s.ordering_changes
      << ",\"quiet_policy_score_changes\":" << s.ordering_score_changes
      << ",\"quiet_policy_head_refreshes\":" << s.ordering_head_refreshes
      << ",\"edge_adds\":" << s.edge_adds << ",\"edge_removes\":" << s.edge_removes
      << ",\"audits\":" << s.audits << ",\"checksum\":" << s.checksum << ",\"dirty_squares\":";
    histogram(s.dirty_squares); std::cout << ",\"dirty_sources\":"; histogram(s.dirty_sources);
    std::cout << ",\"dirty_nodes\":"; histogram(s.dirty_nodes);
    std::cout << ",\"sampled_sync_ns_log2\":"; histogram(s.sampled_sync_ns_log2);
    std::cout << '}' << std::endl;
}

bool self_test() {
    const int saved_mode = Mode; const bool saved_audit = Audit;
    const bool saved_policy = QuietPolicy;
    const int saved_weight = QuietPolicyWeight, saved_min_depth = QuietPolicyMinDepth;
    try {
        QuietPolicy = true; QuietPolicyWeight = 1024; QuietPolicyMinDepth = 4;
#if FORGE_NEXT_OPT >= 1
        // Signed extremes exercise maddubs saturation bounds; sentinel padding
        // and out-of-domain metadata exercise exact scalar fallback.
        std::array<std::int8_t, INPUTS * H> scalar{};
        std::array<std::int8_t, PAD_INPUTS * H> packed{};
        std::int32_t inputs[INPUTS], a[H], b[H];
        for (int sample = 0; sample < 12; ++sample) {
            packed.fill(0);
            for (int i = 0; i < INPUTS; ++i) {
                inputs[i] = sample < 4 ? 127 : ((i * 37 + sample * 19) & 127);
                for (int j = 0; j < H; ++j) {
                    const int w = sample == 0 ? 127 : sample == 1 ? -128
                        : sample == 2 ? (i % 2 ? 127 : -128)
                        : int(mix(std::uint64_t(i * H + j + sample)) & 255) - 128;
                    scalar[i * H + j] = packed[j * PAD_INPUTS + i] = std::int8_t(w);
                }
            }
            if (sample == 10) inputs[17] = 255;
            if (sample == 11) inputs[17] = -1;
            scalar_dense(inputs, scalar.data(), a); packed_dense(inputs, packed.data(), b);
            for (int j = 0; j < H; ++j) check(a[j] == b[j], "boundary dense mismatch");
            // Exercise the specialized kernel with nonzero reserved weights,
            // zero reserved INPUTS, live metadata, padding, signed extremes,
            // and the same out-of-range cases that require scalar fallback.
            for (int i = 2*P; i < 2*P+2*R; ++i) inputs[i] = 0;
            scalar_dense(inputs, scalar.data(), a);
            packed_dense<true>(inputs, packed.data(), b);
            for (int j = 0; j < H; ++j)
                check(a[j] == b[j], "reserved-block skip mismatch");
        }
#endif
        set_mode(LoadedKind ? LoadedKind : 3); set_audit(true);
        struct Fixture { const char* fen; bool chess960; };
        const Fixture fixtures[] = {
          {StartFEN.c_str(), false},
          {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", false},
          {"4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", false},
          {"r6k/1P6/8/8/8/8/6p1/4K3 w - - 0 1", false},
          {"4k3/8/8/8/8/8/8/6KR w H - 0 1", true},
          {"4k3/8/8/8/8/8/8/5KR1 w G - 0 1", true},
          {"4k3/8/8/8/8/8/8/1R1K4 w B - 0 1", true},
          {"4k3/8/8/8/8/3P4/2B5/4K3 w - - 0 1", false},
          {"1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1", false},
          {"6k1/6p1/6Pp/ppp5/3pP3/2P1P1P1/PP5P/K7 w - - 0 1", false}
        };
        std::uint64_t checks = 0, ep = 0, castle = 0, promo = 0, nulls = 0, rng = SEED;
        for (const auto& f : fixtures) {
            Position pos; std::deque<StateInfo> stack(1);
            pos.set(f.fen, &stack.back(), f.chess960); reset(pos); observe_value(pos); ++checks;
            const auto rootfen = pos.fen();
            auto ordered_quiets = [&]() {
                std::vector<ExtMove> moves;
                if (!pos.checkers())
                    for (const auto& e : MoveList<QUIETS>(pos)) moves.push_back({e.move, 0});
                if (!moves.empty()) order_quiets(pos, 4, moves.data(), moves.data() + moves.size());
                return moves;
            };
            const auto root_order = ordered_quiets();
            // Exercise every root move, including all underpromotions and 960 swaps.
            for (const auto& e : MoveList<LEGAL>(pos)) {
                const Move move = e;
                ep += type_of(move) == EN_PASSANT; castle += type_of(move) == CASTLING;
                promo += type_of(move) == PROMOTION;
                StateInfo child; pos.do_move(move, child); observe_value(pos); ++checks;
                pos.undo_move(move);
                // Policy is requested while the working neural state still
                // describes the child. This is how lazy quiet generation can
                // encounter it after earlier captures/refutations were searched.
                const auto restored_order = ordered_quiets();
                check(restored_order.size() == root_order.size(), "policy make/unmake move count");
                for (std::size_t i = 0; i < root_order.size(); ++i)
                    check(restored_order[i].move == root_order[i].move
                          && restored_order[i].value == root_order[i].value,
                          "policy used a stale child head");
                observe_value(pos); ++checks;
                check(pos.fen() == rootfen, "make/unmake changed root FEN");
            }
            std::vector<Move> history;
            for (int ply = 0; ply < 96; ++ply) {
                MoveList<LEGAL> legal(pos); if (!legal.size()) break;
                rng = mix(rng + 0x9e3779b97f4a7c15ULL);
                const Move move = legal.begin()[rng % legal.size()];
                stack.emplace_back(); pos.do_move(move, stack.back()); history.push_back(move);
                // Deliberately skip synchronization across several make/undo operations.
                if (ply % 5 == 0) { observe_value(pos); ++checks; }
                if (ply % 11 == 0 && !pos.checkers()) {
                    StateInfo ns; pos.do_null_move(ns); observe_value(pos); ++checks;
                    pos.undo_null_move(); observe_value(pos); ++checks; ++nulls;
                }
            }
            while (!history.empty()) {
                const auto move = history.back(); history.pop_back(); pos.undo_move(move); stack.pop_back();
                if (history.size() % 3 == 0) { observe_value(pos); ++checks; }
            }
            observe_value(pos); ++checks; check(pos.fen() == rootfen, "lazy undo changed root");
            report();
            const State original = C.state;
            const auto original_hidden = std::to_array(C.hidden);
            // Flip ranks and exchange colours, including rights and EP. The
            // canonical tensors and the side-to-move head must be unchanged.
            std::istringstream fen_stream(rootfen);
            std::string board_part, turn, rights, ep_text, clock, fullmove;
            fen_stream >> board_part >> turn >> rights >> ep_text >> clock >> fullmove;
            std::string mirrored;
            const std::string names = " PNBRQK";
            for (int rank = 7; rank >= 0; --rank) {
                int empty = 0;
                for (int file = 0; file < 8; ++file) {
                    const Piece pc = original.board[(rank * 8 + file) ^ 56];
                    if (!pc) { ++empty; continue; }
                    if (empty) { mirrored += char('0' + empty); empty = 0; }
                    char c = names[int(type_of(pc))];
                    if (color_of(pc) == WHITE) c = char(std::tolower(static_cast<unsigned char>(c)));
                    mirrored += c;
                }
                if (empty) mirrored += char('0' + empty);
                if (rank) mirrored += '/';
            }
            for (char& c : rights) if (c != '-') {
                const auto u = static_cast<unsigned char>(c);
                c = char(std::isupper(u) ? std::tolower(u) : std::toupper(u));
            }
            if (ep_text != "-") ep_text[1] = char('1' + '8' - ep_text[1]);
            mirrored += " " + std::string(turn == "w" ? "b" : "w") + " " + rights + " "
                        + ep_text + " " + clock + " " + fullmove;
            Position mirror; StateInfo ms;
            mirror.set(mirrored, &ms, f.chess960); observe_value(mirror); ++checks;
            for (Color v : {WHITE, BLACK}) {
                for (int k = 0; k < P; ++k)
                    check(original.placement[v][k] == C.state.placement[~v][k], "mirror placement mismatch");
                for (int k = 0; k < R; ++k)
                    check(original.relations[v][k] == C.state.relations[~v][k], "mirror relation mismatch");
                for (int s = 0; s < 64; ++s) for (int k = 0; k < W; ++k) {
                    check(original.incoming[v][s][k] == C.state.incoming[~v][s^56][k], "mirror incoming mismatch");
                    check(original.outgoing[v][s][k] == C.state.outgoing[~v][s^56][k], "mirror outgoing mismatch");
                }
            }
            for (int j = 0; j < H; ++j) check(original_hidden[j] == C.hidden[j], "mirror head mismatch");
        }
        check(ep > 0 && castle >= 3 && promo >= 4 && nulls > 0, "missing special-move coverage");
        // Clock changes must rerun the head even with an unchanged board key.
        Position pos; StateInfo st;
        pos.set(StartFEN, &st); reset(pos); observe_value(pos);
        const auto first = C.stats.heads; ++st.rule50; observe_value(pos);
        check(C.stats.heads == first + 1, "clock failed to invalidate head");
        observe_value(pos); check(C.stats.heads == first + 1, "same input failed to reuse head");
        check(C.stats.head_reuses > 0, "head reuse not reached");
#if FORGE_NEXT_OPT >= 3
        // Returning from a child must recover the exact parent head. A null
        // move uses the same slot but a different board key and clock.
        const auto before_return = C.stats.head_reuses;
        const Move child_move = *MoveList<LEGAL>(pos).begin();
        StateInfo cs; pos.do_move(child_move, cs); observe_value(pos);
        pos.undo_move(child_move); observe_value(pos);
        check(C.stats.head_reuses > before_return, "parent head not recovered");
        StateInfo ns; pos.do_null_move(ns); observe_value(pos);
        pos.undo_null_move(); observe_value(pos);
        // Test production lazy state semantics, then compare to reconstruction.
        set_audit(false);
        pos.do_move(child_move, cs); observe_value(pos); pos.undo_move(child_move);
        observe_value(pos); sync(pos); compare(pos); set_audit(true);
#endif
        // Feature vocabulary bijection bounds and exact colour canonicalization.
        for (int a = 0; a < 64; ++a) for (int b = 0; b < 64; ++b) {
            const int dx = std::abs(a%8-b%8), dy = std::abs(a/8-b/8);
            if (a == b || !(dx == dy || !dx || !dy || dx*dy == 2)) continue;
            for (bool blocked : {false,true}) {
                const int i = edge_row(W_ROOK,a,B_KNIGHT,b,blocked,WHITE);
                const int j = edge_row(B_ROOK,a^56,W_KNIGHT,b^56,blocked,BLACK);
                check(i == j && i >= 0 && i < CONTACT_ROWS, "contact index/canonicalization mismatch");
                ++checks;
            }
        }
        std::cout << "forge2-selftest {\"passed\":true,\"checks\":" << checks
                  << ",\"ep\":" << ep << ",\"castles\":" << castle << ",\"promotions\":" << promo
                  << ",\"nulls\":" << nulls << "}" << std::endl;
        QuietPolicy = saved_policy; QuietPolicyWeight = saved_weight; QuietPolicyMinDepth = saved_min_depth;
        set_audit(saved_audit); set_mode(saved_mode); return true;
    } catch (const std::exception& e) {
        QuietPolicy = saved_policy; QuietPolicyWeight = saved_weight; QuietPolicyMinDepth = saved_min_depth;
        set_audit(saved_audit); set_mode(saved_mode);
        std::cerr << e.what() << std::endl; return false;
    }
}
}  // namespace boring::ForgeNext
