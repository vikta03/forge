// uci.h — the UCI protocol, and the move/square string conversions everything
// else uses for output.

#ifndef BORING_UCI_H
#define BORING_UCI_H

#include <string>

#include "types.h"

namespace boring {

constexpr const char* ENGINE_VERSION  = "0.2";
constexpr int         DEFAULT_HASH_MB = 64;

class Position;

namespace UCI {

// Read commands from stdin until "quit". Runs on the main thread; searches
// run on a worker so that "stop" is answerable.
void loop();

// True when the GUI has told us we are playing Chess960. It only changes how
// castling is written: "e1h1" (king takes rook) instead of "e1g1".
extern bool Chess960;

// Long algebraic notation, as UCI requires: "e2e4", "e7e8q", and for castling
// either "e1g1" or "e1h1" depending on the notation in force.
std::string move(Move m);
std::string square(Square s);

// Parse a UCI move string against a position. Returns MOVE_NONE if the string
// does not name a legal move here.
Move to_move(const Position& pos, const std::string& str);

// Format a search score the way GUIs expect: "cp N" or "mate N".
std::string value(Value v);

}  // namespace UCI
}  // namespace boring

#endif  // BORING_UCI_H
