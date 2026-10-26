// FORGE Wide192 local-tablebase integration. GPL-3.0; Fathom notices retained.
#pragma once
#include <string>
#include <vector>
#include "position.h"
namespace boring::Syzygy {
extern std::string Path;
extern bool RootProbe;
extern int ProbeLimit;
struct Stats {
    uint64_t rootAttempts=0, rootHits=0, attempts=0, mainHits=0, qHits=0, failed=0;
    std::string firstFen;
    int firstWdl=0;
};
extern Stats stats;
void configure(const std::string& path);
bool eligible(const Position& pos);
void reset();
uint64_t hits();
void report();
bool interior(const Position& pos, int ply, bool quiescence, Value& value);
struct RootResult { Move move=MOVE_NONE; Value value=VALUE_NONE; int outcome=0, rank=0; };
bool root(Position& pos, const std::vector<Move>& allowed, RootResult& result);
}
