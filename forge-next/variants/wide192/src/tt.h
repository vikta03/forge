// tt.h — the transposition table.
//
// Layout: 10-byte entries, three to a 32-byte cluster, so a probe touches one
// cluster and never straddles a cache line. Indexing is a single 64x64->128
// multiply rather than a modulo, which costs nothing and lets the table be any
// size in megabytes instead of only powers of two.
//
// Mate scores are NOT stored as-is. A mate score is relative to the root, but a
// position reached at a different distance from the root needs it relative to
// *that* ply. Converting on the way in and out is the caller's job — see
// value_to_tt() and value_from_tt() in search.cpp — and getting it wrong makes
// every reported mate distance unstable.

#ifndef BORING_TT_H
#define BORING_TT_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include "types.h"

namespace boring {

enum Bound : uint8_t {
    BOUND_NONE  = 0,
    BOUND_UPPER = 1,  // failed low: the true value is at most this
    BOUND_LOWER = 2,  // failed high: the true value is at least this
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

// Quiescence nodes are searched at depth 0 and below. Storing depth as an
// unsigned byte therefore needs an offset so that the negative depths survive.
constexpr int DEPTH_ENTRY_OFFSET = -7;
constexpr int DEPTH_QS           = 0;
constexpr int DEPTH_NONE         = -6;

class TranspositionTable;

struct TTEntry {
    // Only the low 16 bits of the key are kept. A wrong-key collision is
    // therefore possible; the search guards against acting on a bogus move by
    // validating it with Position::pseudo_legal().
    uint16_t key16;
    uint16_t move16;
    int16_t  value16;
    int16_t  eval16;
    uint8_t  depth8;
    uint8_t  genBound8;

    Move  move() const { return Move(move16); }
    Value value() const { return Value(value16); }
    Value eval() const { return Value(eval16); }
    int   depth() const { return int(depth8) + DEPTH_ENTRY_OFFSET; }
    bool  is_pv() const { return bool(genBound8 & 0x4); }
    Bound bound() const { return Bound(genBound8 & 0x3); }

    // True only when the evaluation-bearing payload was actually replaced.
    bool save(Key k, Value v, bool pv, Bound b, int d, Move m, Value ev, uint8_t generation);

    friend class TranspositionTable;
};

class TranspositionTable {
    // Three entries share a cluster; the two spare bytes keep the cluster at a
    // clean 32 so that clusters never span cache lines.
    static constexpr int CLUSTER_SIZE = 3;

    struct Cluster {
        TTEntry entry[CLUSTER_SIZE];
        char    padding[2];
    };

    static_assert(sizeof(Cluster) == 32, "cluster must be exactly 32 bytes");

   public:
    ~TranspositionTable();

    void resize(size_t mbSize);
    void clear();

    // Bump the age so that entries from previous searches become preferred
    // replacement victims without having to wipe the table.
    void new_search() { generation8 += GENERATION_DELTA; }
    uint8_t generation() const { return generation8; }

    // Returns the matching entry if there is one (found = true), otherwise the
    // entry that should be overwritten (found = false).
    TTEntry* probe(Key key, bool& found) const;

    // The board-key policy for search bounds is unchanged. Static evaluations
    // additionally depend on the halfmove clock, independently of that policy.
    uint8_t eval_rule50(const TTEntry* entry) const;
    void store_eval(TTEntry* entry, Value eval, int rule50);
    bool save(TTEntry* entry, Key k, Value v, bool pv, Bound b, int d, Move m,
              Value ev, uint8_t generation, int rule50);
    size_t storage_bytes() const { return clusterCount * (sizeof(Cluster) + CLUSTER_SIZE); }

    // Permille of the table in use, for `info hashfull`.
    int hashfull() const;

    void prefetch(Key key) const;

   private:
    // The generation is stored in the top 5 bits of genBound8; the bottom 3
    // hold the bound and the PV flag.
    static constexpr unsigned GENERATION_BITS  = 3;
    static constexpr int      GENERATION_DELTA = 1 << GENERATION_BITS;
    static constexpr int      GENERATION_CYCLE = 255 + GENERATION_DELTA;
    static constexpr int      GENERATION_MASK  = (0xFF << GENERATION_BITS) & 0xFF;

    Cluster* cluster_for(Key key) const;

    size_t   clusterCount = 0;
    Cluster* table        = nullptr;
    std::unique_ptr<uint8_t[]> evalRule50Tags;
    uint8_t  generation8  = 0;

    size_t tag_index(const TTEntry* entry) const;
    void tag_eval(TTEntry* entry, int rule50);
    friend struct TTEntry;
};

extern TranspositionTable TT;

}  // namespace boring

#endif  // BORING_TT_H
