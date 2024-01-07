#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

using Color = bool;
using Piece = int32_t;
using File = int8_t;
using Rank = int8_t;
using Direction = int32_t;
using Square = uint8_t;
using Key = uint64_t;
using Score = int16_t;
using Depth = int16_t;

constexpr int kMaxDepth = 256;
constexpr int kMaxPly = kMaxDepth;

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

namespace color {
constexpr Color make(Piece pc) { return pc >> 3; }
}  // namespace color

namespace piece_type {
constexpr int32_t make(Piece pc) { return pc & 7; }
}  // namespace piece_type

namespace piece {
constexpr Piece make(Color c, int32_t pt) { return (c << 3) + pt; }

constexpr Piece relative(Piece pc) { return pc ? pc ^ 8 : pc; }

const std::string kPieceToChar = " PNBRQK  pnbrqk";
}  // namespace piece

namespace file {
constexpr File make(Square sq) { return sq & 7; }

constexpr char kCharIdentifiers[N_FILES] = {'a', 'b', 'c', 'd',
                                            'e', 'f', 'g', 'h'};
}  // namespace file

namespace rank {
constexpr Rank make(Square sq) { return sq >> 3; }

constexpr char kCharIdentifiers[N_RANKS] = {'1', '2', '3', '4',
                                            '5', '6', '7', '8'};
}  // namespace rank

namespace direction {
constexpr Direction PawnPush(Color c) { return c ? SOUTH : NORTH; }
}  // namespace direction

namespace square {
constexpr Square make(File file, Rank rank) { return (rank << 3) + file; }

constexpr Square make(std::string_view sv) {
  return square::make(File(sv[0] - 'a'), Rank(sv[1] - '1'));
}

constexpr void mirror(Square& sq) { sq ^= A8; }

constexpr Square relative(Color c, Square sq) { return c ? sq ^ A8 : sq; }

inline std::string ToString(Square sq) {
  return std::string(1, file::kCharIdentifiers[file::make(sq)]) +
         std::string(1, rank::kCharIdentifiers[rank::make(sq)]);
}

constexpr bool IsValid(Square sq) { return sq >= A1 && sq <= H8; }

inline uint8_t distance(Square s1, Square s2) {
  return std::max(std::abs(file::make(s1) - file::make(s2)),
                  std::abs(rank::make(s1) - rank::make(s2)));
}
}  // namespace square

struct Bitboard {
  uint64_t data;

  Bitboard() = default;
  constexpr Bitboard(uint64_t data) : data(data) {}

  constexpr static Bitboard FromSquare(Square sq) {
    return {uint64_t(1) << sq};
  }

  constexpr bool IsSet(Square sq) { return data & uint64_t(1) << sq; }

  constexpr void set(Square sq) { data |= uint64_t(1) << sq; }

  constexpr void clear(Square sq) { data &= ~(uint64_t(1) << sq); }

  constexpr void toggle(Square sq) { data ^= uint64_t(1) << sq; }

  template <Direction D>
  constexpr Bitboard shift();

#if defined(_MSC_VER)

  void mirror() { data = _byteswap_uint64(data); }

  Bitboard mirrored() { return _byteswap_uint64(data); }

  int popcount() { return (int)_mm_popcnt_u64(data); }

  Square LSB() {
    unsigned long idx;
    _BitScanForward64(&idx, data);
    return Square(idx);
  }

  Square MSB() {
    unsigned long idx;
    _BitScanReverse64(&idx, data);
    return Square(idx);
  }

#else
  void mirror() {
    constexpr static uint64_t k1 = 0x00ff00ff00ff00ff;
    constexpr static uint64_t k2 = 0x0000ffff0000ffff;
    data = data >> 8 & k1 | (data & k1) << 8;
    data = data >> 16 & k2 | (data & k2) << 16;
    data = data >> 32 | data << 32;
  }

  Bitboard mirrored() {
    Bitboard copy(data);
    copy.mirror();
    return copy;
  }

  int popcount() { return __builtin_popcountll(data); }

  Square LSB() { return Square(__builtin_ctzll(data)); }

  Square MSB() { return Square(__builtin_clzll(data) ^ 63); }

#endif

  Square PopLsb() {
    Square sq = LSB();
    data &= data - 1;
    return sq;
  }

  Square PopMsb() {
    Square sq = MSB();
    data &= data - 1;
    return sq;
  }

  constexpr friend Bitboard operator&(Bitboard a, Bitboard b) {
    return a.data & b.data;
  }

  constexpr friend Bitboard operator|(Bitboard a, Bitboard b) {
    return a.data | b.data;
  }

  constexpr friend Bitboard operator^(Bitboard a, Bitboard b) {
    return a.data ^ b.data;
  }

  constexpr friend Bitboard operator-(Bitboard a, Bitboard b) {
    return a.data & ~b.data;
  }

  constexpr friend Bitboard operator*(Bitboard a, Bitboard b) {
    return a.data * b.data;
  }

  constexpr void operator&=(Bitboard b) { data &= b.data; }

  constexpr void operator|=(Bitboard b) { data |= b.data; }

  constexpr void operator^=(Bitboard b) { data ^= b.data; }

  constexpr void operator-=(Bitboard b) { data &= ~b.data; }

  constexpr Bitboard operator~() { return ~data; }

  constexpr explicit operator bool() { return bool(data); }

  constexpr friend Bitboard operator<<(Bitboard b, uint8_t shift) {
    return b.data << shift;
  }

  constexpr friend Bitboard operator>>(Bitboard b, uint8_t shift) {
    return b.data >> shift;
  }

  constexpr bool operator==(Bitboard b) { return data == b.data; }

  friend std::ostream& operator<<(std::ostream& os, Bitboard b) {
    for (Rank r = RANK_8; r >= RANK_1; --r) {
      for (File f = FILE_A; f <= FILE_H; ++f) {
        Square sq = square::make(f, r);
        os << (b.IsSet(sq) ? 'X' : '.') << ' ';
      }
      os << '\n';
    }
    return os;
  }
};

constexpr Bitboard kFileABb(0x101010101010101);
constexpr Bitboard kFileBBb = kFileABb << 1;
constexpr Bitboard kFileCBb = kFileABb << 2;
constexpr Bitboard kFileDBb = kFileABb << 3;
constexpr Bitboard kFileEBb = kFileABb << 4;
constexpr Bitboard kFileFBb = kFileABb << 5;
constexpr Bitboard kFileGBb = kFileABb << 6;
constexpr Bitboard kFileHBb = kFileABb << 7;

constexpr Bitboard kRank1Bb(0xff);
constexpr Bitboard kRank2Bb = kRank1Bb << 8;
constexpr Bitboard kRank3Bb = kRank1Bb << 16;
constexpr Bitboard kRank4Bb = kRank1Bb << 24;
constexpr Bitboard kRank5Bb = kRank1Bb << 32;
constexpr Bitboard kRank6Bb = kRank1Bb << 40;
constexpr Bitboard kRank7Bb = kRank1Bb << 48;
constexpr Bitboard kRank8Bb = kRank1Bb << 56;

constexpr Bitboard kDiagC2H7(0x0080402010080400);

template <Direction D>
constexpr Bitboard Bitboard::shift() {
  Bitboard b(*this);
  if (D == NORTH)
    return b << NORTH;
  else if (D == SOUTH)
    return b >> NORTH;
  else if (D == 2 * NORTH)
    return b << 2 * NORTH;
  else if (D == 2 * SOUTH)
    return b >> 2 * NORTH;
  else if (D == NORTHEAST)
    return (b << NORTHEAST) - kFileABb;
  else if (D == NORTHWEST)
    return (b << NORTHWEST) - kFileHBb;
  else if (D == SOUTHEAST)
    return (b >> NORTHWEST) - kFileABb;
  else if (D == SOUTHWEST)
    return (b >> NORTHEAST) - kFileHBb;
  else
    return {};
}

namespace move {
// A move has the following format:

// bit 0-5:   from square
// bit 6-11:  to square
// bit 12-13: promotion piece type
// bit 14-15: move type

enum MoveType {
  NORMAL,
  EN_PASSANT = 1 << 14,
  PROMOTION = 2 << 14,
  CASTLING = 3 << 14
};

constexpr uint16_t make(Square from, Square to) { return from | to << 6; }
constexpr uint16_t make(Square from, Square to, MoveType moveType) {
  return from | to << 6 | moveType;
}
template <int32_t Pt>
constexpr uint16_t make(Square from, Square to) {
  return from | to << 6 | PROMOTION | (Pt - 2) << 12;
}

constexpr Square from(uint16_t m) { return m & 0x3f; }

constexpr Square to(uint16_t m) { return m >> 6 & 0x3f; }

constexpr MoveType moveType(uint16_t m) { return MoveType(m & 0x3 << 14); }

constexpr int32_t PieceType(uint16_t m) { return (m >> 12 & 0x3) + 2; }

inline std::string ToString(uint16_t m) {
  std::string s = square::ToString(from(m)) + square::ToString(to(m));

  if (moveType(m) == PROMOTION) {
    switch (PieceType(m)) {
      case KNIGHT:
        return s + 'n';
      case BISHOP:
        return s + 'b';
      case ROOK:
        return s + 'r';
      case QUEEN:
        return s + 'q';
    };
  }
  return s;
}
};  // namespace move