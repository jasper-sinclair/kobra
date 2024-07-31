#include "movegen.h"

#include <algorithm>

#include "attack.h"

template <Color C>
void GenPawnMoves(Board& board, MoveList& move_list) {
  constexpr Color us = C;
  constexpr Color them = !C;
  constexpr Direction up = C == WHITE ? NORTH : SOUTH;
  constexpr Direction up_right = C == WHITE ? NORTHEAST : SOUTHEAST;
  constexpr Direction up_left = C == WHITE ? NORTHWEST : SOUTHWEST;
  constexpr Bitboard relative_rank_8_bb = C == WHITE ? kRank8Bb : kRank1Bb;
  constexpr Bitboard relative_rank_4_bb = C == WHITE ? kRank4Bb : kRank5Bb;

  Bitboard empty = ~board.occupied();
  const Bitboard their_team = board.color(them);
  const Bitboard our_pawns = board.pieces(us, PAWN);
  Square to;
  Square from;

  const Bitboard single_pawn_push_targets = our_pawns.shift<up>() & empty;
  const Bitboard up_right_bb = our_pawns.shift<up_right>();
  const Bitboard up_left_bb = our_pawns.shift<up_left>();
  const Bitboard up_right_captures = up_right_bb & their_team;
  const Bitboard up_left_captures = up_left_bb & their_team;
  Bitboard attacks = single_pawn_push_targets - relative_rank_8_bb;

  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up);
    move_list.add(move::make(from, to));
  }

  attacks = single_pawn_push_targets & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up);
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }

  attacks = single_pawn_push_targets.shift<up>() & empty & relative_rank_4_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(2 * up);
    move_list.add(move::make(from, to));
  }

  attacks = up_right_captures - relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up_right);
    move_list.add(move::make(from, to));
  }

  attacks = up_left_captures - relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up_left);
    move_list.add(move::make(from, to));
  }

  attacks = up_right_captures & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up_right);
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }

  attacks = up_left_captures & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - static_cast<Square>(up_left);
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }

  if (board.st->ep_square != NO_SQUARE) {
    attacks = up_right_bb & Bitboard::FromSquare(board.st->ep_square);
    if (attacks) {
      to = board.st->ep_square;
      from = to - static_cast<Square>(up_right);
      move_list.add(make(from, to, move::EN_PASSANT));
    }

    attacks = up_left_bb & Bitboard::FromSquare(board.st->ep_square);
    if (attacks) {
      to = board.st->ep_square;
      from = to - static_cast<Square>(up_left);
      move_list.add(make(from, to, move::EN_PASSANT));
    }
  }
}

template <Color C, PieceType Pt>
void GenPieceMoves(Board& board, MoveList& move_list) {
  Bitboard friendly = board.color(C);
  Bitboard pieces = board.pieces(C, Pt);
  while (pieces) {
    Square from = pieces.PopLsb();
    Bitboard attacks =
      (Pt == KNIGHT
      ? attack::knight_attacks[from]
      : attack::attacks<Pt>(from, board.occupied())) -
      friendly;
    while (attacks) move_list.add(move::make(from, attacks.PopLsb()));
  }
}

template <Color C>
void GenKingMoves(Board& board, MoveList& move_list) {
  const Square ksq = board.ksq(C);
  Bitboard attacks = attack::king_attacks[ksq] - board.color(C);
  while (attacks) move_list.add(move::make(ksq, attacks.PopLsb()));
  if (ksq == square::relative(C, E1)) {
    const Bitboard empty = ~board.occupied();
    constexpr Bitboard path1 =
      C == WHITE ? kWhiteQueenSidePath : kBlackQueenSidePath;
    constexpr Bitboard path2 =
      C == WHITE ? kWhiteKingSidePath : kBlackKingSidePath;
    if (board.CanCastle(C == WHITE ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE) &&
      (empty & path1) == path1)
      move_list.add(make(ksq, square::relative(C, C1), move::CASTLING));
    if (board.CanCastle(C == WHITE ? WHITE_KING_SIDE : BLACK_KING_SIDE) &&
      (empty & path2) == path2)
      move_list.add(make(ksq, square::relative(C, G1), move::CASTLING));
  }
}

void GenMoves(Board& board, MoveList& move_list) {
  if (!board.st->king_attack_info.computed)
    board.GenKingAttackInfo(board.st->king_attack_info);

  move_list.last = move_list.data;
  if (board.side_to_move == WHITE) {
    if (!board.st->king_attack_info.double_check) {
      GenPawnMoves<WHITE>(board, move_list);
      GenPieceMoves<WHITE, KNIGHT>(board, move_list);
      GenPieceMoves<WHITE, BISHOP>(board, move_list);
      GenPieceMoves<WHITE, ROOK>(board, move_list);
      GenPieceMoves<WHITE, QUEEN>(board, move_list);
    }
    GenKingMoves<WHITE>(board, move_list);
  }
  else {
    if (!board.st->king_attack_info.double_check) {
      GenPawnMoves<BLACK>(board, move_list);
      GenPieceMoves<BLACK, KNIGHT>(board, move_list);
      GenPieceMoves<BLACK, BISHOP>(board, move_list);
      GenPieceMoves<BLACK, ROOK>(board, move_list);
      GenPieceMoves<BLACK, QUEEN>(board, move_list);
    }
    GenKingMoves<BLACK>(board, move_list);
  }
  move_list.last = std::ranges::remove_if(move_list, [&](const MoveData& m) {
    return !board.IsLegal(m.move);
    }).begin();
}
