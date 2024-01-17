#include"eval.h"
#include"movesort.h"
#include"search.h"

MoveSorter::MoveSorter(
	Board& board, 
	Stack* ss, 
	Histories& histories, 
	uint16_t hash_move, 
	bool is_in_check
) :
	board(board), 
	ss(ss), 
	histories(histories), 
	moves{}, 
	hash_move(hash_move), 
	is_in_check(is_in_check), 
	idx{},
	stage(HASH_MOVE)
{
	assert(!histories.killer[ss->ply][0] ||
		histories.killer[ss->ply][0] != histories.killer[ss->ply][1]);
}

uint16_t MoveSorter::next() {
	switch (stage) {

	case HASH_MOVE:
		++stage;
		if (board.IsPseudoLegal(hash_move) &&
			board.IsLegal(hash_move))
			return hash_move;

	case NORMAL_INIT:
		++stage;
		GenerateMoves(board, moves);

		for (int i = 0; i < moves.size(); ++i) {
			if (moves.move(i) == hash_move) {
				moves.remove(i);
				break;
			}
		}

		for (auto& m : moves)
			m.score = ComputeScore(m.move);

		std::sort(moves.begin(), moves.end(),
			[](const Move& m1, const Move& m2) { return m1.score > m2.score; }
		);

	case NORMAL:
		if (idx == moves.size()) return uint16_t();
		return moves.move(idx++);
	}
   return 0;
}

Score MoveSorter::ComputeScore(uint16_t m) {
	Score s = 0;

	if (board.IsCapture(m)) {
		Score see = board.see(m);
		s += see >= 0 ? 16000 : -16000;

		Piece moved = board.PieceOn(move::from(m));
		Square to = move::PieceType(m) == move::EN_PASSANT ?
			move::to(m) - direction::PawnPush(board.side_to_move) : move::to(m);
		int32_t captured = move::PieceType(board.PieceOn(to));

		s += eval::kPieceValues[board.PieceOn(move::to(m))] + 
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
				Square from = move::from(m);
				Square to = move::to(m);

				s += histories.butterfly[board.side_to_move][from][to] / 155 +
					histories.continuation[(ss-1)->moved][move::to((ss-1)->move)][board.PieceOn(from)][to] / 52 +
					histories.continuation[(ss-2)->moved][move::to((ss-2)->move)][board.PieceOn(from)][to] / 61 +
					histories.continuation[(ss-4)->moved][move::to((ss-4)->move)][board.PieceOn(from)][to] / 64;

				assert(std::abs(s) <= 8000);
			}
		}
	}
	return s;
}
