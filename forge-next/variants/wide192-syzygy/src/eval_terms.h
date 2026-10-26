// eval_terms.h — every hand-chosen number the evaluation uses, in one file.
//
// This is the tuning surface. Nothing here is fitted to data; the values come
// from chess understanding and are adjusted by playing. Each term carries a note
// saying what it is meant to express, so that changing one is a chess decision
// rather than a blind numeric poke.
//
// All values are S(middlegame, endgame) centipawn pairs, from the point of view
// of the side the term applies to. They are interpolated by game phase in
// eval.cpp, so a term that only matters in one stage should simply be zero in
// the other — that is how the evaluation knows a king belongs in the corner on
// move 20 and in the centre on move 60.

#ifndef BORING_EVAL_TERMS_H
#define BORING_EVAL_TERMS_H

#include "types.h"

namespace boring {
namespace Eval {

// =========================================================== material ======
//
// Pawns are worth more as the board empties, because promotion becomes real.
// Rooks gain the most: open lines and cutting off the king are endgame skills.
// Knights barely gain — they are poor at fighting a passer on the far wing —
// so a bishop overtakes a knight in the endgame before the pair bonus counts.

constexpr Score PAWN_VALUE   = S(  90,  120);
constexpr Score KNIGHT_VALUE = S( 345,  355);
constexpr Score BISHOP_VALUE = S( 365,  385);
constexpr Score ROOK_VALUE   = S( 500,  640);
constexpr Score QUEEN_VALUE  = S(1010, 1200);

constexpr Score PIECE_VALUE[PIECE_TYPE_NB] = {
    S(0, 0),  // NO_PIECE_TYPE
    PAWN_VALUE, KNIGHT_VALUE, BISHOP_VALUE, ROOK_VALUE, QUEEN_VALUE,
    S(0, 0),  // KING — never traded, so it has no value
    S(0, 0)
};

// One scalar per piece for move ordering, SEE, and non-pawn material counts,
// where a phase-dependent pair would be inconvenient.
constexpr Value SIMPLE_PIECE_VALUE[PIECE_TYPE_NB] = {0, 100, 350, 370, 550, 1100, 0, 0};

// Game phase runs 0 (bare kings) to 24 (all pieces present). Pawns do not count
// towards it: a queenless position with all sixteen pawns is an endgame.
constexpr int PHASE_VALUE[PIECE_TYPE_NB] = {0, 0, 1, 1, 2, 4, 0, 0};
constexpr int PHASE_MIDGAME              = 24;

// ==================================================== piece-square tables ===
//
// Written the way a board is drawn: first row is rank 8, first column file A.
// Read directly for Black and vertically mirrored for White; init_psqt() does
// the mirroring. These express placement only — anything depending on the rest
// of the board (open files, passers, shelter) belongs in eval.cpp.

// Pawns: reward reaching the far ranks and holding the centre; nudge the d and
// e pawns off their starting squares; do not reward the f/g/h pawns for
// wandering forward in the middlegame, since that is the king's roof.
constexpr Score PAWN_PSQT[SQUARE_NB] = {
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    S( 55, 95), S( 60, 95), S( 60, 95), S( 60, 95), S( 60, 95), S( 60, 95), S( 60, 95), S( 55, 95),
    S( 15, 45), S( 20, 45), S( 30, 40), S( 35, 38), S( 35, 38), S( 30, 40), S( 20, 45), S( 15, 45),
    S(  2, 20), S(  8, 20), S( 14, 15), S( 24, 12), S( 24, 12), S( 14, 15), S(  8, 20), S(  2, 20),
    S( -4,  8), S(  2,  8), S(  8,  4), S( 20,  0), S( 20,  0), S(  8,  4), S(  2,  8), S( -4,  8),
    S( -6,  4), S( -2,  4), S(  0,  0), S(  6, -4), S(  6, -4), S(  0,  0), S( -2,  4), S( -6,  4),
    S( -6,  2), S(  2,  2), S(  2,  0), S(-10, -2), S(-10, -2), S(  2,  0), S(  2,  2), S( -6,  2),
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0)
};

// Knights: the classic bowl. A knight on the rim is dim; one on an advanced
// central square is worth most of a bishop more than one in the corner.
constexpr Score KNIGHT_PSQT[SQUARE_NB] = {
    S(-70,-60), S(-45,-45), S(-30,-30), S(-25,-25), S(-25,-25), S(-30,-30), S(-45,-45), S(-70,-60),
    S(-40,-40), S(-20,-20), S(  0,-10), S(  5, -5), S(  5, -5), S(  0,-10), S(-20,-20), S(-40,-40),
    S(-25,-30), S(  5,-10), S( 20,  0), S( 28,  5), S( 28,  5), S( 20,  0), S(  5,-10), S(-25,-30),
    S(-20,-25), S( 10, -5), S( 28,  8), S( 35, 14), S( 35, 14), S( 28,  8), S( 10, -5), S(-20,-25),
    S(-20,-25), S(  8, -5), S( 26,  8), S( 33, 14), S( 33, 14), S( 26,  8), S(  8, -5), S(-20,-25),
    S(-25,-30), S(  2,-10), S( 20,  0), S( 26,  5), S( 26,  5), S( 20,  0), S(  2,-10), S(-25,-30),
    S(-40,-40), S(-20,-20), S(  2,-10), S(  8, -5), S(  8, -5), S(  2,-10), S(-20,-20), S(-40,-40),
    S(-60,-50), S(-35,-40), S(-25,-30), S(-20,-25), S(-20,-25), S(-25,-30), S(-35,-40), S(-60,-50)
};

// Bishops: gentler than the knight bowl, because a bishop on a1 still sees the
// long diagonal. The bumps on b2/g2 (and b7/g7) are the fianchetto.
constexpr Score BISHOP_PSQT[SQUARE_NB] = {
    S(-25,-25), S(-12,-12), S(-15,-12), S(-18,-10), S(-18,-10), S(-15,-12), S(-12,-12), S(-25,-25),
    S(-15,-12), S(  0, -4), S(  2,  0), S(  0,  2), S(  0,  2), S(  2,  0), S(  0, -4), S(-15,-12),
    S( -8, -8), S(  8,  0), S( 12,  4), S( 14,  6), S( 14,  6), S( 12,  4), S(  8,  0), S( -8, -8),
    S( -6, -6), S(  6,  2), S( 14,  6), S( 20, 10), S( 20, 10), S( 14,  6), S(  6,  2), S( -6, -6),
    S( -6, -6), S(  8,  2), S( 14,  6), S( 20, 10), S( 20, 10), S( 14,  6), S(  8,  2), S( -6, -6),
    S( -8, -8), S( 14,  0), S( 14,  4), S( 14,  6), S( 14,  6), S( 14,  4), S( 14,  0), S( -8, -8),
    S(-10,-10), S( 16, -4), S(  8,  0), S(  6,  2), S(  6,  2), S(  8,  0), S( 16, -4), S(-10,-10),
    S(-20,-20), S( -8,-12), S(-14,-12), S(-16,-10), S(-16,-10), S(-14,-12), S( -8,-12), S(-20,-20)
};

// Rooks: the seventh rank is the one placement worth real middlegame points.
// Otherwise prefer central files; open-file bonuses are computed elsewhere.
constexpr Score ROOK_PSQT[SQUARE_NB] = {
    S(  0,  4), S(  2,  4), S(  6,  4), S(  8,  4), S(  8,  4), S(  6,  4), S(  2,  4), S(  0,  4),
    S( 12, 10), S( 16, 10), S( 20, 10), S( 22, 10), S( 22, 10), S( 20, 10), S( 16, 10), S( 12, 10),
    S( -4,  2), S(  0,  2), S(  4,  2), S(  6,  2), S(  6,  2), S(  4,  2), S(  0,  2), S( -4,  2),
    S( -6,  2), S( -2,  2), S(  2,  2), S(  4,  2), S(  4,  2), S(  2,  2), S( -2,  2), S( -6,  2),
    S( -8,  2), S( -4,  2), S(  0,  2), S(  2,  2), S(  2,  2), S(  0,  2), S( -4,  2), S( -8,  2),
    S(-10,  0), S( -4,  0), S(  0,  0), S(  2,  0), S(  2,  0), S(  0,  0), S( -4,  0), S(-10,  0),
    S(-12, -2), S( -6, -2), S( -2, -2), S(  2, -2), S(  2, -2), S( -2, -2), S( -6, -2), S(-12, -2),
    S(-10, -2), S( -6, -2), S(  0, -2), S(  8, -2), S(  8, -2), S(  4, -2), S( -6, -2), S(-10, -2)
};

// Queens: almost flat in the middlegame — where she belongs depends on the
// position, not the square — but centralisation matters in the endgame. The
// back-rank dip discourages developing her on move three.
constexpr Score QUEEN_PSQT[SQUARE_NB] = {
    S( -8,-20), S( -4,-12), S( -4, -8), S( -2, -4), S( -2, -4), S( -4, -8), S( -4,-12), S( -8,-20),
    S( -6,-12), S( -2, -4), S(  0,  0), S(  0,  4), S(  0,  4), S(  0,  0), S( -2, -4), S( -6,-12),
    S( -4, -8), S(  0,  0), S(  2,  6), S(  2, 10), S(  2, 10), S(  2,  6), S(  0,  0), S( -4, -8),
    S( -2, -4), S(  0,  4), S(  2, 10), S(  4, 14), S(  4, 14), S(  2, 10), S(  0,  4), S( -2, -4),
    S( -2, -4), S(  0,  4), S(  2, 10), S(  4, 14), S(  4, 14), S(  2, 10), S(  0,  4), S( -2, -4),
    S( -4, -8), S(  2,  0), S(  2,  6), S(  2, 10), S(  2, 10), S(  2,  6), S(  2,  0), S( -4, -8),
    S( -6,-12), S( -2, -4), S(  2, -4), S(  2,  0), S(  2,  0), S(  2, -4), S( -2, -4), S( -6,-12),
    S(-10,-20), S( -6,-12), S( -4, -8), S( -2, -6), S( -2, -6), S( -4, -8), S( -6,-12), S(-10,-20)
};

// Kings: the one table where middlegame and endgame point in opposite
// directions, and the main reason a tapered evaluation is necessary at all.
constexpr Score KING_PSQT[SQUARE_NB] = {
    S(-60,-70), S(-50,-40), S(-50,-20), S(-50,-15), S(-50,-15), S(-50,-20), S(-50,-40), S(-60,-70),
    S(-50,-40), S(-40,-15), S(-40,  5), S(-40, 12), S(-40, 12), S(-40,  5), S(-40,-15), S(-50,-40),
    S(-40,-20), S(-30,  5), S(-30, 25), S(-30, 32), S(-30, 32), S(-30, 25), S(-30,  5), S(-40,-20),
    S(-35,-15), S(-25, 12), S(-25, 32), S(-25, 40), S(-25, 40), S(-25, 32), S(-25, 12), S(-35,-15),
    S(-25,-15), S(-20, 12), S(-25, 32), S(-30, 40), S(-30, 40), S(-25, 32), S(-20, 12), S(-25,-15),
    S(-10,-20), S( -5,  5), S(-15, 25), S(-25, 32), S(-25, 32), S(-15, 25), S( -5,  5), S(-10,-20),
    S( 15,-40), S( 18,-15), S( -5,  5), S(-20, 12), S(-20, 12), S( -8,  5), S( 18,-15), S( 15,-40),
    S( 15,-70), S( 35,-40), S( 28,-20), S(-15,-15), S( -5,-15), S(-10,-20), S( 32,-40), S( 18,-70)
};

constexpr const Score* PSQT_TABLE[PIECE_TYPE_NB] = {
    nullptr, PAWN_PSQT, KNIGHT_PSQT, BISHOP_PSQT, ROOK_PSQT, QUEEN_PSQT, KING_PSQT, nullptr
};

// Built by init_psqt(): material folded together with placement, negated for
// Black, indexed [piece][square]. Everything that tracks score incrementally
// uses this one table.
extern Score PSQ[PIECE_NB][SQUARE_NB];

void init_psqt();

// ========================================================= pawn structure ===
//
// Pawn terms are the skeleton of the position: unlike pieces, pawns cannot go
// back, so a structural weakness is permanent. That is why the endgame halves
// of these penalties are generally larger than the middlegame halves.

// A pawn with no friendly pawn on either adjacent file. It can never be
// defended by a pawn, so it is a permanent target on an open file.
constexpr Score ISOLATED = S(-6, -18);

// Isolated, and with no enemy pawn on its file to blunt the attack.
constexpr Score WEAK_UNOPPOSED = S(-6, -10);

// A pawn behind its neighbours that cannot advance without being taken by a
// pawn. Like an isolated pawn, but it also blocks its own file.
constexpr Score BACKWARD = S(-10, -14);

// Two pawns on the same file. They cannot defend each other and one of them is
// nearly worthless — crippling in the endgame, tolerable in a sharp middlegame.
constexpr Score DOUBLED = S(-10, -50);

// Connected pawns, scaled by how far up the board they are. A phalanx on the
// sixth is a battering ram; on the second it is just tidy. Indexed by rank.
constexpr Score CONNECTED[RANK_NB] = {
    S(0, 0), S(4, -2), S(6, 0), S(10, 3), S(22, 12), S(38, 30), S(64, 62), S(0, 0)
};

// A pawn whose advance is permanently stopped by an enemy pawn, on the fifth or
// sixth rank. Indexed by rank; only ranks 5 and 6 are non-zero.
constexpr Score BLOCKED_PAWN[RANK_NB] = {
    S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(-6, -4), S(-3, 2), S(0, 0), S(0, 0)
};

// Each additional pawn island beyond the first: more weaknesses to defend.
constexpr Score PAWN_ISLAND = S(-4, -8);

// ---- passed pawns ----
//
// The single most important structural feature in an endgame, and the reason
// the endgame halves here dwarf the middlegame ones.

// Base bonus by the rank the passer stands on, from its own point of view.
constexpr Score PASSED_RANK[RANK_NB] = {
    S(0, 0), S(3, 12), S(8, 20), S(14, 40), S(38, 70), S(88, 130), S(150, 220), S(0, 0)
};

// A passer on a rook file is worth less: the defending king covers the
// promotion square from one side only, and there is less room to escort it.
constexpr Score PASSED_FILE = S(-4, -6);

// A passer whose stop square is controlled by us and not by the enemy is much
// closer to actually queening than one that is merely "passed".
constexpr Score PASSED_FREE_ADVANCE = S(8, 22);

// A passer supported from behind by a rook is the classic Tarrasch rule.
constexpr Score ROOK_BEHIND_PASSER  = S(6, 22);
constexpr Score ROOK_BLOCKS_PASSER  = S(-2, -12);

// King proximity to a passer's promotion path, per square of distance. Only an
// endgame consideration, and it is why kings march in pawn endings.
constexpr Score PASSER_KING_DISTANCE_OURS   = S(0, -4);
constexpr Score PASSER_KING_DISTANCE_THEIRS = S(0,  8);

// =============================================================== mobility ===
//
// The count of safe squares a piece attacks, excluding squares held by our own
// blocked pawns and squares attacked by enemy pawns — a knight "attacking" a
// square a pawn guards is not mobile there in any useful sense.
//
// Each table is indexed by that count. The curves are deliberately steep at the
// bottom: the difference between a piece with two squares and one with six is
// far larger than between one with twelve and one with sixteen.

constexpr Score KNIGHT_MOBILITY[9] = {
    S(-50, -55), S(-30, -40), S(-10, -18), S(-2, -4), S(6, 6), S(12, 14), S(18, 20), S(23, 23),
    S(26, 25)
};

constexpr Score BISHOP_MOBILITY[14] = {
    S(-42, -55), S(-22, -32), S(-2, -14), S(4, 0), S(12, 10), S(20, 20), S(26, 28), S(30, 34),
    S(33, 40), S(36, 44), S(38, 47), S(40, 50), S(42, 52), S(44, 54)
};

constexpr Score ROOK_MOBILITY[15] = {
    S(-44, -60), S(-24, -22), S(-8, 10), S(-4, 30), S(-2, 46), S(0, 58), S(4, 70), S(8, 78),
    S(12, 86), S(15, 92), S(18, 96), S(20, 100), S(23, 103), S(25, 105), S(27, 107)
};

constexpr Score QUEEN_MOBILITY[28] = {
    S(-30, -46), S(-18, -30), S(-6, -12), S(-2, 0), S(4, 12), S(8, 24), S(11, 34), S(14, 42),
    S(17, 50), S(20, 56), S(22, 62), S(24, 66), S(26, 70), S(28, 74), S(29, 77), S(31, 80),
    S(32, 83), S(33, 85), S(35, 88), S(36, 90), S(37, 92), S(38, 94), S(39, 96), S(40, 97),
    S(41, 98), S(42, 99), S(43, 100), S(44, 101)
};

// ========================================================== piece bonuses ===

// Two bishops cover both colour complexes. Worth more as the board opens up.
constexpr Score BISHOP_PAIR = S(28, 58);

// A minor piece on a square no enemy pawn can ever attack, defended by one of
// our own pawns. The classic hole, and the reason a knight on d5 wins games.
constexpr Score OUTPOST_KNIGHT = S(32, 20);
constexpr Score OUTPOST_BISHOP = S(20, 8);

// An outpost square we could reach but do not yet occupy.
constexpr Score REACHABLE_OUTPOST = S(12, 8);

// A minor sheltering directly behind one of our pawns.
constexpr Score MINOR_BEHIND_PAWN = S(8, 2);

// Per own pawn on the bishop's own colour complex, worse when those pawns are
// blocked and the bishop cannot get out from behind them. This is a "bad
// bishop", and it is why the pair bonus is not the whole story.
constexpr Score BISHOP_PAWNS = S(-3, -6);

// A bishop on a long diagonal that is not blocked in the centre.
constexpr Score LONG_DIAGONAL_BISHOP = S(22, 0);

// The a7/h7 (and a2/h2) trap: a bishop that walked into a pawn cage.
constexpr Score TRAPPED_BISHOP = S(-50, -50);

// Rook on a file with no pawns at all, or none of ours. Both are the reason to
// trade pawns on a file in the first place.
constexpr Score ROOK_ON_OPEN_FILE      = S(30, 12);
constexpr Score ROOK_ON_SEMI_OPEN_FILE = S(14, 6);

// A rook sharing a file with the enemy queen, even through pieces.
constexpr Score ROOK_ON_QUEEN_FILE = S(6, 2);

// A rook with nowhere to go, boxed in by its own uncastled king. Worse when the
// king has lost the right to castle out of the way.
constexpr Score TRAPPED_ROOK = S(-28, -2);

// A queen that can be hit by a discovered attack if a piece moves.
constexpr Score WEAK_QUEEN = S(-40, -12);

// Minor pieces far from their own king are not defending it.
constexpr Score KING_PROTECTOR_KNIGHT = S(-6, -4);
constexpr Score KING_PROTECTOR_BISHOP = S(-4, -2);

// ================================================================ threats ===
//
// What is attacked and not adequately defended. These terms are what let a
// hand-crafted evaluation see one move of tactics without searching it.

// A piece of this type attacked by one of our minors. Indexed by victim type.
constexpr Score THREAT_BY_MINOR[PIECE_TYPE_NB] = {
    S(0, 0), S(6, 32), S(60, 42), S(58, 44), S(78, 60), S(60, 116), S(0, 0), S(0, 0)
};

// Attacked by one of our rooks.
constexpr Score THREAT_BY_ROOK[PIECE_TYPE_NB] = {
    S(0, 0), S(4, 44), S(38, 72), S(40, 60), S(0, 40), S(50, 38), S(0, 0), S(0, 0)
};

// Attacked by our king — only relevant when the piece is otherwise undefended.
constexpr Score THREAT_BY_KING = S(24, 90);

// An enemy piece with no defender at all.
constexpr Score HANGING = S(70, 36);

// A non-pawn defended only by the queen: the defence collapses if she is hit.
constexpr Score WEAK_QUEEN_PROTECTION = S(14, 0);

// Attacked by a pawn that is itself safe. The cheapest possible attacker.
constexpr Score THREAT_BY_SAFE_PAWN = S(170, 96);

// A pawn that can advance one square, safely, to attack a piece.
constexpr Score THREAT_BY_PAWN_PUSH = S(48, 40);

// A square the enemy attacks that we attack too, but where they have no more
// defenders — a square they cannot actually use.
constexpr Score RESTRICTED_PIECE = S(6, 7);

// Our slider or knight lining up on the enemy queen.
constexpr Score SLIDER_ON_QUEEN = S(50, 24);
constexpr Score KNIGHT_ON_QUEEN = S(18, 12);

// ========================================================== king safety =====
//
// The single largest and least linear part of the evaluation. Danger is
// accumulated as "attack units" from many sources, then fed through a quadratic
// so that two attackers are worth far more than twice one attacker — which is
// exactly how attacks work over the board.

// Per attacking piece type, weighted by how dangerous that piece is near a king.
constexpr int KING_ATTACK_WEIGHT[PIECE_TYPE_NB] = {0, 0, 81, 52, 44, 10, 0, 0};

// A check the checking piece can deliver without simply being captured. These
// dominate the danger sum, and rightly so: a safe check is the start of a mate.
constexpr int SAFE_CHECK_QUEEN  = 780;
constexpr int SAFE_CHECK_ROOK   = 1080;
constexpr int SAFE_CHECK_BISHOP = 635;
constexpr int SAFE_CHECK_KNIGHT = 790;

// Additive danger contributions.
constexpr int KING_DANGER_ATTACK_SQUARES = 148;  // per attacked square in the ring
constexpr int KING_DANGER_WEAK_RING      = 185;  // per weak square in the ring
constexpr int KING_DANGER_UNSAFE_CHECK   = 118;  // per check that would be captured
constexpr int KING_DANGER_QUEENLESS      = -873; // no enemy queen: attack rarely lands
constexpr int KING_DANGER_KNIGHT_DEFENDER = -35;
constexpr int KING_DANGER_BASE           = 32;

// Pawn shelter in front of the king, by the distance of our nearest pawn on
// that file from the king's rank. Index 0 means there is no pawn at all on the
// file, which is the worst case: an open file at the king.
constexpr Score SHELTER_STRENGTH[4][RANK_NB] = {
    // edge file
    {S(-6, 0), S(84, 0), S(90, 0), S(48, 0), S(24, 0), S(12, 0), S(20, 0), S(0, 0)},
    // knight file
    {S(-56, 0), S(60, 0), S(42, 0), S(-14, 0), S(-30, 0), S(-58, 0), S(-92, 0), S(0, 0)},
    // bishop file
    {S(-16, 0), S(80, 0), S(24, 0), S(-4, 0), S(4, 0), S(-16, 0), S(-32, 0), S(0, 0)},
    // central file
    {S(-40, 0), S(20, 0), S(2, 0), S(-24, 0), S(-42, 0), S(-64, 0), S(-100, 0), S(0, 0)}
};

// Enemy pawns storming towards our king, by how close they have come. A storm
// pawn that is blocked by one of ours is much less dangerous, which is the
// UNBLOCKED/BLOCKED distinction.
constexpr Score STORM_UNBLOCKED[4][RANK_NB] = {
    {S(90, 0), S(-290, 0), S(-56, 0), S(58, 0), S(30, 0), S(14, 0), S(20, 0), S(0, 0)},
    {S(44, 0), S(-18, 0), S(122, 0), S(34, 0), S(22, 0), S(-8, 0), S(12, 0), S(0, 0)},
    {S(-8, 0), S(38, 0), S(140, 0), S(30, 0), S(6, 0), S(-8, 0), S(-4, 0), S(0, 0)},
    {S(-14, 0), S(-14, 0), S(78, 0), S(18, 0), S(12, 0), S(-4, 0), S(-8, 0), S(0, 0)}
};

constexpr Score STORM_BLOCKED[RANK_NB] = {
    S(0, 0), S(0, 0), S(38, 0), S(-6, 0), S(-6, 0), S(-4, 0), S(-2, 0), S(0, 0)
};

// The king standing on a file with no pawn of ours at all.
constexpr Score KING_ON_OPEN_FILE      = S(-24, 6);
constexpr Score KING_ON_SEMI_OPEN_FILE = S(-12, 4);

// Enemy attacks on the flank the king lives on, weighted double for squares
// they attack twice.
constexpr int FLANK_ATTACK_WEIGHT = 8;

// ================================================================= space ====
//
// Safe squares behind our own pawns on the four central files. A crude proxy
// for "room to manoeuvre", and only meaningful while there are pieces to
// manoeuvre — hence the material threshold.

constexpr Value SPACE_THRESHOLD = 3500;

// ============================================================== scaling =====
//
// A tapered score is still an absolute claim; scale factors say "this endgame
// is harder to win than the material suggests". 64 means no scaling at all.

constexpr int SCALE_NORMAL       = 64;
constexpr int SCALE_DRAW         = 0;
constexpr int SCALE_OPPOSITE_BISHOPS = 22;  // notoriously drawish
constexpr int SCALE_ONE_PAWN     = 34;      // a lone extra pawn often is not enough

// ================================================================= tempo ====
//
// Having the move is worth something in a sharp middlegame and almost nothing
// in a locked endgame.
constexpr Value TEMPO = 20;

}  // namespace Eval
}  // namespace boring

#endif  // BORING_EVAL_TERMS_H
