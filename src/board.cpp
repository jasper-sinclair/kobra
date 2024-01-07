#include "board.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

#include "attacks.h"
#include "eval.h"

Board::Board(const std::string& fen) {
  std::memset(this, 0, sizeof(Board));
  history = {};
  history.emplace_back();
  st = GetBoardStatus();

  std::istringstream ss(fen);
  std::string token;

  ss >> token;
  Square sq = A8;
  Piece pc;
  for (char c : token) {
    if (c == ' ') continue;

    if ((pc = piece::kPieceToChar.find(c)) != std::string::npos) {
      SetPiece<false>(pc, sq);
      sq += EAST;
    }

    else if (isdigit(c))
      sq += (c - '0') * EAST;

    else if (c == '/') {
      sq += 2 * SOUTH;
    }
  }

  ss >> token;
  side_to_move = (token == "w") ? WHITE : BLACK;

  ss >> token;
  for (char c : token) {
    switch (c) {
      case 'K':
        st->castlings.set(WHITE_KING_SIDE);
        break;
      case 'Q':
        st->castlings.set(WHITE_QUEEN_SIDE);
        break;
      case 'k':
        st->castlings.set(BLACK_KING_SIDE);
        break;
      case 'q':
        st->castlings.set(BLACK_QUEEN_SIDE);
        break;
    }
  }

  ss >> token;

  st->ep_square = (token == "-") ? NO_SQUARE : square::make(token);

  ss >> st->fifty_move_count;

  ss >> st->ply_count;
  st->ply_count = 2 * (st->ply_count - 1) + side_to_move;

  st->zobrist = 42;
}

Board::Board(const Board& other) { *this = other; }

Board& Board::operator=(const Board& other) {
  std::memcpy(board, other.board, N_SQUARES * sizeof(Piece));
  std::memcpy(piece_bb, other.piece_bb, N_PIECE_TYPES * sizeof(Bitboard));
  std::memcpy(color_bb, other.color_bb, N_COLORS * sizeof(Bitboard));
  occupied_bb = other.occupied_bb;
  side_to_move = other.side_to_move;
  history = other.history;
  st = GetBoardStatus();

  return *this;
}

std::ostream& operator<<(std::ostream& os, Board& board) {
  const std::string hor = "+---+---+---+---+---+---+---+---+";
  const std::string ver = "|";

  for (Rank r = RANK_8; r >= RANK_1; --r) {
    os << hor;
    os << '\n';

    for (File f = FILE_A; f <= FILE_H; ++f) {
      Piece pc = board.PieceOn(square::make(f, r));
      char c = piece::kPieceToChar[pc];
      std::string str = ver + " " + c + " ";
      os << str;
    }
    os << ver;
    os << '\n';
  }
  os << hor;
  os << '\n';
  return os;
}

std::string Board::fen() {
  std::stringstream ss;

  for (Rank r = RANK_8; r >= RANK_1; --r) {
    int empty_count = 0;
    for (File f = FILE_A; f <= FILE_H; ++f) {
      Piece pc = PieceOn(square::make(f, r));
      if (pc) {
        if (empty_count) ss << empty_count;
        ss << piece::kPieceToChar[pc];
        empty_count = 0;
      } else
        ++empty_count;
    }
    if (empty_count) ss << empty_count;
    if (r != RANK_1) ss << '/';
  }

  ss << ' ' << ((side_to_move == WHITE) ? 'w' : 'b');

  ss << ' ';
  if (CanCastle(WHITE_KING_SIDE)) ss << 'K';
  if (CanCastle(WHITE_QUEEN_SIDE)) ss << 'Q';
  if (CanCastle(BLACK_KING_SIDE)) ss << 'k';
  if (CanCastle(BLACK_QUEEN_SIDE)) ss << 'q';
  if (NoCastling()) ss << '-';

  ss << ' '
     << ((st->ep_square == NO_SQUARE) ? "-" : square::ToString(st->ep_square));
  ss << ' ' << st->fifty_move_count;
  ss << ' ' << 1 + (st->ply_count - side_to_move) / 2;

  return ss.str();
}

template <bool UpdateZobrist>
void Board::SetPiece(Piece pc, Square sq) {
  board[sq] = pc;
  piece_bb[piece_type::make(pc)].set(sq);
  color_bb[color::make(pc)].set(sq);
  occupied_bb.set(sq);

  if (UpdateZobrist) st->zobrist ^= zobrist::psq[pc][sq];
}

template <bool UpdateZobrist>
void Board::RemovePiece(Square sq) {
  if (UpdateZobrist) st->zobrist ^= zobrist::psq[PieceOn(sq)][sq];

  piece_bb[piece_type::make(PieceOn(sq))].clear(sq);
  color_bb[color::make(PieceOn(sq))].clear(sq);
  board[sq] = NO_PIECE;
  occupied_bb.clear(sq);
}

template <bool UpdateZobrist>
void Board::MovePiece(Square from, Square to) {
  SetPiece<UpdateZobrist>(PieceOn(from), to);
  RemovePiece<UpdateZobrist>(from);
}

void Board::ApplyMove(uint16_t m) {
  Square from = move::from(m);
  Square to = move::to(m);
  move::MoveType moveType = move::moveType(m);
  Piece pc = PieceOn(from);
  int32_t pt = piece_type::make(pc);
  Color us = side_to_move;
  Color them = !us;
  Direction push = direction::PawnPush(us);

  BoardStatus bs;
  bs.ply_count = st->ply_count + 1;
  bs.fifty_move_count = st->fifty_move_count + 1;
  bs.castlings = st->castlings;
  bs.zobrist = st->zobrist;

  bs.ep_square = NO_SQUARE;
  bs.captured =
      moveType == move::EN_PASSANT ? piece::make(them, PAWN) : PieceOn(to);
  bs.move = m;
  bs.king_attack_info.computed = false;

  // reset ep square
  if (st->ep_square) {
    st->zobrist ^= zobrist::en_passant[file::make(st->ep_square)];
  }

  // capture
  if (bs.captured) {
    bs.fifty_move_count = 0;
    Square capsq = to;

    if (moveType == move::EN_PASSANT) capsq -= push;

    RemovePiece(capsq);

    if (bs.captured == piece::make(them, ROOK)) {
      if (to == square::relative(us, A8)) {
        st->zobrist ^= zobrist::castling[bs.castlings.data];
        bs.castlings.reset(us ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE);
        st->zobrist ^= zobrist::castling[bs.castlings.data];
      } else if (to == square::relative(us, H8)) {
        st->zobrist ^= zobrist::castling[bs.castlings.data];
        bs.castlings.reset(us ? WHITE_KING_SIDE : BLACK_KING_SIDE);
        st->zobrist ^= zobrist::castling[bs.castlings.data];
      }
    }
  }

  // pawn move
  if (pt == PAWN) {
    bs.fifty_move_count = 0;

    if (to - from == 2 * push) {
      bs.ep_square = to - push;
      st->zobrist ^= zobrist::en_passant[file::make(bs.ep_square)];
    }
  }

  // king move
  else if (pt == KING) {
    st->zobrist ^= zobrist::castling[bs.castlings.data];
    bs.castlings.reset(us ? BLACK_CASTLING : WHITE_CASTLING);
    st->zobrist ^= zobrist::castling[bs.castlings.data];

    // castling
    if (moveType == move::CASTLING) {
      // move Rook
      if (to == square::relative(us, C1)) {
        Square rfrom = square::relative(us, A1);
        Square rto = square::relative(us, D1);
        MovePiece(rfrom, rto);
      } else if (to == square::relative(us, G1)) {
        Square rfrom = square::relative(us, H1);
        Square rto = square::relative(us, F1);
        MovePiece(rfrom, rto);
      }
    }
  }

  // rook move
  else if (pt == ROOK) {
    if (from == square::relative(us, A1)) {
      st->zobrist ^= zobrist::castling[bs.castlings.data];
      bs.castlings.reset(us ? BLACK_QUEEN_SIDE : WHITE_QUEEN_SIDE);
      st->zobrist ^= zobrist::castling[bs.castlings.data];
    } else if (from == square::relative(us, H1)) {
      st->zobrist ^= zobrist::castling[bs.castlings.data];
      bs.castlings.reset(us ? BLACK_KING_SIDE : WHITE_KING_SIDE);
      st->zobrist ^= zobrist::castling[bs.castlings.data];
    }
  }

  // promotion
  if (moveType == move::PROMOTION) {
    RemovePiece(from);
    pc = piece::make(us, move::PieceType(m));
    SetPiece(pc, to);
  } else {
    MovePiece(from, to);
  }

  st->zobrist ^= zobrist::side;

  side_to_move = !side_to_move;

  // compute repetitions
  bs.repetitions = 0;
  for (int i = (int)history.size() - 4;
       i > (int)history.size() - bs.fifty_move_count - 1; i -= 2) {
    if (i <= 0) break;
    if (history[i].zobrist == st->zobrist) {
      bs.repetitions = history[i].repetitions + 1;
      break;
    }
  }

  std::swap(st->zobrist, bs.zobrist);
  history.push_back(bs);
  st = GetBoardStatus();
}

void Board::UndoMove() {
  side_to_move = !side_to_move;

  Square from = move::from(st->move);
  Square to = move::to(st->move);
  move::MoveType moveType = move::moveType(st->move);
  Piece pc = PieceOn(to);
  Color us = side_to_move;
  Color them = !us;
  Direction push = direction::PawnPush(us);

  if (moveType == move::EN_PASSANT) {
    MovePiece<false>(to, from);
    to -= push;
  }

  else if (moveType == move::PROMOTION) {
    RemovePiece<false>(to);
    SetPiece<false>(piece::make(us, PAWN), from);
  }

  else if (moveType == move::CASTLING) {
    bool isKingSide = to > from;
    Square rto = square::relative(us, isKingSide ? F1 : D1);
    Square rfrom = square::relative(us, isKingSide ? H1 : A1);
    MovePiece<false>(rto, rfrom);
    MovePiece<false>(to, from);
  }

  else
    MovePiece<false>(to, from);

  if (st->captured) SetPiece<false>(st->captured, to);

  history.pop_back();
  st = GetBoardStatus();
}

void Board::ApplyNullMove() {
  BoardStatus bs;
  bs.ply_count = st->ply_count;
  bs.fifty_move_count = st->fifty_move_count;
  bs.castlings = st->castlings;
  bs.repetitions = 0;
  bs.ep_square = NO_SQUARE;
  bs.captured = NO_PIECE;
  bs.move = 0;
  bs.king_attack_info.computed = false;

  history.push_back(bs);
  st = GetBoardStatus();
  side_to_move = !side_to_move;
}

void Board::UndoNullMove() {
  side_to_move = !side_to_move;
  history.pop_back();
  st = GetBoardStatus();
}

bool Board::GivesCheck(uint16_t m) {
  Bitboard their_king_bb = pieces(!side_to_move, KING);
  Square ksq = their_king_bb.LSB();
  Square from = move::from(m);
  Square to = move::to(m);
  move::MoveType moveType = move::moveType(m);
  Piece pt = piece_type::make(PieceOn(from));
  Bitboard occ = occupied_bb;

  // promotion
  if (moveType == move::PROMOTION) {
    occ.clear(from);
    pt = move::PieceType(m);
  }

  Bitboard attacked;
  switch (pt) {
    case PAWN: {
      attacked = attacks::pawn_attacks[side_to_move][to];
      break;
    }
    case KNIGHT: {
      attacked = attacks::knight_attacks[to];
      break;
    }
    case BISHOP: {
      attacked = attacks::attacks<BISHOP>(to, occ);
      break;
    }
    case ROOK: {
      attacked = attacks::attacks<ROOK>(to, occ);
      break;
    }
    case QUEEN: {
      attacked = attacks::attacks<QUEEN>(to, occ);
      break;
    }
    default:
      attacked = {};
  }
  if (attacked & their_king_bb) return true;

  // castling
  if (moveType == move::CASTLING) {
    occ.clear(from);
    Square rsq = side_to_move == WHITE ? to > from ? F1 : D1
                 : to > from           ? F8
                                       : D8;
    if (attacks::attacks<ROOK>(rsq, occ) & their_king_bb) return true;
  }

  // en passant
  else if (moveType == move::EN_PASSANT) {
    occ.clear(from);
    occ.clear(to - direction::PawnPush(side_to_move));
    occ.set(to);
    if (IsUnderAttack(!side_to_move, ksq)) return true;
  }

  // discovery
  else {
    occ.clear(from);
    occ.set(to);

    if (IsUnderAttack(!side_to_move, ksq)) return true;
  }

  return false;
}

bool Board::IsPseudoLegal(uint16_t m) {
  if (!m) return false;

  Color us = side_to_move;
  Color them = !side_to_move;
  Square from = move::from(m);
  Square to = move::to(m);
  move::MoveType moveType = move::moveType(m);
  Piece pc = PieceOn(from);
  int32_t pt = piece_type::make(pc);

  if (pc == NO_PIECE || color::make(pc) == them ||
      pt != PAWN &&
          (moveType == move::PROMOTION || moveType == move::EN_PASSANT) ||
      pt != KING && moveType == move::CASTLING)
    return false;

  if (pt == KNIGHT) {
    if (color(us).IsSet(to) || !attacks::knight_attacks[from].IsSet(to))
      return false;
  }

  else if (pt == ROOK) {
    if (color(us).IsSet(to) || !attacks::rook_attacks[from].IsSet(to) ||
        attacks::in_between_squares[from][to] & occupied_bb)
      return false;
  }

  else if (pt == BISHOP) {
    if (color(us).IsSet(to) || !attacks::bishop_attacks[from].IsSet(to) ||
        attacks::in_between_squares[from][to] & occupied_bb)
      return false;
  }

  else if (pt == QUEEN) {
    if (color(us).IsSet(to) ||
        !(attacks::rook_attacks[from].IsSet(to) ||
          attacks::bishop_attacks[from].IsSet(to)) ||
        attacks::in_between_squares[from][to] & occupied_bb)
      return false;
  }

  else if (pt == PAWN) {
    int pawn_push = direction::PawnPush(us);

    if (rank::make(from) == (us == WHITE ? RANK_7 : RANK_2) &&
        moveType != move::PROMOTION)
      return false;

    if (rank::make(from) == (us == WHITE ? RANK_2 : RANK_7) &&
        to - from == 2 * pawn_push && !occupied_bb.IsSet(from + pawn_push) &&
        !occupied_bb.IsSet(to))
      return true;

    else if (to - from == pawn_push) {
      if (!occupied_bb.IsSet(to)) return true;
    }

    else if (moveType == move::EN_PASSANT) {
      if (st->ep_square == to &&
          PieceOn(to - pawn_push) == piece::make(them, PAWN) &&
          !occupied_bb.IsSet(to) && attacks::pawn_attacks[us][from].IsSet(to))
        return true;
    }

    else if (square::distance(from, to) == 1 && color(them).IsSet(to)) {
      if (attacks::pawn_attacks[us][from].IsSet(to)) return true;
    }

    return false;
  }

  else if (pt == KING) {
    if (moveType == move::CASTLING) {
      Color c = rank::make(to) == RANK_1 ? WHITE : BLACK;
      bool king_side = to > from;

      if (c != us ||
          !CanCastle(c == WHITE ? king_side ? WHITE_KING_SIDE : WHITE_QUEEN_SIDE
                     : king_side ? BLACK_KING_SIDE
                                 : BLACK_QUEEN_SIDE) ||
          occupied_bb &
              (c == WHITE ? king_side ? kWhiteKingSidePath : kWhiteQueenSidePath
               : king_side ? kBlackKingSidePath
                           : kBlackQueenSidePath))
        return false;
    } else {
      if (color(us).IsSet(to) || square::distance(from, to) != 1) return false;
    }
  }

  return true;
}

void Board::GenerateKingAttackInfo(KingAttackInfo& k) {
  k.computed = true;

  Color them = !side_to_move;
  Square ksq = this->ksq(side_to_move);
  Bitboard ourTeam = color(side_to_move);
  Bitboard theirTeam = color(them);

  k.pinned = {};
  k.attacks =
      theirTeam & (attacks::pawn_attacks[side_to_move][ksq] & pieces(PAWN) |
                   attacks::knight_attacks[ksq] & pieces(KNIGHT));

  int attacker_count = k.attacks.popcount();

  Bitboard slider_attackers =
      theirTeam &
      (attacks::attacks<BISHOP>(ksq, theirTeam) &
           (pieces(BISHOP) | pieces(QUEEN)) |
       attacks::attacks<ROOK>(ksq, theirTeam) & (pieces(ROOK) | pieces(QUEEN)));

  while (slider_attackers) {
    Square s = slider_attackers.PopLsb();
    Bitboard between = attacks::in_between_squares[ksq][s];
    Bitboard blockers = between & ourTeam;

    auto numBlockers = blockers.popcount();
    if (numBlockers == 0) {
      ++attacker_count;
      k.attacks |= between;
      k.attacks |= Bitboard::FromSquare(s);
    } else if (numBlockers == 1)
      k.pinned.set(blockers.LSB());
  }

  k.double_check = attacker_count == 2;
}

bool Board::IsLegal(uint16_t m) {
  if (!st->king_attack_info.computed)
    GenerateKingAttackInfo(st->king_attack_info);

  Square from = move::from(m);
  Square to = move::to(m);
  move::MoveType moveType = move::moveType(m);
  Piece pc = PieceOn(from);
  Color us = side_to_move;
  Color them = !us;

  if (pc == piece::make(us, KING)) {
    if (moveType == move::CASTLING) {
      if (st->king_attack_info.check() ||
          to == square::relative(us, C1) &&
              (IsUnderAttack(us, square::relative(us, D1)) ||
               IsUnderAttack(us, square::relative(us, C1))) ||
          to == square::relative(us, G1) &&
              (IsUnderAttack(us, square::relative(us, F1)) ||
               IsUnderAttack(us, square::relative(us, G1))))
        return false;
    } else {
      Bitboard occ_copy = occupied_bb;
      int32_t captured = piece_type::make(PieceOn(to));

      occupied_bb.clear(from);
      occupied_bb.set(to);

      bool illegal;
      if (captured) {
        piece_bb[captured].clear(to);
        illegal = IsUnderAttack(us, to);
        piece_bb[captured].set(to);
      } else
        illegal = IsUnderAttack(us, to);

      occupied_bb = occ_copy;
      if (illegal) return false;
    }
  }

  else if (st->king_attack_info.check() &&
               (!st->king_attack_info.attacks.IsSet(to) ||
                st->king_attack_info.pinned.IsSet(from)) ||
           st->king_attack_info.double_check)
    return false;

  else {
    // pins when not in check
    if (st->king_attack_info.pinned.IsSet(from)) {
      Square sq = ksq(us);
      int dx_from = file::make(from) - file::make(sq);
      int dy_from = rank::make(from) - rank::make(sq);
      int dx_to = file::make(to) - file::make(sq);
      int dy_to = rank::make(to) - rank::make(sq);

      // north, south
      if (dx_from == 0 || dx_to == 0) {
        if (dx_from != dx_to) return false;
      }
      // east, west
      else if (dy_from == 0 || dy_to == 0) {
        if (dy_from != dy_to) return false;
      }
      // northeast, southeast, southwest, northwest
      else if (dx_from * dy_to != dy_from * dx_to)
        return false;
    }

    // en passant
    if (moveType == move::EN_PASSANT) {
      ApplyMove(m);
      bool illegal = IsUnderAttack(us, ksq(us));
      UndoMove();
      if (illegal) return false;
    }
  }
  return true;
}

bool Board::IsUnderAttack(Color us, Square sq) {
  Color them = !us;
  return attacks::attacks<ROOK>(sq, occupied_bb) &
             (pieces(them, ROOK) | pieces(them, QUEEN)) ||
         attacks::attacks<BISHOP>(sq, occupied_bb) &
             (pieces(them, BISHOP) | pieces(them, QUEEN)) ||
         attacks::knight_attacks[sq] & pieces(them, KNIGHT) ||
         attacks::king_attacks[sq] & pieces(them, KING) ||
         attacks::pawn_attacks[us][sq] & pieces(them, PAWN);
}

bool Board::IsInCheck() {
  return IsUnderAttack(side_to_move, ksq(side_to_move));
}
