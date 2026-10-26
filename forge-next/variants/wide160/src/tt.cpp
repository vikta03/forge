// tt.cpp — transposition table storage and replacement.

#include "tt.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

#if defined(__GNUC__)
    #include <xmmintrin.h>
#endif

namespace boring {

TranspositionTable TT;

namespace {

// The high 64 bits of a 64x64 multiply. Multiplying the key by the cluster
// count maps the key uniformly onto [0, clusterCount) using the *high* bits,
// which are the ones a Zobrist key randomises best.
inline uint64_t mul_hi64(uint64_t a, uint64_t b) {
    return uint64_t((__uint128_t(a) * __uint128_t(b)) >> 64);
}

}  // namespace

// Store a result, but not unconditionally: an entry that is deeper and still
// current is more valuable than a shallow new one.
bool TTEntry::save(Key k, Value v, bool pv, Bound b, int d, Move m, Value ev,
                   uint8_t generation) {
    // Keep an existing move if the new search produced none for this position.
    // A move from a previous iteration is still a good ordering hint.
    if (m != MOVE_NONE || uint16_t(k) != key16)
        move16 = uint16_t(m);

    // Overwrite when this is a different position, when the result is exact, or
    // when the new search is meaningfully deeper than what is stored.
    if (b == BOUND_EXACT || uint16_t(k) != key16 || d - DEPTH_ENTRY_OFFSET + 2 * pv > depth8 - 4) {
        assert(d > DEPTH_ENTRY_OFFSET);
        assert(d < 256 + DEPTH_ENTRY_OFFSET);

        key16     = uint16_t(k);
        depth8    = uint8_t(d - DEPTH_ENTRY_OFFSET);
        genBound8 = uint8_t(generation | (uint8_t(pv) << 2) | b);
        value16   = int16_t(v);
        eval16    = int16_t(ev);
        return true;
    }
    return false;
}

TranspositionTable::~TranspositionTable() { std::free(table); }

void TranspositionTable::resize(size_t mbSize) {
    // Three clock bytes accompany each unchanged 32-byte cluster. Round down
    // to pairs so aligned_alloc's size remains a multiple of 64. Both arrays
    // fit the requested Hash budget (allocator bookkeeping is not included).
    const auto allocate = [&](size_t megabytes) {
        const size_t count = std::max<size_t>(
            2, ((megabytes * 1024 * 1024) / (sizeof(Cluster) + CLUSTER_SIZE)) & ~size_t(1));
        auto* replacement = static_cast<Cluster*>(std::aligned_alloc(64, count * sizeof(Cluster)));
        if (!replacement)
            return false;
        std::unique_ptr<uint8_t[]> tags(new (std::nothrow) uint8_t[count * CLUSTER_SIZE]);
        if (!tags) {
            std::free(replacement);
            return false;
        }
        std::free(table);
        table = replacement;
        clusterCount = count;
        evalRule50Tags = std::move(tags);
        clear();
        return true;
    };
    if (!allocate(std::max<size_t>(1, mbSize))
        && !allocate(std::max<size_t>(1, std::min<size_t>(mbSize, 16))))
        throw std::bad_alloc();
}

void TranspositionTable::clear() {
    if (table)
        std::memset(table, 0, clusterCount * sizeof(Cluster));
    if (evalRule50Tags)
        std::fill_n(evalRule50Tags.get(), clusterCount * CLUSTER_SIZE, uint8_t(255));
    generation8 = 0;
}

size_t TranspositionTable::tag_index(const TTEntry* entry) const {
    const auto offset = reinterpret_cast<const char*>(entry) - reinterpret_cast<const char*>(table);
    return size_t(offset) / sizeof(Cluster) * CLUSTER_SIZE
           + size_t(offset) % sizeof(Cluster) / sizeof(TTEntry);
}

uint8_t TranspositionTable::eval_rule50(const TTEntry* entry) const {
    return evalRule50Tags[tag_index(entry)];
}

void TranspositionTable::tag_eval(TTEntry* entry, int rule50) {
    evalRule50Tags[tag_index(entry)] = entry->eval() == VALUE_NONE || rule50 < 0 || rule50 >= 255
        ? uint8_t(255) : uint8_t(rule50);
}

void TranspositionTable::store_eval(TTEntry* entry, Value eval, int rule50) {
    entry->eval16 = int16_t(eval);
    tag_eval(entry, rule50);
}

bool TranspositionTable::save(TTEntry* entry, Key k, Value v, bool pv, Bound b,
                              int d, Move m, Value ev, uint8_t generation, int rule50) {
    if (!entry->save(k, v, pv, b, d, m, ev, generation))
        return false;
    tag_eval(entry, rule50);
    return true;
}

TranspositionTable::Cluster* TranspositionTable::cluster_for(Key key) const {
    return &table[mul_hi64(key, clusterCount)];
}

void TranspositionTable::prefetch(Key key) const {
#if defined(__GNUC__)
    __builtin_prefetch(cluster_for(key));
#else
    (void) key;
#endif
}

TTEntry* TranspositionTable::probe(Key key, bool& found) const {
    TTEntry*       tte   = cluster_for(key)->entry;
    const uint16_t key16 = uint16_t(key);

    for (int i = 0; i < CLUSTER_SIZE; ++i)
        if (tte[i].key16 == key16 || !tte[i].depth8) {
            // Refresh the generation so a hit stops looking like a stale entry.
            tte[i].genBound8 = uint8_t(generation8 | (tte[i].genBound8 & (GENERATION_DELTA - 1)));

            found = bool(tte[i].depth8);
            return &tte[i];
        }

    // Nothing matched, so pick a victim. Depth is the primary criterion, but an
    // entry from an older search is discounted by how old it is, so that deep
    // results from a position the game has left behind eventually give way.
    TTEntry* replace = tte;

    for (int i = 1; i < CLUSTER_SIZE; ++i)
        if (replace->depth8 - ((GENERATION_CYCLE + generation8 - replace->genBound8) & GENERATION_MASK)
            > tte[i].depth8 - ((GENERATION_CYCLE + generation8 - tte[i].genBound8) & GENERATION_MASK))
            replace = &tte[i];

    found = false;
    return replace;
}

int TranspositionTable::hashfull() const {
    // Sample the first thousand clusters rather than walking the whole table.
    int cnt = 0;

    // Any occupied entry counts, whatever search wrote it. Restricting this
    // to the current generation would restart the figure at zero every move
    // and read like a table wipe, when entries from earlier searches are
    // still present and still probed -- the generation only orders
    // replacement victims.
    for (int i = 0; i < 1000; ++i)
        for (int j = 0; j < CLUSTER_SIZE; ++j)
            cnt += table[i].entry[j].depth8 != 0;

    return cnt / CLUSTER_SIZE;
}

}  // namespace boring
