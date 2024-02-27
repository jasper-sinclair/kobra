#include "movesort.h"

#include <algorithm>
#include <cassert>
#include "bitboard.h"
#include "eval.h"
#include "search.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif

MoveSort::MoveSort(Board& board, Stack* ss, Histories& histories,
                   const Move hash_move, const bool is_in_check)
  : board(board),
    histories(histories),
    is_in_check(is_in_check),
    idx{},
    stage(HASH_MOVE),
    hash_move(hash_move),
    ss(ss) {
  assert(!histories.killer[ss->ply][0] ||
    histories.killer[ss->ply][0] != histories.killer[ss->ply][1]);
}

Move MoveSort::next() {
  switch (stage) {
  case HASH_MOVE:
    ++stage;
    if (board.IsPseudoLegal(hash_move) && board.IsLegal(hash_move))
      return hash_move;
    [[fallthrough]];
  case NORMAL_INIT:
    ++stage;
    GenMoves(board, moves);
    for (size_t i = 0; i < moves.size(); ++i) {
      if (moves.move(i) == hash_move) {
        moves.remove(i);
        break;
      }
    }
    ComputeScores();
    std::ranges::sort(moves, [](const MoveData& m1, const MoveData& m2) {
      return m1.score > m2.score;
    });
    [[fallthrough]];
  case NORMAL:
    if (static_cast<size_t>(idx) == moves.size()) return Move();
    return moves.move(idx++);
  default: ;
  }
  return 0;
}

void MoveSort::ComputeScores() {
  const Color us = board.side_to_move;
  const Color them = !us;
  const Bitboard threatened_by_pawn = board.AttacksBy<PAWN>(them);
  const Bitboard threatened_by_minor = threatened_by_pawn |
    board.AttacksBy<KNIGHT>(them) |
    board.AttacksBy<BISHOP>(them);
  const Bitboard threatened_by_rook =
    threatened_by_minor | board.AttacksBy<ROOK>(them);
  const Bitboard threatened_pieces =
    board.color(us) &
    (threatened_by_pawn & (board.pieces(KNIGHT) | board.pieces(BISHOP)) |
      threatened_by_minor & board.pieces(ROOK) |
      threatened_by_rook & board.pieces(QUEEN));

  for (auto& [move, score] : moves) {
    const Move m = move;
    int s = 0;
    if (board.IsCapture(m)) {
      const Score see = board.see(m);
      s += see >= 0 ? 1000000 : -1000000;
      const Piece moved = board.PieceOn(move::from(m));
      const Square to =
        move::move_type(m) == move::EN_PASSANT
          ? move::to(m) -
          static_cast<Square>(direction::PawnPush(board.side_to_move))
          : move::to(m);
      const PieceType captured = piece_type::make(board.PieceOn(to));
      s += 89 * eval::kPieceValues[board.PieceOn(move::to(m))] +
        histories.capture[moved][to][captured];
    }
    else {
      if (m == histories.killer[ss->ply][0])
        s += 500004;
      else if (m == histories.killer[ss->ply][1])
        s += 500003;
      else if (ss->ply >= 2 && m == histories.killer[ss->ply - 2][0])
        s += 500002;
      else {
        if (m == histories.counter[(ss - 1)->moved][move::to((ss - 1)->move)])
          s += 500001;
        else {
          const Square from = move::from(m);
          const Square to = move::to(m);
          s += histories.butterfly[board.side_to_move][from][to] / 155 +
            histories.continuation[(ss - 1)->moved][move::to((ss - 1)->move)]
            [board.PieceOn(from)][to] /
            52 +
            histories.continuation[(ss - 2)->moved][move::to((ss - 2)->move)]
            [board.PieceOn(from)][to] /
            61 +
            histories.continuation[(ss - 4)->moved][move::to((ss - 4)->move)]
            [board.PieceOn(from)][to] /
            64;
          if (threatened_pieces.IsSet(from)) {
            if (const PieceType pt = piece_type::make(board.PieceOn(from));
              pt == KNIGHT && !threatened_by_pawn.IsSet(to))
              s += 561;
            else if (pt == BISHOP && !threatened_by_pawn.IsSet(to))
              s += 561;
            else if (pt == ROOK && !threatened_by_minor.IsSet(to))
              s += 561;
            else if (pt == QUEEN && !threatened_by_rook.IsSet(to))
              s += 561;
          }
        }
      }
    }
    score = s;
  }
}
