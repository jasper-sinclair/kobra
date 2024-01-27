#pragma once

#include "bitboard.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif
namespace attacks {
inline Bitboard pawn_attacks[N_COLORS][N_SQUARES];
inline Bitboard knight_attacks[N_SQUARES];
inline Bitboard king_attacks[N_SQUARES];
inline int knight_directions[8] = {17, 10, -6, -15, -17, -10, 6, 15};
inline int king_directions[8] = {8, 9, 1, -7, -8, -9, -1, 7};
inline int bishop_directions[4] = {9, -7, -9, 7};
inline int rook_directions[4] = {8, 1, -8, -1};

inline Bitboard files[8] = {kFileABb, kFileBBb, kFileCBb, kFileDBb,
                            kFileEBb, kFileFBb, kFileGBb, kFileHBb};
inline Bitboard ranks[8] = {kRank1Bb, kRank2Bb, kRank3Bb, kRank4Bb,
                            kRank5Bb, kRank6Bb, kRank7Bb, kRank8Bb};
inline Bitboard diagonals[15];
inline Bitboard anti_diagonals[15];
inline Bitboard bishop_attacks[N_SQUARES];
inline Bitboard rook_attacks[N_SQUARES];

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
inline Bitboard ray_attacks[N_DIRECTIONS][N_SQUARES];
inline Bitboard in_between_squares[N_SQUARES][N_SQUARES];
inline Bitboard diagonals_by_square[N_SQUARES];
inline Bitboard anti_diagonals_by_square[N_SQUARES];
inline Bitboard files_by_square[N_SQUARES];
inline Bitboard ranks_by_square[N_SQUARES];
inline Bitboard first_rank_attacks[8][64];
inline Bitboard fill_up_attacks[8][64];
inline Bitboard a_file_attacks[8][64];

inline void init() {
  // knight attacks
  for (Square from = A1; from < N_SQUARES; ++from) {
    Bitboard attacks = Bitboard();
    for (const auto& d : knight_directions) {
      Square to = from + d;
      if (square::IsValid(to) && square::distance(from, to) <= 2)
        attacks.set(to);
    }
    knight_attacks[from] = attacks;
  }

  // king attacks
  for (Square from = A1; from < N_SQUARES; ++from) {
    Bitboard attacks = Bitboard();
    for (const auto& d : king_directions) {
      Square to = from + d;
      if (square::IsValid(to) && square::distance(from, to) == 1)
        attacks.set(to);
    }
    king_attacks[from] = attacks;
  }

  // diagonals
  diagonals[0] = Bitboard(0x8040201008040201);
  for (int i = 1; i < 8; ++i) {
    diagonals[i] = diagonals[0] << i;
    for (int j = 0; j < i - 1; ++j) diagonals[i] -= ranks[7 - j];
  }
  for (int i = 1; i < 8; ++i) {
    diagonals[i + 7] = diagonals[0] >> i;
    for (int j = 0; j < i - 1; ++j) diagonals[i + 7] -= ranks[j];
  }

  // anti-diagonals
  for (int i = 0; i < 15; ++i) anti_diagonals[i] = diagonals[i].mirrored();

  // bishop attacks on empty board
  for (Square sq = A1; sq < N_SQUARES; ++sq) {
    Bitboard attacks;
    for (auto& b : diagonals) {
      if (b.IsSet(sq)) {
        attacks = b;
        break;
      }
    }
    for (auto& b : anti_diagonals) {
      if (b.IsSet(sq)) {
        attacks ^= b;
        break;
      }
    }
    bishop_attacks[sq] = attacks;
  }

  // rook attacks on empty board
  for (Square sq = A1; sq < N_SQUARES; ++sq) {
    Bitboard attacks;
    for (auto& b : files) {
      if (b.IsSet(sq)) {
        attacks = b;
        break;
      }
    }
    for (auto& b : ranks) {
      if (b.IsSet(sq)) {
        attacks ^= b;
        break;
      }
    }
    rook_attacks[sq] = attacks;
  }

  // ray attacks
  for (Square sq = A1; sq < N_SQUARES; ++sq) {
    File file = file::make(sq);
    Rank rank = rank::make(sq);
    ray_attacks[NORTH][sq] = Bitboard(0x101010101010100) << sq;
    ray_attacks[SOUTH][sq] = Bitboard(0x80808080808080) >> 63 - sq;
    ray_attacks[EAST][sq] = Bitboard(0xfe) << sq;
    if (rank < RANK_8) ray_attacks[EAST][sq] -= ranks[rank + 1];
    ray_attacks[WEST][sq] = Bitboard(0x7f00000000000000) >> (sq ^ 7 ^ 56);
    if (rank > RANK_1) ray_attacks[WEST][sq] -= ranks[rank - 1];
    ray_attacks[NORTHEAST][sq] = Bitboard(0x8040201008040200) << sq;
    for (int i = 0; i < file - rank - 1; ++i)
      ray_attacks[NORTHEAST][sq] -= ranks[7 - i];
    ray_attacks[NORTHWEST][sq] =
        (Bitboard(0x102040810204000) >> file::make(sq ^ 7)) << 8 * rank;
    for (int i = 7; i > file; --i) ray_attacks[NORTHWEST][sq] -= files[i];
    ray_attacks[SOUTHEAST][sq ^ 56] = ray_attacks[NORTHEAST][sq].mirrored();
    ray_attacks[SOUTHWEST][sq ^ 56] = ray_attacks[NORTHWEST][sq].mirrored();
  }

  // pawn attacks
  for (Square sq = A1; sq < N_SQUARES; ++sq) {
    Bitboard b = Bitboard::FromSquare(sq);
    pawn_attacks[WHITE][sq] |= b.shift<7>() | b.shift<9>();
    pawn_attacks[BLACK][sq] |= b.shift<-9>() | b.shift<-7>();
  }

  // squares in between two squares
  for (Square i = A1; i < N_SQUARES; ++i) {
    for (Square j = A1; j < N_SQUARES; ++j) {
      Bitboard b = Bitboard::FromSquare(j);
      for (const auto& arr : ray_attacks) {
        if (arr[i] & b) {
          in_between_squares[i][j] = arr[i] - arr[j] - b;
          break;
        }
      }
    }
  }

  for (Square i = 0; i < 64; ++i) {
    Bitboard b = Bitboard::FromSquare(i);
    // diagonals by square
    for (auto& d : diagonals) {
      if (d & b) {
        diagonals_by_square[i] = d - b;
        break;
      }
    }
    // anti-diagonals by square
    for (auto& d : anti_diagonals) {
      if (d & b) {
        anti_diagonals_by_square[i] = d - b;
        break;
      }
    }
    // files by square
    for (auto& d : files) {
      if (d & b) {
        files_by_square[i] = d - b;
        break;
      }
    }
    // ranks by square
    for (auto& d : ranks) {
      if (d & b) {
        ranks_by_square[i] = d - b;
        break;
      }
    }
  }

  for (Square i = 0; i < 8; ++i) {
    for (int j = 0; j < 64; ++j) {
      // first rank attacks,
      Bitboard occ = Bitboard(j << 1) - Bitboard::FromSquare(i);

      for (int k = i - 1;; --k) {
        if (k < 0) break;
        first_rank_attacks[i][j].set(k);
        if (occ.IsSet(k)) break;
      }
      for (int k = i + 1;; ++k) {
        if (k > 7) break;
        first_rank_attacks[i][j].set(k);
        if (occ.IsSet(k)) break;
      }

      // fill-up attacks
      fill_up_attacks[i][j] = kFileABb * first_rank_attacks[i][j];

      // A-file attacks
      Square r = i ^ 7;
      occ = Bitboard(j << 1) - Bitboard::FromSquare(r);
      for (int k = r - 1;; --k) {
        if (k < 0) break;
        a_file_attacks[i][j].set(8 * (k ^ 7));
        if (occ.IsSet(k)) break;
      }
      for (int k = r + 1;; ++k) {
        if (k > 7) break;
        a_file_attacks[i][j].set(8 * (k ^ 7));
        if (occ.IsSet(k)) break;
      }
    }
  }
}

inline Bitboard DiagonalAttacks(Square sq, Bitboard occ) {
  occ = diagonals_by_square[sq] & occ;
  occ = occ * kFileBBb >> 58;
  return fill_up_attacks[file::make(sq)][occ.data] & diagonals_by_square[sq];
}

inline Bitboard AntiDiagonalAttacks(Square sq, Bitboard occ) {
  occ = anti_diagonals_by_square[sq] & occ;
  occ = occ * kFileBBb >> 58;
  return fill_up_attacks[file::make(sq)][occ.data] &
         anti_diagonals_by_square[sq];
}

inline Bitboard RankAttacks(Square sq, Bitboard occ) {
  occ = ranks_by_square[sq] & occ;
  occ = occ * kFileBBb >> 58;
  return fill_up_attacks[file::make(sq)][occ.data] & ranks_by_square[sq];
}

inline Bitboard FileAttacks(Square sq, Bitboard occ) {
  occ = kFileABb & (occ >> file::make(sq));
  occ = occ * kDiagC2H7 >> 58;
  return a_file_attacks[rank::make(sq)][occ.data] << file::make(sq);
}

template <int32_t Pt>
inline Bitboard attacks(Square sq, Bitboard occupied) {
  switch (Pt) {
    case BISHOP:
      return DiagonalAttacks(sq, occupied) | AntiDiagonalAttacks(sq, occupied);
    case ROOK:
      return FileAttacks(sq, occupied) | RankAttacks(sq, occupied);
    case QUEEN:
      return DiagonalAttacks(sq, occupied) | AntiDiagonalAttacks(sq, occupied) |
             FileAttacks(sq, occupied) | RankAttacks(sq, occupied);
  }
  return {};
}

}  // namespace attacks
