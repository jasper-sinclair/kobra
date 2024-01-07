#include "movegen.h"

#include "attacks.h"

template <Color C>
void GeneratePawnMoves(Board& board, MoveList& move_list) {
  constexpr Color us = C;
  constexpr Color them = !C;

  constexpr Direction up = C == WHITE ? NORTH : SOUTH;
  constexpr Direction up_right = C == WHITE ? NORTHEAST : SOUTHEAST;
  constexpr Direction up_left = C == WHITE ? NORTHWEST : SOUTHWEST;

  Bitboard relative_rank_8_bb = C == WHITE ? kRank8Bb : kRank1Bb;
  Bitboard relative_rank_4_bb = C == WHITE ? kRank4Bb : kRank5Bb;

  Bitboard empty = ~board.occupied();
  Bitboard their_team = board.color(them);
  Bitboard our_pawns = board.pieces(us, PAWN);

  Bitboard attacks;
  Square to;
  Square from;

  Bitboard single_pawn_push_targets = our_pawns.shift<up>() & empty;
  Bitboard up_right_bb = our_pawns.shift<up_right>();
  Bitboard up_left_bb = our_pawns.shift<up_left>();
  Bitboard up_right_captures = up_right_bb & their_team;
  Bitboard up_left_captures = up_left_bb & their_team;

  // single pawn pushes
  attacks = single_pawn_push_targets - relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - up;
    move_list.add(move::make(from, to));
  }
  // pawn push promotions
  attacks = single_pawn_push_targets & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - up;
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }
  // double pawn pushes
  attacks = single_pawn_push_targets.shift<up>() & empty & relative_rank_4_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - 2 * up;
    move_list.add(move::make(from, to));
  }
  // pawn captures
  attacks = up_right_captures - relative_rank_8_bb;

  while (attacks) {
    to = attacks.PopLsb();
    from = to - up_right;
    move_list.add(move::make(from, to));
  }
  attacks = up_left_captures - relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - up_left;
    move_list.add(move::make(from, to));
  }
  // pawn capture promotions
  attacks = up_right_captures & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - up_right;
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }
  attacks = up_left_captures & relative_rank_8_bb;
  while (attacks) {
    to = attacks.PopLsb();
    from = to - up_left;
    move_list.add(move::make<KNIGHT>(from, to));
    move_list.add(move::make<BISHOP>(from, to));
    move_list.add(move::make<ROOK>(from, to));
    move_list.add(move::make<QUEEN>(from, to));
  }
  // en passant
  if (board.st->ep_square != NO_SQUARE) {
    attacks = up_right_bb & Bitboard::FromSquare(board.st->ep_square);
    if (attacks) {
      to = board.st->ep_square;
      from = to - up_right;
      move_list.add(move::make(from, to, move::EN_PASSANT));
    }
    attacks = up_left_bb & Bitboard::FromSquare(board.st->ep_square);
    if (attacks) {
      to = board.st->ep_square;
      from = to - up_left;
      move_list.add(move::make(from, to, move::EN_PASSANT));
    }
  }
}

template <Color C, int32_t Pt>
void GeneratePieceMoves(Board& board, MoveList& move_list) {
  Bitboard friendly = board.color(C);
  Bitboard pieces = board.pieces(C, Pt);

  while (pieces) {
    Square from = pieces.PopLsb();

    Bitboard attacks =
        (Pt == KNIGHT ? attacks::knight_attacks[from]
                      : attacks::attacks<Pt>(from, board.occupied())) -
        friendly;

    while (attacks) move_list.add(move::make(from, attacks.PopLsb()));
  }
}

template <Color C>
void GenerateKingMoves(Board& board, MoveList& move_list) {
  Square ksq = board.ksq(C);
  Bitboard attacks = attacks::king_attacks[ksq] - board.color(C);
  while (attacks) move_list.add(move::make(ksq, attacks.PopLsb()));

  // castling
  if (ksq == square::relative(C, E1)) {
    Bitboard empty = ~board.occupied();
    Bitboard path1 = C == WHITE ? kWhiteQueenSidePath : kBlackQueenSidePath;
    Bitboard path2 = C == WHITE ? kWhiteKingSidePath : kBlackKingSidePath;

    if (board.CanCastle(C == WHITE ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE) &&
        (empty & path1) == path1)
      move_list.add(move::make(ksq, square::relative(C, C1), move::CASTLING));
    if (board.CanCastle(C == WHITE ? WHITE_KING_SIDE : BLACK_KING_SIDE) &&
        (empty & path2) == path2)
      move_list.add(move::make(ksq, square::relative(C, G1), move::CASTLING));
  }
}

void GenerateMoves(Board& board, MoveList& move_list) {
  if (!board.st->king_attack_info.computed)
    board.GenerateKingAttackInfo(board.st->king_attack_info);

  move_list.last = move_list.data;

  if (board.side_to_move == WHITE) {
    if (!board.st->king_attack_info.double_check) {
      GeneratePawnMoves<WHITE>(board, move_list);
      GeneratePieceMoves<WHITE, KNIGHT>(board, move_list);
      GeneratePieceMoves<WHITE, BISHOP>(board, move_list);
      GeneratePieceMoves<WHITE, ROOK>(board, move_list);
      GeneratePieceMoves<WHITE, QUEEN>(board, move_list);
    }
    GenerateKingMoves<WHITE>(board, move_list);
  }

  else {
    if (!board.st->king_attack_info.double_check) {
      GeneratePawnMoves<BLACK>(board, move_list);
      GeneratePieceMoves<BLACK, KNIGHT>(board, move_list);
      GeneratePieceMoves<BLACK, BISHOP>(board, move_list);
      GeneratePieceMoves<BLACK, ROOK>(board, move_list);
      GeneratePieceMoves<BLACK, QUEEN>(board, move_list);
    }
    GenerateKingMoves<BLACK>(board, move_list);
  }

  move_list.last =
      std::remove_if(move_list.begin(), move_list.end(),
                     [&](Move& m) { return !board.IsLegal(m.move); });
}