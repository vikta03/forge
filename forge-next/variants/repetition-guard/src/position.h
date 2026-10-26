// position.h — the board, and everything that can be asked of it.
//
// Undo information lives in a chain of StateInfo records owned by the caller,
// not inside Position. do_move() links a fresh StateInfo in front; undo_move()
// unlinks it. Only irreversible state is stored, so a ply costs a few dozen
// bytes rather than a full board snapshot.

#ifndef BORING_POSITION_H
#define BORING_POSITION_H

#include <string>

#include "bitboard.h"
#include "types.h"

namespace boring {

namespace Zobrist {
extern Key PSQ[PIECE_NB][SQUARE_NB];
extern Key EN_PASSANT[FILE_NB];
extern Key CASTLING[CASTLING_RIGHTS_NB];
extern Key SIDE;
}  // namespace Zobrist

extern const std::string StartFEN;

// Which right a castle by `us` towards the named wing corresponds to.
constexpr CastlingRights castling_right(Color us, bool kingSide) {
    return CastlingRights((us == WHITE ? WHITE_OO : BLACK_OO) << (kingSide ? 0 : 1));
}

// Where the king and rook end up. These squares are the same in Chess960 as in
// standard chess — only the starting squares differ, and that is the whole
// trick that makes 960 castling tractable: the destination never varies, so
// only the path and the origins need per-position bookkeeping.
constexpr Square castling_king_to(Color us, bool kingSide) {
    return relative_square(us, kingSide ? SQ_G1 : SQ_C1);
}
constexpr Square castling_rook_to(Color us, bool kingSide) {
    return relative_square(us, kingSide ? SQ_F1 : SQ_D1);
}

// Irreversible position state. The first block is carried forward from the
// previous ply and then adjusted; the second block is rebuilt from scratch by
// every do_move().
struct StateInfo {
    // --- carried forward and adjusted ---
    Key   pawnKey;
    Key   minorPieceKey;
    Key   nonPawnKey[COLOR_NB];
    Key   materialKey;
    Value nonPawnMaterial[COLOR_NB];
    int   castlingRights;
    int   rule50;         // halfmove clock
    int   pliesFromNull;  // plies since the last null move or irreversible move
    Square epSquare;
    Score psq;    // incremental material + piece-square score, White's point of view
    int   phase;  // incremental game phase, 0 (bare kings) .. 24 (full board)

    // --- rebuilt every move ---
    Key      key;
    // Rules identity omits an EP marker when every EP capture is illegal.
    // Keep key/epSquare unchanged for the trained input and evaluation caches.
    Key      repetitionKey;
    Bitboard checkersBB;
    Bitboard blockersForKing[COLOR_NB];  // pieces that would expose their king if moved
    Bitboard pinners[COLOR_NB];          // enemy sliders doing the pinning
    Bitboard checkSquares[PIECE_TYPE_NB];
    Piece    capturedPiece;
    int      repetition;
    StateInfo* previous;
};

class Position {
   public:
    static void init();

    Position()                           = default;
    Position(const Position&)            = delete;
    Position& operator=(const Position&) = delete;

    // --- setup ---
    Position&   set(const std::string& fen, StateInfo* si, bool isChess960 = false);
    std::string fen() const;

    // --- board contents ---
    Piece piece_on(Square s) const { return board[s]; }
    bool  empty(Square s) const { return board[s] == NO_PIECE; }

    Bitboard pieces(PieceType pt = ALL_PIECES) const { return byTypeBB[pt]; }
    template<typename... PieceTypes>
    Bitboard pieces(PieceType pt, PieceTypes... pts) const {
        return pieces(pt) | pieces(pts...);
    }
    Bitboard pieces(Color c) const { return byColorBB[c]; }
    template<typename... PieceTypes>
    Bitboard pieces(Color c, PieceTypes... pts) const {
        return pieces(c) & pieces(pts...);
    }

    template<PieceType Pt>
    int count(Color c) const {
        return pieceCount[make_piece(c, Pt)];
    }
    template<PieceType Pt>
    int count() const {
        return count<Pt>(WHITE) + count<Pt>(BLACK);
    }
    int count(Color c) const { return popcount(pieces(c)); }

    Square king_square(Color c) const { return lsb(pieces(c, KING)); }

    // --- game state ---
    Color side_to_move() const { return sideToMove; }
    int   game_ply() const { return gamePly; }
    int   rule50_count() const { return st->rule50; }
    Square ep_square() const { return st->epSquare; }
    Key   key() const { return st->key; }
    Key   repetition_key() const { return st->repetitionKey; }
    Key   key_after(Move m) const;
    Key   pawn_key() const { return st->pawnKey; }
    Key   minor_piece_key() const { return st->minorPieceKey; }
    Key   non_pawn_key(Color c) const { return st->nonPawnKey[c]; }
    Key   material_key() const { return st->materialKey; }
    Score psq_score() const { return st->psq; }
    int   phase() const { return st->phase; }
    Value non_pawn_material(Color c) const { return st->nonPawnMaterial[c]; }
    Value non_pawn_material() const {
        return st->nonPawnMaterial[WHITE] + st->nonPawnMaterial[BLACK];
    }
    uint64_t nodes_searched() const { return nodes; }
    void     set_nodes_searched(uint64_t n) { nodes = n; }

    int  castling_rights(Color c) const { return st->castlingRights & (c == WHITE ? WHITE_CASTLING : BLACK_CASTLING); }
    bool can_castle(CastlingRights cr) const { return st->castlingRights & cr; }
    // True when a piece stands somewhere the king or rook has to pass through.
    bool   castling_impeded(CastlingRights cr) const { return pieces() & castlingPath[cr]; }
    Square castling_rook_square(CastlingRights cr) const { return castlingRookSquare[cr]; }
    bool   is_chess960() const { return chess960; }

    // --- checks and pins ---
    Bitboard checkers() const { return st->checkersBB; }
    Bitboard blockers_for_king(Color c) const { return st->blockersForKing[c]; }
    Bitboard pinners(Color c) const { return st->pinners[c]; }
    Bitboard check_squares(PieceType pt) const { return st->checkSquares[pt]; }

    // All pieces of either colour that attack `s` given `occupied`.
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    Bitboard attackers_to(Square s) const { return attackers_to(s, pieces()); }
    // Pieces blocking a would-be attack on `s` by `sliders`; fills `pinners`.
    Bitboard slider_blockers(Bitboard sliders, Square s, Bitboard& pinners) const;

    // --- move properties ---
    bool  legal(Move m) const;
    bool  pseudo_legal(Move m) const;
    bool  capture(Move m) const;
    bool  capture_stage(Move m) const;  // capture or queen promotion — the qsearch set
    bool  gives_check(Move m) const;
    Piece moved_piece(Move m) const { return board[from_sq(m)]; }
    PieceType captured_piece_type(Move m) const {
        return type_of(m) == EN_PASSANT ? PAWN : type_of(board[to_sq(m)]);
    }
    Piece captured_piece() const { return st->capturedPiece; }

    // --- doing and undoing ---
    void do_move(Move m, StateInfo& newSt);
    void do_move(Move m, StateInfo& newSt, bool givesCheck);
    void undo_move(Move m);
    void do_null_move(StateInfo& newSt);
    void undo_null_move();

    // --- static exchange evaluation ---
    bool see_ge(Move m, int threshold = 0) const;

    // --- draws ---
    bool is_draw(int ply) const;
    bool has_repeated() const;
    bool has_game_cycle(int ply) const;
    // A legal quiet move whose child satisfies our existing repetition rule.
    // MOVE_NONE does not assert that this position is winning or non-drawn.
    Move repetition_move(int ply) const;

    // --- debugging ---
    bool pos_is_ok() const;
    Key  compute_key() const;
    Key  compute_repetition_key() const;
    Key  compute_pawn_key() const;
    Key  compute_minor_piece_key() const;
    Key  compute_non_pawn_key(Color c) const;

   private:
    void set_castling_right(Color c, Square rfrom);
    void set_state() const;
    void set_check_info() const;

    void put_piece(Piece pc, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);
    template<bool Do>
    void do_castling(Color us, Square from, Square& to, Square& rfrom, Square& rto);

    Piece    board[SQUARE_NB]      = {};
    Bitboard byTypeBB[PIECE_TYPE_NB] = {};
    Bitboard byColorBB[COLOR_NB]   = {};
    int      pieceCount[PIECE_NB]  = {};
    int      castlingRightsMask[SQUARE_NB] = {};
    // Per-position castling geometry: where each right's rook starts, and which
    // squares must be empty for that castle to be available.
    Square   castlingRookSquare[CASTLING_RIGHTS_NB] = {};
    Bitboard castlingPath[CASTLING_RIGHTS_NB]       = {};
    bool     chess960              = false;
    Color    sideToMove            = WHITE;
    int      gamePly               = 0;
    uint64_t nodes                 = 0;
    StateInfo* st                  = nullptr;
};

std::ostream& operator<<(std::ostream& os, const Position& pos);

// A move is a capture when the destination holds an enemy piece, or it is en
// passant. Nothing about this is stored in the move itself, so it can never
// disagree with the board.
inline bool Position::capture(Move m) const {
    assert(is_ok(m));
    return (!empty(to_sq(m)) && type_of(m) != CASTLING) || type_of(m) == EN_PASSANT;
}

// What quiescence search considers "noisy": real captures plus queen promotions.
inline bool Position::capture_stage(Move m) const {
    assert(is_ok(m));
    return capture(m) || (type_of(m) == PROMOTION && promotion_type(m) == QUEEN);
}

inline void Position::put_piece(Piece pc, Square s) {
    board[s] = pc;
    byTypeBB[ALL_PIECES] |= s;
    byTypeBB[type_of(pc)] |= s;
    byColorBB[color_of(pc)] |= s;
    pieceCount[pc]++;
}

inline void Position::remove_piece(Square s) {
    const Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
    pieceCount[pc]--;
}

inline void Position::move_piece(Square from, Square to) {
    const Piece     pc   = board[from];
    const Bitboard  fromTo = from | to;
    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to]   = pc;
}

inline void Position::do_move(Move m, StateInfo& newSt) { do_move(m, newSt, gives_check(m)); }

}  // namespace boring

#endif  // BORING_POSITION_H
