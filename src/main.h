#pragma once
#include <cstdint>

using Color = bool;
using Depth = int16_t;
using Direction = int32_t;
using File = int8_t;
using Key = uint64_t;
using Move = uint16_t;
using Piece = int32_t;
using PieceType = int32_t;
using Rank = int8_t;
using Score = int16_t;
using Square = uint8_t;

inline constexpr int kMaxDepth = 256;
inline constexpr int kMaxPly = kMaxDepth;
inline constexpr int kContinuationPly = 6;

enum Colors { WHITE, BLACK, N_COLORS };

enum Pieces {
  NO_PIECE,
  WHITE_PAWN,
  WHITE_KNIGHT,
  WHITE_BISHOP,
  WHITE_ROOK,
  WHITE_QUEEN,
  WHITE_KING,
  BLACK_PAWN = WHITE_PAWN + 8,
  BLACK_KNIGHT,
  BLACK_BISHOP,
  BLACK_ROOK,
  BLACK_QUEEN,
  BLACK_KING,
  N_PIECES
};

enum PieceTypes {
  NO_PIECE_TYPE,
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING,
  N_PIECE_TYPES
};

enum Squares {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8,

  NO_SQUARE = 0,
  N_SQUARES = 64
};

enum Files {
  FILE_A,
  FILE_B,
  FILE_C,
  FILE_D,
  FILE_E,
  FILE_F,
  FILE_G,
  FILE_H,
  N_FILES
};

enum Ranks {
  RANK_1,
  RANK_2,
  RANK_3,
  RANK_4,
  RANK_5,
  RANK_6,
  RANK_7,
  RANK_8,
  N_RANKS
};

enum Directions {
  NORTH = 8,
  EAST = 1,
  SOUTH = -NORTH,
  WEST = -EAST,
  NORTHEAST = NORTH + EAST,
  NORTHWEST = NORTH + WEST,
  SOUTHEAST = SOUTH + EAST,
  SOUTHWEST = SOUTH + WEST,
  N_DIRECTIONS = 8
};

enum CastlingRights {
  NO_CASTLING,
  WHITE_QUEEN_SIDE = 1,
  WHITE_KING_SIDE = 1 << 1,
  BLACK_QUEEN_SIDE = 1 << 2,
  BLACK_KING_SIDE = 1 << 3,
  QUEEN_SIDE = WHITE_QUEEN_SIDE | BLACK_QUEEN_SIDE,
  KING_SIDE = WHITE_KING_SIDE | BLACK_KING_SIDE,
  WHITE_CASTLING = WHITE_KING_SIDE | WHITE_QUEEN_SIDE,
  BLACK_CASTLING = BLACK_KING_SIDE | BLACK_QUEEN_SIDE,
  ANY_CASTLING = WHITE_CASTLING | BLACK_CASTLING,
};

enum Scores : int16_t {
  DRAW_SCORE = 0,
  INFINITY_SCORE = 32001,
  MATE_SCORE = 32000,
  MIN_MATE_SCORE = MATE_SCORE - kMaxDepth,
  STOP_SCORE = 32002,
  MAX_EVAL_SCORE = 30000
};