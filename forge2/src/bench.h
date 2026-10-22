// bench.h — the self-contained correctness and speed harnesses.
//
// Every position and every expected node count is compiled into the binary.
// Nothing here reads a file, so the engine can be verified anywhere it runs.

#ifndef BORING_BENCH_H
#define BORING_BENCH_H

#include <cstdint>

#include "types.h"

namespace boring {

class Position;

// Count leaf nodes at `depth`. With Root == true, also prints the per-move
// breakdown ("divide"), which is how a perft mismatch gets bisected.
template<bool Root>
uint64_t perft(Position& pos, int depth);

// Run the embedded perft suite. Returns true if every position matched.
bool perft_suite();

// Run every self-check: perft, static exchange evaluation, FEN round-trip,
// make/unmake restoration, and evaluation colour symmetry. Returns true if all
// of them passed. This is the whole correctness net — there is no external
// test data anywhere in the project.
bool run_tests();

// Search a fixed set of positions to a fixed depth and report total nodes and
// speed. The node count is a deterministic signature of the search: if it moves
// when it should not have, something changed that was not meant to.
void bench();

}  // namespace boring

#endif  // BORING_BENCH_H
