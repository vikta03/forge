// FORGE 2 isolated learning pilot. GPL-3.0-or-later, like the host engine.
#pragma once
#include "types.h"
#include <string>

namespace boring {
class Position;
struct ExtMove;
namespace ForgeNext {
bool loaded();
const std::string& model_path();
void set_telemetry(bool enabled);
bool telemetry();
extern bool QuietPolicy;
extern int QuietPolicyWeight;
extern int QuietPolicyMinDepth;
void order_quiets(const Position& pos, int depth, ExtMove* begin, ExtMove* end);
bool load_model(const std::string& path, std::string& error);
void unload();
Value evaluate(const Position& pos);
void inspect(const Position& pos);
void cost(const Position& pos, unsigned iterations);       // exact integer outputs, raw 960 UCI
void set_mode(int mode);                 // 0 off, 1 placement, 2 contacts, 3 policy
void set_audit(bool enabled);            // expensive independent reconstruction
void reset(const Position& pos);
void mark_move(const Position& pos, Move move);  // before make, after undo
void observe_value(const Position& pos);
void observe_quiets(const Position& pos, int depth, const ExtMove* begin,
                    const ExtMove* end);
void report();
bool self_test();
}  // namespace ForgeNext
}  // namespace boring
