#pragma once
#include <array>
#include <cstdint>

#include "movegen.h"

using HistoryEntry = int16_t;

template <size_t Size, size_t... Sizes>
struct History : std::array<History<Sizes...>, Size> {
  void fill(const HistoryEntry value) {
    const auto e = reinterpret_cast<HistoryEntry*>(this);
    std::fill_n(e, sizeof(*this) / sizeof(HistoryEntry), value);
  }

  static constexpr HistoryEntry max_entry = 30000;

  static HistoryEntry p(const Depth d) {
    return static_cast<HistoryEntry>(std::min(153 * d - 133, 1525));
  }
};

template <size_t Size>
struct History<Size> : std::array<HistoryEntry, Size> {};

struct CaptureHistory : History<N_PIECES, N_SQUARES, N_PIECE_TYPES> {
  static constexpr HistoryEntry i_a = 7;
  static constexpr HistoryEntry d_a = 8;

  void increase(const Board& board, Move move, Depth d);
  void decrease(const Board& board, Move move, Depth d);
};

struct ButterflyHistory : History<N_COLORS, N_SQUARES, N_SQUARES> {
  static constexpr HistoryEntry i_a = 10;
  static constexpr HistoryEntry d_a = 5;

  void increase(const Board& board, Move move, Depth d);
  void decrease(const Board& board, Move move, Depth d);
};

struct Stack;

struct ContinuationHistory : History<N_PIECES, N_SQUARES, N_PIECES, N_SQUARES> {
  static constexpr HistoryEntry i_a1 = 2;
  static constexpr HistoryEntry i_a2 = 2;
  static constexpr HistoryEntry i_a4 = 2;
  static constexpr HistoryEntry d_a1 = 2;
  static constexpr HistoryEntry d_a2 = 2;
  static constexpr HistoryEntry d_a4 = 2;

  void increase(const Board& board, const Stack* ss, Move move, Depth d);
  void decrease(const Board& board, const Stack* ss, Move move, Depth d);
};

struct Histories {
  ButterflyHistory butterfly;
  CaptureHistory capture;
  ContinuationHistory continuation;

  Move counter[N_PIECES][N_SQUARES];
  Move killer[kMaxDepth + 1][2];

  static constexpr HistoryEntry butterfly_fill = -425;
  static constexpr HistoryEntry capture_fill = -970;
  static constexpr HistoryEntry continuation_fill = -470;

  void clear();
  void update(const Board& board, const Stack* ss, Move best_move,
              MoveList& moves, Depth depth);
  Histories() { clear(); }
};
