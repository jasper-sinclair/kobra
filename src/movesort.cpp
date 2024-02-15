#include"eval.h"
#include"movesort.h"
#include"search.h"

MoveSorter::MoveSorter(
	Board& board, 
	Stack* ss, 
	Histories& histories, 
	Move hashMove, 
	bool isInCheck
) :
	board(board), 
	ss(ss), 
	histories(histories), 
	hashMove(hashMove), 
	isInCheck(isInCheck), 
	idx{},
	stage(HASH_MOVE)
{
	assert(!histories.killer[ss->ply][0] ||
		histories.killer[ss->ply][0] != histories.killer[ss->ply][1]);
}

Move MoveSorter::next() {
	switch (stage) {

	case HASH_MOVE:
		++stage;
		if (board.isPseudoLegal(hashMove) &&
			board.isLegal(hashMove))
			return hashMove;

	case NORMAL_INIT:
		++stage;
		generateMoves(board, moves);

		for (int i = 0; i < moves.size(); ++i) {
			if (moves.move(i) == hashMove) {
				moves.remove(i);
				break;
			}
		}

		for (auto& m : moves)
			m.score = computeScore(m.move);

		std::sort(moves.begin(), moves.end(),
			[](const ExtMove& m1, const ExtMove& m2) { return m1.score > m2.score; }
		);

	case NORMAL:
		if (idx == moves.size()) return Move();
		return moves.move(idx++);
  default: ;
  }
  return 0;
}

Score MoveSorter::computeScore(Move m) const
{
	Score s = 0;

	if (board.isCapture(m)) {
      const Score see = board.see(m);
		s += see >= 0 ? 16000 : -16000;

      const Piece moved = board.pieceOn(move::from(m));
      const Square to = move::pieceType(m) == move::EN_PASSANT ?
                           move::to(m) - direction::pawnPush(board.sideToMove) : move::to(m);
      const PieceType captured = move::pieceType(board.pieceOn(to));

		s += eval::PIECE_VALUES[board.pieceOn(move::to(m))] + 
			histories.capture[moved][to][captured] / 89;
	}
	else {
		if (m == histories.killer[ss->ply][0])
			s += 8004;
		else if (m == histories.killer[ss->ply][1])
			s += 8003;
		else if (ss->ply >= 2 && m == histories.killer[ss->ply - 2][0])
			s += 8002;

		else {
			if (m == histories.counter[(ss-1)->moved][move::to((ss-1)->move)])
				s += 8001;

			else {
            const Square from = move::from(m);
            const Square to = move::to(m);

				s += histories.butterfly[board.sideToMove][from][to] / 155 +
					histories.continuation[(ss-1)->moved][move::to((ss-1)->move)][board.pieceOn(from)][to] / 52 +
					histories.continuation[(ss-2)->moved][move::to((ss-2)->move)][board.pieceOn(from)][to] / 61 +
					histories.continuation[(ss-4)->moved][move::to((ss-4)->move)][board.pieceOn(from)][to] / 64;

				assert(std::abs(s) <= 8000);
			}
		}
	}
	return s;
}
