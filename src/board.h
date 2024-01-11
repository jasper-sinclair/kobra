#pragma once

#include <vector>

#include "bitboard.h"
#include "random.h"

const std::string kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr Bitboard kWhiteQueenSidePath(0xe);
constexpr Bitboard kWhiteKingSidePath(0x60);
constexpr Bitboard kBlackQueenSidePath(0xE00000000000000);
constexpr Bitboard kBlackKingSidePath(0x6000000000000000);

struct Castlings {
  int data;

  void set(int cr) { data |= cr; }
  void reset(int cr) { data &= ~cr; }
  bool CanCastle(int cr) { return data & cr; }
  bool NoCastling() { return data == 0; }
};

struct KingAttackInfo {
  Bitboard pinned;
  Bitboard attacks;
  bool double_check;
  bool computed;

  bool check() { return bool(attacks); }
};

struct BoardStatus {
  int ply_count;
  int fifty_move_count;
  Castlings castlings;
  Key zobrist;

  int repetitions;
  Square ep_square;
  Piece captured;
  uint16_t move;
  KingAttackInfo king_attack_info;

  friend std::ostream& operator<<(std::ostream& os, BoardStatus& bs);
};

struct Board {
  Piece board[N_SQUARES];
  Bitboard piece_bb[N_PIECE_TYPES];
  Bitboard color_bb[N_COLORS];
  Bitboard occupied_bb;
  Color side_to_move;
  std::vector<BoardStatus> history;
  BoardStatus* st;

  Board() = default;
  Board(const std::string& fen);

  Board(const Board& other);
  Board& operator=(const Board& other);

  friend std::ostream& operator<<(std::ostream& os, Board& board);

  std::string fen();

  Piece PieceOn(Square sq);
  template <bool UpdateZobrist = true>
  void SetPiece(Piece pc, Square sq);
  template <bool UpdateZobrist = true>
  void RemovePiece(Square sq);
  template <bool UpdateZobrist = true>
  void MovePiece(Square from, Square to);

  BoardStatus* GetBoardStatus();
  BoardStatus* boardStatus(int idx);
  Key key();

  bool CanCastle(int cr);
  bool NoCastling();

  void ApplyMove(uint16_t m);
  void UndoMove();
  void ApplyNullMove();
  void UndoNullMove();

  Bitboard color(Color c);
  Bitboard pieces(int32_t pt);
  Bitboard pieces(Color c, int32_t pt);
  Bitboard occupied();
  // King square
  Square ksq(Color c);

  // Draw by fifty-move rule or threefold repetition
  bool IsDraw();

  bool IsCapture(uint16_t m);
  bool IsPromotion(uint16_t m);
  bool GivesCheck(uint16_t m);

  // Test whether a move from the transposition table or the principal
  // variation is pseudo-legal
  bool IsPseudoLegal(uint16_t m);
  // Test whether a pseudo-legal move is legal
  bool IsLegal(uint16_t m);

  bool IsUnderAttack(Color us, Square sq);
  bool IsInCheck();
  void GenerateKingAttackInfo(KingAttackInfo& k);

  Bitboard LeastValuablePiece(Bitboard attacking, Color attacker, Piece& pc);

  bool NonPawnMaterial(Color c);
};

inline Piece Board::PieceOn(Square sq) { return board[sq]; }

inline BoardStatus* Board::GetBoardStatus() { return &history.back(); }

inline BoardStatus* Board::boardStatus(int idx) {
  return &history[history.size() - idx - 1];
}

inline bool Board::CanCastle(int cr) { return st->castlings.CanCastle(cr); }

inline bool Board::NoCastling() { return st->castlings.NoCastling(); }

inline Bitboard Board::color(Color c) { return color_bb[c]; }

inline Bitboard Board::pieces(int32_t pt) { return piece_bb[pt]; }

inline Bitboard Board::pieces(Color c, int32_t pt) {
  return color(c) & pieces(pt);
}

inline Bitboard Board::occupied() { return occupied_bb; }

inline Square Board::ksq(Color c) { return pieces(c, KING).LSB(); }

inline Key Board::key() { return st->zobrist; }

inline bool Board::IsDraw() {
  return st->repetitions >= 2 || st->fifty_move_count >= 2 * 50;
}

inline bool Board::IsCapture(uint16_t m) {
  return PieceOn(move::to(m)) || move::moveType(m) == move::EN_PASSANT;
}

inline bool Board::IsPromotion(uint16_t m) {
  return move::moveType(m) == move::PROMOTION;
}

inline bool Board::NonPawnMaterial(Color c) {
  return bool(color(c) - pieces(PAWN) - pieces(KING));
}

namespace zobrist {
inline Key psq[16][N_SQUARES];
inline Key side;
inline Key castling[16];
inline Key en_passant[N_FILES];

inline void init() {
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < N_SQUARES; ++j) psq[i][j] = rand64();
  }
  side = rand64();
  for (int i = 0; i < 16; ++i) castling[i] = rand64();
  for (int i = 0; i < N_FILES; ++i) en_passant[i] = rand64();
}
}  // namespace zobrist