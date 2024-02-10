#include"attack.h"
#include"movegen.h"

template<Color c>
void generatePawnMoves(Board& board, MoveList& moveList) {
	constexpr Color us = c;
	constexpr Color them = !c;

	constexpr Direction up = c == WHITE ? NORTH : SOUTH;
	constexpr Direction upRight = c == WHITE ? NORTHEAST : SOUTHEAST;
	constexpr Direction upLeft = c == WHITE ? NORTHWEST : SOUTHWEST;

	Bitboard relative_rank_8_bb = c == WHITE ? RANK_8_BB : RANK_1_BB;
	Bitboard relative_rank_4_bb = c == WHITE ? RANK_4_BB : RANK_5_BB;

	Bitboard empty = ~board.occupied();
	Bitboard theirTeam = board.color(them);
	Bitboard ourPawns = board.pieces(us, PAWN);

  Square to;
	Square from;

	Bitboard singlePawnPushTargets = ourPawns.shift<up>() & empty;
	Bitboard upRightBB = ourPawns.shift<upRight>();
	Bitboard upLeftBB = ourPawns.shift<upLeft>();
	Bitboard upRightCaptures = upRightBB & theirTeam;
	Bitboard upLeftCaptures = upLeftBB & theirTeam;

	// single pawn pushes
	Bitboard attacks = singlePawnPushTargets - relative_rank_8_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - up;
		moveList.add(move::make(from, to));
	}
	// pawn push promotions
	attacks = singlePawnPushTargets & relative_rank_8_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - up;
		moveList.add(move::make<KNIGHT>(from, to));
		moveList.add(move::make<BISHOP>(from, to));
		moveList.add(move::make<ROOK>(from, to));
		moveList.add(move::make<QUEEN>(from, to));
	}
	// double pawn pushes
	attacks = singlePawnPushTargets.shift<up>() &
		empty &
		relative_rank_4_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - 2 * up;
		moveList.add(move::make(from, to));
	}
	// pawn captures
	attacks = upRightCaptures - relative_rank_8_bb;

	while (attacks) {
		to = attacks.popLSB();
		from = to - upRight;
		moveList.add(move::make(from, to));
	}
	attacks = upLeftCaptures - relative_rank_8_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - upLeft;
		moveList.add(move::make(from, to));
	}
	// pawn capture promotions
	attacks = upRightCaptures & relative_rank_8_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - upRight;
		moveList.add(move::make<KNIGHT>(from, to));
		moveList.add(move::make<BISHOP>(from, to));
		moveList.add(move::make<ROOK>(from, to));
		moveList.add(move::make<QUEEN>(from, to));
	}
	attacks = upLeftCaptures & relative_rank_8_bb;
	while (attacks) {
		to = attacks.popLSB();
		from = to - upLeft;
		moveList.add(move::make<KNIGHT>(from, to));
		moveList.add(move::make<BISHOP>(from, to));
		moveList.add(move::make<ROOK>(from, to));
		moveList.add(move::make<QUEEN>(from, to));
	}
	// en passant
	if (board.st->epSquare != NO_SQUARE) {
		attacks = upRightBB & Bitboard::fromSquare(board.st->epSquare);
		if (attacks) {
			to = board.st->epSquare;
			from = to - upRight;
			moveList.add(move::make(from, to, move::EN_PASSANT));
		}
		attacks = upLeftBB & Bitboard::fromSquare(board.st->epSquare);
		if (attacks) {
			to = board.st->epSquare;
			from = to - upLeft;
			moveList.add(move::make(from, to, move::EN_PASSANT));
		}
	}
}

template<Color c, PieceType pt>
void generatePieceMoves(Board& board, MoveList& moveList) {
	Bitboard friendly = board.color(c);
	Bitboard pieces = board.pieces(c, pt);

	while (pieces) {
		Square from = pieces.popLSB();

		Bitboard attacks = (pt == KNIGHT ? attacks::knightAttacks[from] : attacks::attacks<pt>(from, board.occupied())) - friendly;

		while (attacks)
			moveList.add(move::make(from, attacks.popLSB()));
	}
}

template<Color c>
void generateKingMoves(Board& board, MoveList& moveList) {
	Square ksq = board.ksq(c);
	Bitboard attacks = attacks::kingAttacks[ksq] - board.color(c);
	while (attacks)
		moveList.add(move::make(ksq, attacks.popLSB()));

	// castling
	if (ksq == square::relative(c, E1)) {
		Bitboard empty = ~board.occupied();
		Bitboard path1 = c == WHITE ? WHITE_QUEEN_SIDE_PATH : BLACK_QUEEN_SIDE_PATH;
		Bitboard path2 = c == WHITE ? WHITE_KING_SIDE_PATH : BLACK_KING_SIDE_PATH;

		if (board.canCastle(c == WHITE ? WHITE_QUEEN_SIDE : BLACK_QUEEN_SIDE) && (empty & path1) == path1)
			moveList.add(move::make(ksq, square::relative(c, C1), move::CASTLING));
		if (board.canCastle(c == WHITE ? WHITE_KING_SIDE : BLACK_KING_SIDE) && (empty & path2) == path2)
			moveList.add(move::make(ksq, square::relative(c, G1), move::CASTLING));
	}
}

void generateMoves(Board& board, MoveList& moveList) {
	if (!board.st->kingAttackInfo.computed)
		board.generateKingAttackInfo(board.st->kingAttackInfo);

	moveList.last = moveList.data;

	if (board.sideToMove == WHITE) {
		if (!board.st->kingAttackInfo.doubleCheck) {
			generatePawnMoves<WHITE>(board, moveList);
			generatePieceMoves<WHITE, KNIGHT>(board, moveList);
			generatePieceMoves<WHITE, BISHOP>(board, moveList);
			generatePieceMoves<WHITE, ROOK>(board, moveList);
			generatePieceMoves<WHITE, QUEEN>(board, moveList);
		}
		generateKingMoves<WHITE>(board, moveList);
	}

	else {
		if (!board.st->kingAttackInfo.doubleCheck) {
			generatePawnMoves<BLACK>(board, moveList);
			generatePieceMoves<BLACK, KNIGHT>(board, moveList);
			generatePieceMoves<BLACK, BISHOP>(board, moveList);
			generatePieceMoves<BLACK, ROOK>(board, moveList);
			generatePieceMoves<BLACK, QUEEN>(board, moveList);
		}
		generateKingMoves<BLACK>(board, moveList);
	}

	moveList.last = std::remove_if(moveList.begin(), moveList.end(),
		[&](ExtMove& m) { return !board.isLegal(m.move); });
}