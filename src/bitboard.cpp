#include "bitboard.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>

#include "attack.h"
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

  for (const char c : token) {
    if (c == ' ') continue;
    if ((pc = static_cast<Piece>(piece::piece_to_char.find(c))) !=
      static_cast<int>(std::string::npos)) {
      SetPiece<false>(pc, sq);
      sq += EAST;
    }
    else if (isdigit(c))
      sq += (c - '0') * EAST;
    else if (c == '/') {
      sq += static_cast<Square>(2 * SOUTH);
    }
  }

  ss >> token;
  side_to_move = token == "w" ? WHITE : BLACK;
  ss >> token;

  for (const char c : token) {
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
    default: ;
    }
  }

  ss >> token;
  st->ep_square = token == "-" ? NO_SQUARE : square::make(token);
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

std::ostream& operator<<(std::ostream& os, const Board& board) {
  const std::string sp = " ";
  for (Rank r = RANK_8; r >= RANK_1; --r) {
    os << '\n';
    for (File f = FILE_A; f <= FILE_H; ++f) {
      const Piece pc = board.PieceOn(square::make(f, r));
      char c = piece::piece_to_char[pc];
      if (c == ' ') c = '.';
      std::string str = c + sp;
      os << str;
    }
    os << sp;
  }
  os << '\n';
  return os;
}

std::string Board::fen() const {
  std::stringstream ss;

  for (Rank r = RANK_8; r >= RANK_1; --r) {
    int empty_count = 0;
    for (File f = FILE_A; f <= FILE_H; ++f) {
      if (const Piece pc = PieceOn(square::make(f, r))) {
        if (empty_count) ss << empty_count;
        ss << piece::piece_to_char[pc];
        empty_count = 0;
      }
      else
        ++empty_count;
    }
    if (empty_count) ss << empty_count;
    if (r != RANK_1) ss << '/';
  }

  ss << ' ' << (side_to_move == WHITE ? 'w' : 'b');
  ss << ' ';
  if (CanCastle(WHITE_KING_SIDE)) ss << 'K';
  if (CanCastle(WHITE_QUEEN_SIDE)) ss << 'Q';
  if (CanCastle(BLACK_KING_SIDE)) ss << 'k';
  if (CanCastle(BLACK_QUEEN_SIDE)) ss << 'q';
  if (CannotCastle()) ss << '-';
  ss << ' '
    << (st->ep_square == NO_SQUARE ? "-" : square::ToString(st->ep_square));
  ss << ' ' << st->fifty_move_count;
  ss << ' ' << 1 + (st->ply_count - side_to_move) / 2;
  return ss.str();
}

template <bool UpdateZobrist>
void Board::SetPiece(const Piece pc, const Square sq) {
  board[sq] = pc;
  piece_bb[piece_type::make(pc)].set(sq);
  color_bb[color::make(pc)].set(sq);
  occupied_bb.set(sq);
  if (UpdateZobrist) st->zobrist ^= zobrist::psq[pc][sq];
}

template <bool UpdateZobrist>
void Board::RemovePiece(const Square sq) {
  if (UpdateZobrist) st->zobrist ^= zobrist::psq[PieceOn(sq)][sq];
  piece_bb[piece_type::make(PieceOn(sq))].clear(sq);
  color_bb[color::make(PieceOn(sq))].clear(sq);
  board[sq] = NO_PIECE;
  occupied_bb.clear(sq);
}

template <bool UpdateZobrist>
void Board::MovePiece(const Square from, const Square to) {
  SetPiece<UpdateZobrist>(PieceOn(from), to);
  RemovePiece<UpdateZobrist>(from);
}

void Board::ApplyMove(const Move m) {
  const Square from = move::from(m);
  const Square to = move::to(m);
  const move::MoveType move_type = move::move_type(m);
  Piece pc = PieceOn(from);
  const PieceType pt = piece_type::make(pc);
  const Color us = side_to_move;
  const Color them = !us;
  const Direction push = direction::PawnPush(us);
  BoardStatus bs;
  bs.ply_count = st->ply_count + 1;
  bs.fifty_move_count = st->fifty_move_count + 1;
  bs.castlings = st->castlings;
  bs.zobrist = st->zobrist;
  bs.ep_square = NO_SQUARE;
  bs.captured =
    move_type == move::EN_PASSANT ? piece::make(them, PAWN) : PieceOn(to);
  bs.move = m;
  bs.king_attack_info.computed = false;

  if (st->ep_square) {
    st->zobrist ^= zobrist::en_passant[file::make(st->ep_square)];
  }

  if (bs.captured) {
    bs.fifty_move_count = 0;
    Square capsq = to;
    if (move_type == move::EN_PASSANT) capsq -= push;
    RemovePiece(capsq);

    if (bs.captured == piece::make(them, ROOK)) {
      if (to == square::relative(us, A8)) {
        st->zobrist ^= zobrist::castling[bs.castlings.data];
        bs.castlings.reset(us ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE);
        st->zobrist ^= zobrist::castling[bs.castlings.data];
      }
      else if (to == square::relative(us, H8)) {
        st->zobrist ^= zobrist::castling[bs.castlings.data];
        bs.castlings.reset(us ? WHITE_KING_SIDE : BLACK_KING_SIDE);
        st->zobrist ^= zobrist::castling[bs.castlings.data];
      }
    }
  }
  if (pt == PAWN) {
    bs.fifty_move_count = 0;
    if (to - from == 2 * push) {
      bs.ep_square = static_cast<Square>(to - push);
      st->zobrist ^= zobrist::en_passant[file::make(bs.ep_square)];
    }
  }
  else if (pt == KING) {
    st->zobrist ^= zobrist::castling[bs.castlings.data];
    bs.castlings.reset(us ? BLACK_CASTLING : WHITE_CASTLING);
    st->zobrist ^= zobrist::castling[bs.castlings.data];
    if (move_type == move::CASTLING) {
      if (to == square::relative(us, C1)) {
        const Square rfrom = square::relative(us, A1);
        const Square rto = square::relative(us, D1);
        MovePiece(rfrom, rto);
      }
      else if (to == square::relative(us, G1)) {
        const Square rfrom = square::relative(us, H1);
        const Square rto = square::relative(us, F1);
        MovePiece(rfrom, rto);
      }
    }
  }
  else if (pt == ROOK) {
    if (from == square::relative(us, A1)) {
      st->zobrist ^= zobrist::castling[bs.castlings.data];
      bs.castlings.reset(us ? BLACK_QUEEN_SIDE : WHITE_QUEEN_SIDE);
      st->zobrist ^= zobrist::castling[bs.castlings.data];
    }
    else if (from == square::relative(us, H1)) {
      st->zobrist ^= zobrist::castling[bs.castlings.data];
      bs.castlings.reset(us ? BLACK_KING_SIDE : WHITE_KING_SIDE);
      st->zobrist ^= zobrist::castling[bs.castlings.data];
    }
  }
  if (move_type == move::PROMOTION) {
    RemovePiece(from);
    pc = piece::make(us, move::GetPieceType(m));
    SetPiece(pc, to);
  }
  else {
    MovePiece(from, to);
  }

  st->zobrist ^= zobrist::side;
  side_to_move = !side_to_move;
  bs.repetitions = 0;

  for (int i = static_cast<int>(history.size()) - 4;
       i > static_cast<int>(history.size()) - bs.fifty_move_count - 1; i -= 2) {
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
  const Square from = move::from(st->move);
  Square to = move::to(st->move);
  const move::MoveType move_type = move::move_type(st->move);
  const Color us = side_to_move;
  const Direction push = direction::PawnPush(us);

  if (move_type == move::EN_PASSANT) {
    MovePiece<false>(to, from);
    to -= push;
  }
  else if (move_type == move::PROMOTION) {
    RemovePiece<false>(to);
    SetPiece<false>(piece::make(us, PAWN), from);
  }
  else if (move_type == move::CASTLING) {
    const bool is_king_side = to > from;
    const Square rto = square::relative(us, is_king_side ? F1 : D1);
    const Square rfrom = square::relative(us, is_king_side ? H1 : A1);
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

bool Board::GivesCheck(const Move m) const {
  Bitboard their_king_bb = pieces(!side_to_move, KING);
  Square ksq = their_king_bb.Lsb();
  const Square from = move::from(m);
  const Square to = move::to(m);
  const move::MoveType move_type = move::move_type(m);
  Piece pt = piece_type::make(PieceOn(from));
  Bitboard occ = occupied_bb;

  if (move_type == move::PROMOTION) {
    occ.clear(from);
    pt = move::GetPieceType(m);
  }

  Bitboard attacked;

  switch (pt) {
  case PAWN: {
    attacked = attack::pawn_attacks[side_to_move][to];
    break;
  }
  case KNIGHT: {
    attacked = attack::knight_attacks[to];
    break;
  }
  case BISHOP: {
    attacked = attack::attacks<BISHOP>(to, occ);
    break;
  }
  case ROOK: {
    attacked = attack::attacks<ROOK>(to, occ);
    break;
  }
  case QUEEN: {
    attacked = attack::attacks<QUEEN>(to, occ);
    break;
  }
  default:
    attacked = {};
  }

  if (attacked & their_king_bb) return true;

  if (move_type == move::CASTLING) {
    occ.clear(from);
    if (const Square rsq = side_to_move == WHITE
      ? to > from
      ? F1
      : D1
      : to > from
      ? F8
      : D8;
      attack::attacks<ROOK>(rsq, occ) & their_king_bb)
      return true;
  }
  else if (move_type == move::EN_PASSANT) {
    occ.clear(from);
    occ.clear(static_cast<Square>(to - direction::PawnPush(side_to_move)));
    occ.set(to);
    if (IsUnderAttack(!side_to_move, ksq)) return true;
  }
  else {
    occ.clear(from);
    occ.set(to);
    if (IsUnderAttack(!side_to_move, ksq)) return true;
  }
  return false;
}

bool Board::IsPseudoLegal(const Move m) const {
  if (!m) return false;
  const Color us = side_to_move;
  const Color them = !side_to_move;
  const Square from = move::from(m);
  const Square to = move::to(m);
  const move::MoveType move_type = move::move_type(m);
  const Piece pc = PieceOn(from);
  const PieceType pt = piece_type::make(pc);

  if (pc == NO_PIECE || color::make(pc) == them ||
    pt != PAWN &&
    (move_type == move::PROMOTION || move_type == move::EN_PASSANT) ||
    pt != KING && move_type == move::CASTLING)
    return false;

  if (pt == KNIGHT) {
    if (color(us).IsSet(to) || !attack::knight_attacks[from].IsSet(to))
      return false;
  }
  else if (pt == ROOK) {
    if (color(us).IsSet(to) || !attack::rook_attacks[from].IsSet(to) ||
      attack::in_between_squares[from][to] & occupied_bb)
      return false;
  }
  else if (pt == BISHOP) {
    if (color(us).IsSet(to) || !attack::bishop_attacks[from].IsSet(to) ||
      attack::in_between_squares[from][to] & occupied_bb)
      return false;
  }
  else if (pt == QUEEN) {
    if (color(us).IsSet(to) ||
      !(attack::rook_attacks[from].IsSet(to) ||
        attack::bishop_attacks[from].IsSet(to)) ||
      attack::in_between_squares[from][to] & occupied_bb)
      return false;
  }
  else if (pt == PAWN) {
    const int pawn_push = direction::PawnPush(us);
    if (rank::make(from) == (us == WHITE ? RANK_7 : RANK_2) &&
      move_type != move::PROMOTION)
      return false;
    if (rank::make(from) == (us == WHITE ? RANK_2 : RANK_7) &&
      to - from == 2 * pawn_push &&
      !occupied_bb.IsSet(static_cast<Square>(from + pawn_push)) &&
      !occupied_bb.IsSet(to))
      return true;
    if (to - from == pawn_push) {
      if (!occupied_bb.IsSet(to)) return true;
    }
    else if (move_type == move::EN_PASSANT) {
      if (st->ep_square == to &&
        PieceOn(static_cast<Square>(to - pawn_push)) ==
        piece::make(them, PAWN) &&
        !occupied_bb.IsSet(to) && attack::pawn_attacks[us][from].IsSet(to))
        return true;
    }
    else if (square::distance(from, to) == 1 && color(them).IsSet(to)) {
      if (attack::pawn_attacks[us][from].IsSet(to)) return true;
    }
    return false;
  }
  else if (pt == KING) {
    if (move_type == move::CASTLING) {
      const Color c = rank::make(to) == RANK_1 ? WHITE : BLACK;
      if (const bool king_side = to > from;
        c != us ||
        !CanCastle(c == WHITE
                     ? king_side
                     ? WHITE_KING_SIDE
                     : WHITE_QUEEN_SIDE
                     : king_side
                     ? BLACK_KING_SIDE
                     : BLACK_QUEEN_SIDE) ||
        occupied_bb &
        (c == WHITE
           ? king_side
           ? kWhiteKingSidePath
           : kWhiteQueenSidePath
           : king_side
           ? kBlackKingSidePath
           : kBlackQueenSidePath))
        return false;
    }
    else {
      if (color(us).IsSet(to) || square::distance(from, to) != 1) return false;
    }
  }
  return true;
}

void Board::GenKingAttackInfo(KingAttackInfo& k) const {
  k.computed = true;
  const Color them = !side_to_move;
  const Square ksq = this->ksq(side_to_move);
  const Bitboard our_team = color(side_to_move);
  const Bitboard their_team = color(them);
  k.pinned = {};

  k.attacks =
    their_team & (attack::pawn_attacks[side_to_move][ksq] & pieces(PAWN) |
      attack::knight_attacks[ksq] & pieces(KNIGHT));
  int attacker_count = k.attacks.popcount();

  Bitboard slider_attackers =
    their_team &
    (attack::attacks<BISHOP>(ksq, their_team) &
      (pieces(BISHOP) | pieces(QUEEN)) |
      attack::attacks<ROOK>(ksq, their_team) & (pieces(ROOK) | pieces(QUEEN)));

  while (slider_attackers) {
    const Square s = slider_attackers.PopLsb();
    const Bitboard between = attack::in_between_squares[ksq][s];
    Bitboard blockers = between & our_team;
    if (const auto num_blockers = blockers.popcount(); num_blockers == 0) {
      ++attacker_count;
      k.attacks |= between;
      k.attacks |= Bitboard::FromSquare(s);
    }
    else if (num_blockers == 1)
      k.pinned.set(blockers.Lsb());
  }

  k.double_check = attacker_count == 2;
}

bool Board::IsLegal(const Move m) {
  if (!st->king_attack_info.computed) GenKingAttackInfo(st->king_attack_info);
  const Square from = move::from(m);
  const Square to = move::to(m);
  const move::MoveType move_type = move::move_type(m);
  const Piece pc = PieceOn(from);

  if (const Color us = side_to_move; pc == piece::make(us, KING)) {
    if (move_type == move::CASTLING) {
      if (st->king_attack_info.check() ||
        to == square::relative(us, C1) &&
        (IsUnderAttack(us, square::relative(us, D1)) ||
          IsUnderAttack(us, square::relative(us, C1))) ||
        to == square::relative(us, G1) &&
        (IsUnderAttack(us, square::relative(us, F1)) ||
          IsUnderAttack(us, square::relative(us, G1))))
        return false;
    }
    else {
      const Bitboard occ_copy = occupied_bb;
      const PieceType captured = piece_type::make(PieceOn(to));
      occupied_bb.clear(from);
      occupied_bb.set(to);
      bool illegal;

      if (captured) {
        piece_bb[captured].clear(to);
        illegal = IsUnderAttack(us, to);
        piece_bb[captured].set(to);
      }
      else
        illegal = IsUnderAttack(us, to);
      occupied_bb = occ_copy;
      if (illegal) return false;
    }
  }
  else if (st->king_attack_info.check() &&
    (!st->king_attack_info.attacks.IsSet(
        move::move_type(m) == move::EN_PASSANT
          ? static_cast<Square>(to - direction::PawnPush(us))
          : to) ||
      st->king_attack_info.pinned.IsSet(from)) ||
    st->king_attack_info.double_check)
    return false;
  else {
    if (st->king_attack_info.pinned.IsSet(from)) {
      const Square sq = ksq(us);
      const int dx_from = file::make(from) - file::make(sq);
      const int dy_from = rank::make(from) - rank::make(sq);
      const int dx_to = file::make(to) - file::make(sq);
      const int dy_to = rank::make(to) - rank::make(sq);

      if (dx_from == 0 || dx_to == 0) {
        if (dx_from != dx_to) return false;
      }
      else if (dy_from == 0 || dy_to == 0) {
        if (dy_from != dy_to) return false;
      }
      else if (dx_from * dy_to != dy_from * dx_to)
        return false;
    }
    if (move_type == move::EN_PASSANT) {
      ApplyMove(m);
      const bool illegal = IsUnderAttack(us, ksq(us));
      UndoMove();
      if (illegal) return false;
    }
  }
  return true;
}

bool Board::IsUnderAttack(const Color us, const Square sq) const {
  const Color them = !us;
  return attack::attacks<ROOK>(sq, occupied_bb) &
    (pieces(them, ROOK) | pieces(them, QUEEN)) ||
    attack::attacks<BISHOP>(sq, occupied_bb) &
    (pieces(them, BISHOP) | pieces(them, QUEEN)) ||
    attack::knight_attacks[sq] & pieces(them, KNIGHT) ||
    attack::king_attacks[sq] & pieces(them, KING) ||
    attack::pawn_attacks[us][sq] & pieces(them, PAWN);
}

bool Board::IsInCheck() const {
  return IsUnderAttack(side_to_move, ksq(side_to_move));
}

Bitboard Board::AttackersTo(const Square sq, const Bitboard occupied) const {
  return attack::pawn_attacks[WHITE][sq] & pieces(BLACK, PAWN) |
    attack::pawn_attacks[BLACK][sq] & pieces(WHITE, PAWN) |
    attack::knight_attacks[sq] & pieces(KNIGHT) |
    attack::attacks<BISHOP>(sq, occupied) &
    (pieces(BISHOP) | pieces(QUEEN)) |
    attack::attacks<ROOK>(sq, occupied) & (pieces(ROOK) | pieces(QUEEN)) |
    attack::king_attacks[sq] & pieces(KING);
}

template <PieceType Pt>
Bitboard Board::AttacksBy(const Color c) {
  if constexpr (Pt == PAWN)
    return c == WHITE
      ? attack::PawnAttacksBb<WHITE>(pieces(c, PAWN))
      : attack::PawnAttacksBb<BLACK>(pieces(c, PAWN));
  if constexpr (Pt == KNIGHT) {
    Bitboard attackers = pieces(c, Pt);
    Bitboard threats{};
    while (attackers) threats |= attack::knight_attacks[attackers.PopLsb()];
    return threats;
  }
  Bitboard attackers = pieces(c, Pt);
  Bitboard threats{};
  while (attackers)
    threats |= attack::attacks<Pt>(attackers.PopLsb(), occupied());
  return threats;
}

template Bitboard Board::AttacksBy<PAWN>(Color c);
template Bitboard Board::AttacksBy<KNIGHT>(Color c);
template Bitboard Board::AttacksBy<BISHOP>(Color c);
template Bitboard Board::AttacksBy<ROOK>(Color c);
template Bitboard Board::AttacksBy<QUEEN>(Color c);

Bitboard Board::LeastValuablePiece(const Bitboard attacking,
                                   const Color attacker, Piece& pc) const {
  for (PieceType pt = PAWN; pt <= KING; ++pt) {
    if (Bitboard subset = attacking & pieces(pt)) {
      pc = piece::make(attacker, pt);
      return Bitboard::FromSquare(subset.Lsb());
    }
  }
  return {};
}

Score Board::see(const Move m) const {
  if (move::move_type(m) != move::NORMAL) return 0;
  const Square from = move::from(m);
  const Square to = move::to(m);
  Piece from_pc = PieceOn(from);
  const Piece to_pc = PieceOn(to);
  Color attacker = color::make(from_pc);
  Score gain[32], d = 0;
  Bitboard from_set = Bitboard::FromSquare(from);
  Bitboard occ = occupied_bb;
  Bitboard attacking = AttackersTo(to, occ);
  gain[0] = eval::kPieceValues[to_pc];
  for (;;) {
    ++d;
    attacker = !attacker;
    gain[d] = static_cast<Score>(eval::kPieceValues[from_pc] - gain[d - 1]);
    if (std::max(static_cast<Score>(-gain[d - 1]), gain[d]) < 0) break;
    attacking ^= from_set;
    occ ^= from_set;
    attacking |=
      occ &
      (attack::attacks<BISHOP>(to, occ) & (pieces(BISHOP) | pieces(QUEEN)) |
        attack::attacks<ROOK>(to, occ) & (pieces(ROOK) | pieces(QUEEN)));
    from_set = LeastValuablePiece(attacking, attacker, from_pc);
    if (!from_set) break;
  }
  while (--d)
    gain[d - 1] = static_cast<Score>(
      -std::max(static_cast<Score>(-gain[d - 1]), gain[d]));
  return gain[0];
}
