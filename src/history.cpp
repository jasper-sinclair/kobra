#include"history.h"
#include"search.h"

void ButterflyHistory::increase(const Board& board, Move move, Depth d) {

	auto& e = data()[board.sideToMove][move::from(move)][move::to(move)];

	e += I_A * p(d) * (MAX - e) / MAX;
	assert(std::abs(e) < MAX);
}

void ButterflyHistory::decrease(const Board& board, Move move, Depth d) {

	auto& e = data()[board.sideToMove][move::from(move)][move::to(move)];

	e -= D_A * p(d) * (MAX + e) / MAX;
	assert(std::abs(e) < MAX);
}

void CaptureHistory::increase(const Board& board, Move move, Depth d) {
   const Piece moved = board.pieceOn(move::from(move));
   const Square to = move::pieceType(move) == move::EN_PASSANT ?
                        move::to(move) - direction::pawnPush(board.sideToMove) : move::to(move);
   const PieceType captured = move::pieceType(board.pieceOn(to));
	auto& e = data()[moved][to][captured];

	e += I_A * p(d) * (MAX - e) / MAX;
	assert(std::abs(e) < MAX);
}

void CaptureHistory::decrease(const Board& board, Move move, Depth d) {
   const Piece moved = board.pieceOn(move::from(move));
   const Square to = move::pieceType(move) == move::EN_PASSANT ?
                        move::to(move) - direction::pawnPush(board.sideToMove) : move::to(move);
   const PieceType captured = move::pieceType(board.pieceOn(to));
	auto& e = data()[moved][to][captured];

	e -= D_A * p(d) * (MAX + e) / MAX;
	assert(std::abs(e) < MAX);
}

void ContinuationHistory::increase(const Board& board, const Stack* ss, Move move, Depth d) {

	if ((ss-1)->move) {
		auto& e = data()[(ss-1)->moved][move::to((ss-1)->move)][board.pieceOn(move::from(move))][move::to(move)];
		e += I_A1 * p(d) * (MAX - e) / MAX;
		assert(std::abs(e) < MAX);

		if ((ss-2)->move) {
			e = data()[(ss-2)->moved][move::to((ss-2)->move)][board.pieceOn(move::from(move))][move::to(move)];
			e += I_A2 * p(d) * (MAX - e) / MAX;
			assert(std::abs(e) < MAX);

			if ((ss-4)->move) {
				e = data()[(ss-4)->moved][move::to((ss-4)->move)][board.pieceOn(move::from(move))][move::to(move)];
				e += I_A4 * p(d) * (MAX - e) / MAX;
				assert(std::abs(e) < MAX);
			}
		}
	}
}

void ContinuationHistory::decrease(const Board& board, const Stack* ss, Move move, Depth d) {

	if ((ss-1)->move) {
		auto& e = data()[(ss-1)->moved][move::to((ss-1)->move)][board.pieceOn(move::from(move))][move::to(move)];
		e -= D_A1 * p(d) * (MAX + e) / MAX;
		assert(std::abs(e) < MAX);

		if ((ss-2)->move) {
			e = data()[(ss-2)->moved][move::to((ss-2)->move)][board.pieceOn(move::from(move))][move::to(move)];
			e -= D_A2 * p(d) * (MAX + e) / MAX;
			assert(std::abs(e) < MAX);

			if ((ss-4)->move) {
				e = data()[(ss-4)->moved][move::to((ss-4)->move)][board.pieceOn(move::from(move))][move::to(move)];
				e -= D_A4 * p(d) * (MAX + e) / MAX;
				assert(std::abs(e) < MAX);
			}
		}
	}
}

void Histories::update(
	Board& board,
	Stack* ss,
	Move bestMove,
	MoveList& moves,
	Depth depth,
	bool isInCheck) {

	if (board.isCapture(bestMove)) {

		capture.increase(board, bestMove, depth);

		for (const auto& m : moves) {
			if (m.move != bestMove && board.isCapture(m.move)) {

				capture.decrease(board, m.move, depth);
			}
		}
	}

	else {
		if (bestMove != killer[ss->ply][0]) {
			killer[ss->ply][1] = killer[ss->ply][0];
			killer[ss->ply][0] = bestMove;
		}

		if ((ss-1)->move)
			counter[(ss-1)->moved][move::to((ss-1)->move)] = bestMove;

		butterfly.increase(board, bestMove, depth);
		continuation.increase(board, ss, bestMove, depth);

		for (const auto& m : moves) {
			if (m.move != bestMove) {
				if (board.isCapture(m.move)) {

					capture.decrease(board, m.move, depth);
				}

				else {
					butterfly.decrease(board, m.move, depth);
					continuation.decrease(board, ss, m.move, depth);
				}
			}
		}
	}
}

void Histories::clear() {
	std::memset(killer, 0, sizeof(killer));
	std::memset(counter, 0, sizeof(counter));
	butterfly.fill(BUTTERFLY_FILL);
	capture.fill(CAPTURE_FILL);
	continuation.fill(CONTINUATION_FILL);
}