#include "history.h"

#include <cassert>

#include "search.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif

void ButterflyHistory::increase(const Board& board, const Move move,
                                const Depth d) {
  auto& e = data()[board.side_to_move][move::from(move)][move::to(move)];
  e += i_a * p(d) * (max - e) / max;
  assert(std::abs(e) < MAX);
}

void ButterflyHistory::decrease(const Board& board, const Move move,
                                const Depth d) {
  auto& e = data()[board.side_to_move][move::from(move)][move::to(move)];
  e -= d_a * p(d) * (max + e) / max;
  assert(std::abs(e) < MAX);
}

void CaptureHistory::increase(const Board& board, const Move move,
                              const Depth d) {
  const Piece moved = board.PieceOn(move::from(move));
  const Square to =
      move::move_type(move) == move::EN_PASSANT
          ? move::to(move) -
                static_cast<Square>(direction::PawnPush(board.side_to_move))
          : move::to(move);
  const PieceType captured = piece_type::make(board.PieceOn(to));
  auto& e = data()[moved][to][captured];
  e += i_a * p(d) * (max - e) / max;
  assert(std::abs(e) < MAX);
}

void CaptureHistory::decrease(const Board& board, const Move move,
                              const Depth d) {
  const Piece moved = board.PieceOn(move::from(move));
  const Square to =
      move::move_type(move) == move::EN_PASSANT
          ? move::to(move) -
                static_cast<Square>(direction::PawnPush(board.side_to_move))
          : move::to(move);
  const PieceType captured = piece_type::make(board.PieceOn(to));
  auto& e = data()[moved][to][captured];
  e -= d_a * p(d) * (max + e) / max;
  assert(std::abs(e) < MAX);
}

void ContinuationHistory::increase(const Board& board, const Stack* ss,
                                   const Move move, const Depth d) {
  if ((ss - 1)->move) {
    auto& e = data()[(ss - 1)->moved][move::to((ss - 1)->move)]
                    [board.PieceOn(move::from(move))][move::to(move)];
    e += i_a1 * p(d) * (max - e) / max;
    assert(std::abs(e) < MAX);
    if ((ss - 2)->move) {
      e = data()[(ss - 2)->moved][move::to((ss - 2)->move)]
                [board.PieceOn(move::from(move))][move::to(move)];
      e += i_a2 * p(d) * (max - e) / max;
      assert(std::abs(e) < MAX);
      if ((ss - 4)->move) {
        e = data()[(ss - 4)->moved][move::to((ss - 4)->move)]
                  [board.PieceOn(move::from(move))][move::to(move)];
        e += i_a4 * p(d) * (max - e) / max;
        assert(std::abs(e) < MAX);
      }
    }
  }
}

void ContinuationHistory::decrease(const Board& board, const Stack* ss,
                                   const Move move, const Depth d) {
  if ((ss - 1)->move) {
    auto& e = data()[(ss - 1)->moved][move::to((ss - 1)->move)]
                    [board.PieceOn(move::from(move))][move::to(move)];
    e -= d_a1 * p(d) * (max + e) / max;
    assert(std::abs(e) < MAX);
    if ((ss - 2)->move) {
      e = data()[(ss - 2)->moved][move::to((ss - 2)->move)]
                [board.PieceOn(move::from(move))][move::to(move)];
      e -= d_a2 * p(d) * (max + e) / max;
      assert(std::abs(e) < MAX);
      if ((ss - 4)->move) {
        e = data()[(ss - 4)->moved][move::to((ss - 4)->move)]
                  [board.PieceOn(move::from(move))][move::to(move)];
        e -= d_a4 * p(d) * (max + e) / max;
        assert(std::abs(e) < MAX);
      }
    }
  }
}

void Histories::update(const Board& board, const Stack* ss, const Move best_move,
                       MoveList& moves, const Depth depth) {
  if (board.IsCapture(best_move)) {
    capture.increase(board, best_move, depth);
    for (const auto& [move, score] : moves) {
      if (move != best_move && board.IsCapture(move)) {
        capture.decrease(board, move, depth);
      }
    }
  } else {
    if (best_move != killer[ss->ply][0]) {
      killer[ss->ply][1] = killer[ss->ply][0];
      killer[ss->ply][0] = best_move;
    }
    if ((ss - 1)->move)
      counter[(ss - 1)->moved][move::to((ss - 1)->move)] = best_move;
    butterfly.increase(board, best_move, depth);
    continuation.increase(board, ss, best_move, depth);
    for (const auto& [move, score] : moves) {
      if (move != best_move) {
        if (board.IsCapture(move)) {
          capture.decrease(board, move, depth);
        } else {
          butterfly.decrease(board, move, depth);
          continuation.decrease(board, ss, move, depth);
        }
      }
    }
  }
}

void Histories::clear() {
  std::memset(killer, 0, sizeof(killer));
  std::memset(counter, 0, sizeof(counter));
  butterfly.fill(butterfly_fill);
  capture.fill(capture_fill);
  continuation.fill(continuation_fill);
}
