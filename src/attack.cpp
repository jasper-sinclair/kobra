#include "attack.h"

#include "bitboard.h"
#include "main.h"

namespace attack {
  void init() {
    for (Square from = A1; from < N_SQUARES; ++from) {
      auto attacks = Bitboard();
      for (const auto& d : knight_directions) {
        if (const Square to = from + static_cast<Square>(d);
          square::IsValid(to) && square::distance(from, to) <= 2)
          attacks.set(to);
      }
      knight_attacks[from] = attacks;
    }

    for (Square from = A1; from < N_SQUARES; ++from) {
      auto attacks = Bitboard();
      for (const auto& d : king_directions) {
        if (const Square to = from + static_cast<Square>(d);
          square::IsValid(to) && square::distance(from, to) == 1)
          attacks.set(to);
      }
      king_attacks[from] = attacks;
    }

    diagonals[0] = Bitboard(0x8040201008040201);
    for (int i = 1; i < 8; ++i) {
      diagonals[i] = diagonals[0] << static_cast<uint8_t>(i);
      for (int j = 0; j < i - 1; ++j) diagonals[i] -= ranks[7 - j];
    }

    for (int i = 1; i < 8; ++i) {
      diagonals[i + 7] = diagonals[0] >> static_cast<uint8_t>(i);
      for (int j = 0; j < i - 1; ++j) diagonals[i + 7] -= ranks[j];
    }

    for (int i = 0; i < 15; ++i) anti_diagonals[i] = diagonals[i].mirrored();
    for (Square sq = A1; sq < N_SQUARES; ++sq) {
      Bitboard attacks = {};
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

    for (Square sq = A1; sq < N_SQUARES; ++sq) {
      Bitboard attacks = {};
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

    for (Square sq = A1; sq < N_SQUARES; ++sq) {
      const File file = file::make(sq);
      const Rank rank = rank::make(sq);
      ray_attacks[NORTH][sq] = Bitboard(0x101010101010100) << sq;
      ray_attacks[SOUTH][sq] = Bitboard(0x80808080808080) >> (63 - sq);
      ray_attacks[EAST][sq] = Bitboard(0xfe) << sq;
      if (rank < RANK_8) ray_attacks[EAST][sq] -= ranks[rank + 1];
      ray_attacks[WEST][sq] = Bitboard(0x7f00000000000000) >> (sq ^ 7 ^ 56);
      if (rank > RANK_1) ray_attacks[WEST][sq] -= ranks[rank - 1];
      ray_attacks[NORTHEAST][sq] = Bitboard(0x8040201008040200) << sq;
      for (int i = 0; i < file - rank - 1; ++i)
        ray_attacks[NORTHEAST][sq] -= ranks[7 - i];
      ray_attacks[NORTHWEST][sq] =
        Bitboard(0x102040810204000) >> file::make(sq ^ 7) << 8 * rank;
      for (int i = 7; i > file; --i) ray_attacks[NORTHWEST][sq] -= files[i];
      ray_attacks[SOUTHEAST][sq ^ 56] = ray_attacks[NORTHEAST][sq].mirrored();
      ray_attacks[SOUTHWEST][sq ^ 56] = ray_attacks[NORTHWEST][sq].mirrored();
    }

    for (Square sq = A1; sq < N_SQUARES; ++sq) {
      Bitboard b = Bitboard::FromSquare(sq);
      pawn_attacks[WHITE][sq] |= b.shift<7>() | b.shift<9>();
      pawn_attacks[BLACK][sq] |= b.shift<-9>() | b.shift<-7>();
    }

    for (Square i = A1; i < N_SQUARES; ++i) {
      for (Square j = A1; j < N_SQUARES; ++j) {
        const Bitboard b = Bitboard::FromSquare(j);
        for (const auto& arr : ray_attacks) {
          if (arr[i] & b) {
            in_between_squares[i][j] = arr[i] - arr[j] - b;
            break;
          }
        }
      }
    }

    for (Square i = 0; i < 64; ++i) {
      const Bitboard b = Bitboard::FromSquare(i);
      for (const auto& d : diagonals) {
        if (d & b) {
          diagonals_by_square[i] = d - b;
          break;
        }
      }
      for (const auto& d : anti_diagonals) {
        if (d & b) {
          anti_diagonals_by_square[i] = d - b;
          break;
        }
      }
      for (const auto& d : files) {
        if (d & b) {
          files_by_square[i] = d - b;
          break;
        }
      }
      for (const auto& d : ranks) {
        if (d & b) {
          ranks_by_square[i] = d - b;
          break;
        }
      }
    }

    for (Square i = 0; i < 8; ++i) {
      for (int j = 0; j < 64; ++j) {
        Bitboard occ = Bitboard(j << 1) - Bitboard::FromSquare(i);
        for (int k = i - 1;; --k) {
          if (k < 0) break;
          first_rank_attacks[i][j].set(static_cast<Square>(k));
          if (occ.IsSet(static_cast<Square>(k))) break;
        }
        for (int k = i + 1;; ++k) {
          if (k > 7) break;
          first_rank_attacks[i][j].set(static_cast<Square>(k));
          if (occ.IsSet(static_cast<Square>(k))) break;
        }
        fill_up_attacks[i][j] = kFileABb * first_rank_attacks[i][j];
        const Square r = i ^ 7;
        occ = Bitboard(j << 1) - Bitboard::FromSquare(r);
        for (int k = r - 1;; --k) {
          if (k < 0) break;
          a_file_attacks[i][j].set(static_cast<Square>(8 * (k ^ 7)));
          if (occ.IsSet(static_cast<Square>(k))) break;
        }
        for (int k = r + 1;; ++k) {
          if (k > 7) break;
          a_file_attacks[i][j].set(static_cast<Square>(8 * (k ^ 7)));
          if (occ.IsSet(static_cast<Square>(k))) break;
        }
      }
    }
  }
}
