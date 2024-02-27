#pragma once
#include "bitboard.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif

namespace attack {
  inline Bitboard files[8] = {
    kFileABb, kFileBBb, kFileCBb, kFileDBb,
    kFileEBb, kFileFBb, kFileGBb, kFileHBb
  };

  inline Bitboard ranks[8] = {
    kRank1Bb, kRank2Bb, kRank3Bb, kRank4Bb,
    kRank5Bb, kRank6Bb, kRank7Bb, kRank8Bb
  };

  enum Directions {
    NORTHEAST,
    NORTH,
    NORTHWEST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    N_DIRECTIONS
  };

  inline int knight_directions[8] = {17, 10, -6, -15, -17, -10, 6, 15};

  inline int king_directions[8] = {8, 9, 1, -7, -8, -9, -1, 7};

  inline int bishop_directions[4] = {9, -7, -9, 7};

  inline int rook_directions[4] = {8, 1, -8, -1};

  inline Bitboard pawn_attacks[N_COLORS][N_SQUARES];
  inline Bitboard knight_attacks[N_SQUARES];
  inline Bitboard king_attacks[N_SQUARES];

  inline Bitboard diagonals[15];
  inline Bitboard anti_diagonals[15];

  inline Bitboard bishop_attacks[N_SQUARES];
  inline Bitboard rook_attacks[N_SQUARES];
  inline Bitboard ray_attacks[N_DIRECTIONS][N_SQUARES];

  inline Bitboard in_between_squares[N_SQUARES][N_SQUARES];
  inline Bitboard diagonals_by_square[N_SQUARES];
  inline Bitboard anti_diagonals_by_square[N_SQUARES];
  inline Bitboard files_by_square[N_SQUARES];
  inline Bitboard ranks_by_square[N_SQUARES];

  inline Bitboard first_rank_attacks[8][64];
  inline Bitboard fill_up_attacks[8][64];
  inline Bitboard a_file_attacks[8][64];

  inline Bitboard DiagonalAttacks(const Square sq, Bitboard occ) {
    occ = diagonals_by_square[sq] & occ;
    occ = occ * kFileBBb >> 58;
    return fill_up_attacks[file::make(sq)][occ.data] & diagonals_by_square[sq];
  }

  inline Bitboard AntiDiagonalAttacks(const Square sq, Bitboard occ) {
    occ = anti_diagonals_by_square[sq] & occ;
    occ = occ * kFileBBb >> 58;
    return fill_up_attacks[file::make(sq)][occ.data] & anti_diagonals_by_square[sq];
  }

  inline Bitboard RankAttacks(const Square sq, Bitboard occ) {
    occ = ranks_by_square[sq] & occ;
    occ = occ * kFileBBb >> 58;
    return fill_up_attacks[file::make(sq)][occ.data] & ranks_by_square[sq];
  }

  inline Bitboard FileAttacks(const Square sq, Bitboard occ) {
    occ = kFileABb & occ >> file::make(sq);
    occ = occ * kDiagC2H7 >> 58;
    return a_file_attacks[rank::make(sq)][occ.data] << file::make(sq);
  }

  template <PieceType Pt>
  Bitboard attacks(const Square sq, const Bitboard occupied) {
    switch (Pt) {
    case BISHOP:
      return DiagonalAttacks(sq, occupied) | AntiDiagonalAttacks(sq, occupied);
    case ROOK:
      return FileAttacks(sq, occupied) | RankAttacks(sq, occupied);
    case QUEEN:
      return DiagonalAttacks(sq, occupied) | AntiDiagonalAttacks(sq, occupied) |
        FileAttacks(sq, occupied) | RankAttacks(sq, occupied);
    default: ;
    }
    return {};
  }

  template <Color C>
  Bitboard PawnAttacksBb(const Bitboard b) {
    return C == WHITE
      ? b.shift<7>() | b.shift<9>()
      : b.shift<-9>() | b.shift<-7>();
  }

  void init();
}
