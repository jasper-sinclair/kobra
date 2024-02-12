#include<algorithm>
#include<sstream>
#include<string>

#include"attack.h"
#include"board.h"
#include"eval.h"

Board::Board(const std::string& FEN) {
   std::memset(this, 0, sizeof(Board));
   history = {};
   history.emplace_back();
   st = getBoardStatus();

   std::istringstream ss(FEN);
   std::string token;

   ss >> token;
   Square sq = A8;
   Piece pc;
   for (const char c : token) {
      if (c == ' ') continue;

      if ((pc = piece::PIECE_TO_CHAR.find(c)) != std::string::npos) {
         setPiece<false>(pc, sq);
         sq += EAST;
      }

      else if (isdigit(c))
         sq += (c - '0') * EAST;

      else if (c == '/') {
         sq += 2 * SOUTH;
      }
   }

   ss >> token;
   sideToMove = (token == "w") ? WHITE : BLACK;

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
      default:;
      }
   }

   ss >> token;

   st->epSquare = (token == "-") ? NO_SQUARE : square::make(token);

   ss >> st->fiftyMoveCount;

   ss >> st->plyCount;
   st->plyCount = 2 * (st->plyCount - 1) + sideToMove;

   st->zobrist = 42;
}

Board::Board(const Board& other) {
   *this = other;
}

Board& Board::operator=(const Board& other) {
   std::memcpy(board, other.board, N_SQUARES * sizeof(Piece));
   std::memcpy(pieceBB, other.pieceBB, N_PIECE_TYPES * sizeof(Bitboard));
   std::memcpy(colorBB, other.colorBB, N_COLORS * sizeof(Bitboard));
   occupiedBB = other.occupiedBB;
   sideToMove = other.sideToMove;
   history = other.history;
   st = getBoardStatus();

   return *this;
}

std::ostream& operator<<(std::ostream& os, Board& board) {
   const std::string hor = "+---+---+---+---+---+---+---+---+";
   const std::string ver = "|";

   for (Rank r = RANK_8; r >= RANK_1; --r) {
      os << hor;
      os << '\n';

      for (File f = FILE_A; f <= FILE_H; ++f) {
         const Piece pc = board.pieceOn(square::make(f, r));
         const char c = piece::PIECE_TO_CHAR[pc];
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

std::ostream& operator<<(std::ostream& os, BoardStatus& bs) {
   os << "plyCount: " << bs.plyCount << '\n'
      << "fityMoveCount: " << bs.fiftyMoveCount << '\n'
      << "castlings: " << bs.castlings.data << '\n'
      << "repetitions: " << bs.repetitions << '\n'
      << "epSquare: " << square::toString(bs.epSquare) << '\n'
      << "captured: " << bs.captured << '\n'
      << "move: " << move::toString(bs.move) << '\n';
   return os;
}

std::string Board::fen() {
   std::stringstream ss;

   for (Rank r = RANK_8; r >= RANK_1; --r) {
      int emptyCount = 0;
      for (File f = FILE_A; f <= FILE_H; ++f) {
         const Piece pc = pieceOn(square::make(f, r));
         if (pc) {
            if (emptyCount)
               ss << emptyCount;
            ss << piece::PIECE_TO_CHAR[pc];
            emptyCount = 0;
         }
         else ++emptyCount;
      }
      if (emptyCount) ss << emptyCount;
      if (r != RANK_1) ss << '/';
   }

   ss << ' ' << ((sideToMove == WHITE) ? 'w' : 'b');

   ss << ' ';
   if (canCastle(WHITE_KING_SIDE)) ss << 'K';
   if (canCastle(WHITE_QUEEN_SIDE)) ss << 'Q';
   if (canCastle(BLACK_KING_SIDE)) ss << 'k';
   if (canCastle(BLACK_QUEEN_SIDE)) ss << 'q';
   if (noCastling()) ss << '-';

   ss << ' ' << ((st->epSquare == NO_SQUARE) ? "-" : square::toString(st->epSquare));
   ss << ' ' << st->fiftyMoveCount;
   ss << ' ' << 1 + (st->plyCount - sideToMove) / 2;

   return ss.str();
}

template<bool updateZobrist>
void Board::setPiece(Piece pc, Square sq) {
   board[sq] = pc;
   pieceBB[pieceType::make(pc)].set(sq);
   colorBB[color::make(pc)].set(sq);
   occupiedBB.set(sq);

   if (updateZobrist)
      st->zobrist ^= zobrist::psq[pc][sq];
}

template<bool updateZobrist>
void Board::removePiece(Square sq) {
   if (updateZobrist)
      st->zobrist ^= zobrist::psq[pieceOn(sq)][sq];

   pieceBB[pieceType::make(pieceOn(sq))].clear(sq);
   colorBB[color::make(pieceOn(sq))].clear(sq);
   board[sq] = NO_PIECE;
   occupiedBB.clear(sq);
}

template<bool updateZobrist>
void Board::movePiece(Square from, Square to) {
   setPiece<updateZobrist>(pieceOn(from), to);
   removePiece<updateZobrist>(from);
}

void Board::applyMove(Move m) {
   const Square from = move::from(m);
   const Square to = move::to(m);
   const move::MoveType moveType = move::moveType(m);
   Piece pc = pieceOn(from);
   const PieceType pt = pieceType::make(pc);
   const Color us = sideToMove;
   const Color them = !us;
   const Direction push = direction::pawnPush(us);

   BoardStatus bs;
   bs.plyCount = st->plyCount + 1;
   bs.fiftyMoveCount = st->fiftyMoveCount + 1;
   bs.castlings = st->castlings;
   bs.zobrist = st->zobrist;

   bs.epSquare = NO_SQUARE;
   bs.captured = moveType == move::EN_PASSANT ? piece::make(them, PAWN) : pieceOn(to);
   bs.move = m;
   bs.kingAttackInfo.computed = false;

   // reset ep square
   if (st->epSquare) {
      st->zobrist ^= zobrist::enPassant[file::make(st->epSquare)];
   }

   // capture
   if (bs.captured) {
      bs.fiftyMoveCount = 0;
      Square capsq = to;

      if (moveType == move::EN_PASSANT)
         capsq -= push;

      removePiece(capsq);

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

   // pawn move
   if (pt == PAWN) {
      bs.fiftyMoveCount = 0;

      if (to - from == 2 * push) {
         bs.epSquare = to - push;
         st->zobrist ^= zobrist::enPassant[file::make(bs.epSquare)];
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
            const Square rfrom = square::relative(us, A1);
            const Square rto = square::relative(us, D1);
            movePiece(rfrom, rto);
         }
         else if (to == square::relative(us, G1)) {
            const Square rfrom = square::relative(us, H1);
            const Square rto = square::relative(us, F1);
            movePiece(rfrom, rto);
         }
      }
   }

   // rook move
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

   // promotion
   if (moveType == move::PROMOTION) {
      removePiece(from);
      pc = piece::make(us, move::pieceType(m));
      setPiece(pc, to);
   }
   else {
      movePiece(from, to);
   }

   st->zobrist ^= zobrist::side;

   sideToMove = !sideToMove;

   // compute repetitions
   bs.repetitions = 0;
   for (int i = static_cast<int>(history.size()) - 4; i > static_cast<int>(history.size()) - bs.fiftyMoveCount - 1; i -= 2) {
      if (i <= 0)
         break;
      if (history[i].zobrist == st->zobrist) {
         bs.repetitions = history[i].repetitions + 1;
         break;
      }
   }

   std::swap(st->zobrist, bs.zobrist);
   history.push_back(bs);
   st = getBoardStatus();
}

void Board::undoMove() {
   sideToMove = !sideToMove;

   const Square from = move::from(st->move);
   Square to = move::to(st->move);
   const move::MoveType moveType = move::moveType(st->move);
   Piece pc = pieceOn(to);
   const Color us = sideToMove;
   Color them = !us;
   const Direction push = direction::pawnPush(us);

   if (moveType == move::EN_PASSANT) {
      movePiece<false>(to, from);
      to -= push;
   }

   else if (moveType == move::PROMOTION) {
      removePiece<false>(to);
      setPiece<false>(piece::make(us, PAWN), from);
   }

   else if (moveType == move::CASTLING) {
      const bool isKingSide = to > from;
      const Square rto = square::relative(us, isKingSide ? F1 : D1);
      const Square rfrom = square::relative(us, isKingSide ? H1 : A1);
      movePiece<false>(rto, rfrom);
      movePiece<false>(to, from);
   }

   else movePiece<false>(to, from);

   if (st->captured) setPiece<false>(st->captured, to);

   history.pop_back();
   st = getBoardStatus();
}

void Board::applyNullMove() {
   BoardStatus bs;
   bs.plyCount = st->plyCount;
   bs.fiftyMoveCount = st->fiftyMoveCount;
   bs.castlings = st->castlings;
   bs.repetitions = 0;
   bs.epSquare = NO_SQUARE;
   bs.captured = NO_PIECE;
   bs.move = 0;
   bs.kingAttackInfo.computed = false;

   history.push_back(bs);
   st = getBoardStatus();
   sideToMove = !sideToMove;
}

void Board::undoNullMove() {
   sideToMove = !sideToMove;
   history.pop_back();
   st = getBoardStatus();
}

bool Board::givesCheck(Move m) {
   Bitboard theirKingBB = pieces(!sideToMove, KING);
   const Square ksq = theirKingBB.LSB();
   const Square from = move::from(m);
   const Square to = move::to(m);
   const move::MoveType moveType = move::moveType(m);
   Piece pt = pieceType::make(pieceOn(from));
   Bitboard occ = occupiedBB;

   // promotion
   if (moveType == move::PROMOTION) {
      occ.clear(from);
      pt = move::pieceType(m);
   }

   Bitboard attacked;
   switch (pt) {
   case PAWN: {
      attacked = attacks::pawnAttacks[sideToMove][to];
      break;
   }
   case KNIGHT: {
      attacked = attacks::knightAttacks[to];
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
   if (attacked & theirKingBB)
      return true;

   // castling
   if (moveType == move::CASTLING) {
      occ.clear(from);
      const Square rsq = sideToMove == WHITE ? to > from ? F1 : D1 : to > from ? F8 : D8;
      if (attacks::attacks<ROOK>(rsq, occ) & theirKingBB)
         return true;
   }

   // en passant
   else if (moveType == move::EN_PASSANT) {
      occ.clear(from);
      occ.clear(to - direction::pawnPush(sideToMove));
      occ.set(to);
      if (isUnderAttack(!sideToMove, ksq))
         return true;
   }

   // discovery
   else {
      occ.clear(from);
      occ.set(to);

      if (isUnderAttack(!sideToMove, ksq))
         return true;
   }

   return false;
}

bool Board::isPseudoLegal(Move m) {
   if (!m) return false;

   const Color us = sideToMove;
   const Color them = !sideToMove;
   const Square from = move::from(m);
   const Square to = move::to(m);
   const move::MoveType moveType = move::moveType(m);
   const Piece pc = pieceOn(from);
   const PieceType pt = pieceType::make(pc);

   if (pc == NO_PIECE ||
      color::make(pc) == them ||
      pt != PAWN && (moveType == move::PROMOTION || moveType == move::EN_PASSANT) ||
      pt != KING && moveType == move::CASTLING)
      return false;

   if (pt == KNIGHT) {
      if (color(us).isSet(to) ||
         !attacks::knightAttacks[from].isSet(to))
         return false;
   }

   else if (pt == ROOK) {
      if (color(us).isSet(to) ||
         !attacks::rookAttacks[from].isSet(to) ||
         attacks::inBetweenSquares[from][to] & occupiedBB)
         return false;
   }

   else if (pt == BISHOP) {
      if (color(us).isSet(to) ||
         !attacks::bishopAttacks[from].isSet(to) ||
         attacks::inBetweenSquares[from][to] & occupiedBB)
         return false;
   }

   else if (pt == QUEEN) {
      if (color(us).isSet(to) ||
         !(attacks::rookAttacks[from].isSet(to) ||
            attacks::bishopAttacks[from].isSet(to)) ||
         attacks::inBetweenSquares[from][to] & occupiedBB)
         return false;
   }

   else if (pt == PAWN) {
      const int pawnPush = direction::pawnPush(us);

      if (rank::make(from) == (us == WHITE ? RANK_7 : RANK_2) && moveType != move::PROMOTION)
         return false;

      if (rank::make(from) == (us == WHITE ? RANK_2 : RANK_7) &&
         to - from == 2 * pawnPush &&
         !occupiedBB.isSet(from + pawnPush) &&
         !occupiedBB.isSet(to))
         return true;

      else if (to - from == pawnPush) {
         if (!occupiedBB.isSet(to))
            return true;
      }

      else if (moveType == move::EN_PASSANT) {
         if (st->epSquare == to &&
            pieceOn(to - pawnPush) == piece::make(them, PAWN) &&
            !occupiedBB.isSet(to) &&
            attacks::pawnAttacks[us][from].isSet(to))
            return true;
      }

      else if (square::distance(from, to) == 1 &&
         color(them).isSet(to)) {
         if (attacks::pawnAttacks[us][from].isSet(to))
            return true;
      }

      return false;
   }

   else if (pt == KING) {
      if (moveType == move::CASTLING) {
         const Color c = rank::make(to) == RANK_1 ? WHITE : BLACK;
         const bool kingSide = to > from;

         if (c != us ||
            !canCastle(c == WHITE ? kingSide ? WHITE_KING_SIDE : WHITE_QUEEN_SIDE :
               kingSide ? BLACK_KING_SIDE : BLACK_QUEEN_SIDE) ||
            occupiedBB & (c == WHITE ? kingSide ? WHITE_KING_SIDE_PATH : WHITE_QUEEN_SIDE_PATH :
               kingSide ? BLACK_KING_SIDE_PATH : BLACK_QUEEN_SIDE_PATH))
            return false;
      }
      else {
         if (color(us).isSet(to) ||
            square::distance(from, to) != 1)
            return false;
      }
   }

   return true;
}

void Board::generateKingAttackInfo(KingAttackInfo& k) {
   k.computed = true;

   const Color them = !sideToMove;
   const Square ksq = this->ksq(sideToMove);
   const Bitboard ourTeam = color(sideToMove);
   const Bitboard theirTeam = color(them);

   k.pinned = {};
   k.attacks = theirTeam & (
      attacks::pawnAttacks[sideToMove][ksq] & pieces(PAWN) |
      attacks::knightAttacks[ksq] & pieces(KNIGHT));

   int attackerCount = k.attacks.popcount();

   Bitboard sliderAttackers = theirTeam & (
      attacks::attacks<BISHOP>(ksq, theirTeam) & (pieces(BISHOP) | pieces(QUEEN)) |
      attacks::attacks<ROOK>(ksq, theirTeam) & (pieces(ROOK) | pieces(QUEEN)));

   while (sliderAttackers) {
      const Square s = sliderAttackers.popLSB();
      const Bitboard between = attacks::inBetweenSquares[ksq][s];
      Bitboard blockers = between & ourTeam;

      const auto numBlockers = blockers.popcount();
      if (numBlockers == 0) {
         ++attackerCount;
         k.attacks |= between;
         k.attacks |= Bitboard::fromSquare(s);
      }
      else if (numBlockers == 1)
         k.pinned.set(blockers.LSB());
   }

   k.doubleCheck = attackerCount == 2;
}

bool Board::isLegal(Move m) {
   if (!st->kingAttackInfo.computed)
      generateKingAttackInfo(st->kingAttackInfo);

   const Square from = move::from(m);
   const Square to = move::to(m);
   const move::MoveType moveType = move::moveType(m);
   const Piece pc = pieceOn(from);
   const Color us = sideToMove;
   Color them = !us;

   if (pc == piece::make(us, KING)) {
      if (moveType == move::CASTLING) {
         if (st->kingAttackInfo.check() ||
            to == square::relative(us, C1) &&
            (isUnderAttack(us, square::relative(us, D1)) ||
               isUnderAttack(us, square::relative(us, C1))) ||
            to == square::relative(us, G1) &&
            (isUnderAttack(us, square::relative(us, F1)) ||
               isUnderAttack(us, square::relative(us, G1))))
            return false;
      }
      else {
         const Bitboard occCopy = occupiedBB;
         const PieceType captured = pieceType::make(pieceOn(to));

         occupiedBB.clear(from);
         occupiedBB.set(to);

         bool illegal;
         if (captured) {
            pieceBB[captured].clear(to);
            illegal = isUnderAttack(us, to);
            pieceBB[captured].set(to);
         }
         else illegal = isUnderAttack(us, to);

         occupiedBB = occCopy;
         if (illegal) return false;
      }
   }

   else if (st->kingAttackInfo.check() &&
      (!st->kingAttackInfo.attacks.isSet(to) || st->kingAttackInfo.pinned.isSet(from)) ||
      st->kingAttackInfo.doubleCheck)
      return false;

   else {
      // pins when not in check
      if (st->kingAttackInfo.pinned.isSet(from)) {
         const Square sq = ksq(us);
         const int dx_from = file::make(from) - file::make(sq);
         const int dy_from = rank::make(from) - rank::make(sq);
         const int dx_to = file::make(to) - file::make(sq);
         const int dy_to = rank::make(to) - rank::make(sq);

         // north, south
         if (dx_from == 0 || dx_to == 0) {
            if (dx_from != dx_to)
               return false;
         }
         // east, west
         else if (dy_from == 0 || dy_to == 0) {
            if (dy_from != dy_to)
               return false;
         }
         // northeast, southeast, southwest, northwest
         else if (dx_from * dy_to != dy_from * dx_to)
            return false;
      }

      // en passant
      if (moveType == move::EN_PASSANT) {
         applyMove(m);
         const bool illegal = isUnderAttack(us, ksq(us));
         undoMove();
         if (illegal) return false;
      }
   }
   return true;
}

bool Board::isUnderAttack(Color us, Square sq) {
   const Color them = !us;
   return attacks::attacks<ROOK>(sq, occupiedBB) & (pieces(them, ROOK) | pieces(them, QUEEN)) ||
      attacks::attacks<BISHOP>(sq, occupiedBB) & (pieces(them, BISHOP) | pieces(them, QUEEN)) ||
      attacks::knightAttacks[sq] & pieces(them, KNIGHT) ||
      attacks::kingAttacks[sq] & pieces(them, KING) ||
      attacks::pawnAttacks[us][sq] & pieces(them, PAWN);
}

bool Board::isInCheck() {
   return isUnderAttack(sideToMove, ksq(sideToMove));
}

Bitboard Board::attackersTo(Square sq, Bitboard occupied) {
   return attacks::pawnAttacks[WHITE][sq] & pieces(BLACK, PAWN) |
      attacks::pawnAttacks[BLACK][sq] & pieces(WHITE, PAWN) |
      attacks::knightAttacks[sq] & pieces(KNIGHT) |
      attacks::attacks<BISHOP>(sq, occupied) & (pieces(BISHOP) | pieces(QUEEN)) |
      attacks::attacks<ROOK>(sq, occupied) & (pieces(ROOK) | pieces(QUEEN)) |
      attacks::kingAttacks[sq] & pieces(KING);
}

Bitboard Board::leastValuablePiece(Bitboard attadef, Color attacker, Piece& pc) {
   for (PieceType pt = PAWN; pt <= KING; ++pt) {
      Bitboard subset = attadef & pieces(pt);
      if (subset) {
         pc = piece::make(attacker, pt);
         return Bitboard::fromSquare(subset.LSB());
      }
   }
   return {};
}

Score Board::see(Move m) {
   if (move::moveType(m) != move::NORMAL) return 0;

   const Square from = move::from(m);
   const Square to = move::to(m);
   Piece fromPc = pieceOn(from);
   const Piece toPc = pieceOn(to);
   Color attacker = color::make(fromPc);

   Score gain[32], d = 0;
   Bitboard fromSet = Bitboard::fromSquare(from);
   Bitboard occ = occupiedBB;
   Bitboard attadef = attackersTo(to, occ);
   gain[0] = eval::PIECE_VALUES[toPc];

   for (;;) {
      ++d;
      attacker = !attacker;
      gain[d] = eval::PIECE_VALUES[fromPc] - gain[d - 1];

      if (std::max(static_cast<Score>(-gain[d - 1]), gain[d]) < 0)
         break;

      attadef ^= fromSet;
      occ ^= fromSet;
      attadef |= occ & (attacks::attacks<BISHOP>(to, occ) & (pieces(BISHOP) | pieces(QUEEN)) |
         attacks::attacks<ROOK>(to, occ) & (pieces(ROOK) | pieces(QUEEN)));
      fromSet = leastValuablePiece(attadef, attacker, fromPc);

      if (!fromSet)
         break;
   }
   while (--d)
      gain[d - 1] = -std::max(static_cast<Score>(-gain[d - 1]), gain[d]);
   return gain[0];
}