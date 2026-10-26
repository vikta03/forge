// endgame.h — specialised knowledge for endings a general evaluation gets wrong.
//
// A tapered material-and-placement score has no idea that king and two knights
// cannot mate, that king, bishop and knight must drive the enemy king to a
// corner of a *particular* colour, or whether a given king-and-pawn race is won
// or drawn. Those are facts, not heuristics, and they belong in a table.
//
// The KPK bitbase is generated at start-up by retrograde analysis rather than
// loaded from a file, so the engine stays entirely self-contained.

#ifndef BORING_ENDGAME_H
#define BORING_ENDGAME_H

#include "types.h"

namespace boring {

class Position;

// A score that means "this is winning, and the exact number does not matter" —
// far above any plausible evaluation but well below a mate score, so it never
// gets mistaken for one.
constexpr Value VALUE_KNOWN_WIN = 10000;

namespace Endgames {

// When false, probe() reports nothing and the learned evaluator handles even
// the endings this file knows exactly. An experiment control: the shipped
// default keeps the table.
extern bool Enabled;

// Load Syzygy tablebases from a directory. Returns the largest piece
// count available, or 0 when none could be opened; an empty path
// unloads them. The built-in knowledge below stays in place either way
// and answers whatever the tables do not.
int init_syzygy(const char* path);

// Build the KPK bitbase. Must be called once before any evaluation.
void init();

// If this exact material configuration is one we know outright, write the score
// (from the side to move's point of view) into `value` and return true.
bool probe(const Position& pos, Value& value);

// True when neither side has enough material to force mate, whatever the
// position: bare kings, a lone minor, or two knights against a bare king.
bool insufficient_material(const Position& pos);

}  // namespace Endgames
}  // namespace boring

#endif  // BORING_ENDGAME_H
