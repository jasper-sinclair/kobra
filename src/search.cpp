#include"eval.h"
#include"search.h"

template<bool mainThread>
Move Search::bestMove(Board& board, ThreadID id) {

	if (mainThread) {

		for (auto& td : threadData) {
			td->pv.clear();
			td->nodeCount = 0;
			td->rootDepth = 1;
			td->selDepth = 1;
			td->nmpMinPly = 0;
		}

		for (ThreadID i = 1; i < numThreads; ++i)
			threads.emplace_back(&Search::bestMove<false>, this, std::ref(board), i);
	}

	ThreadData& td = *threadData[id];
	Board copy(board);

	Score alpha = -INFINITY_SCORE;
	Score beta = INFINITY_SCORE;
	Score delta;
	Score score;
	Score prevScore;
	Depth maxDepth = MAX_DEPTH;

	for (int i = 0; i < MAX_PLY + CONTINUATION_PLY; ++i) {
		td.stack[i].ply = i - CONTINUATION_PLY;
		td.stack[i].moved = NO_PIECE;
		td.stack[i].pvSize = 0;
		td.stack[i].move = Move();
	}

	Stack* ss = &td.stack[CONTINUATION_PLY];
	*ss = Stack();

	// iterative deepening
	for (;;) {
		if (td.rootDepth >= maxDepth) break;
		if (mainThread && time.useDepthLimit && td.rootDepth > time.depthLimit) break;

		if (td.rootDepth < 4)
			score = alphaBeta<Root>(copy, alpha, beta, td.rootDepth, td, ss);

		else {
			delta = 25;
			alpha = std::max(score - delta, -INFINITY_SCORE);
			beta = std::min(score + delta, (int)INFINITY_SCORE);
			for (;;) {
				score = alphaBeta<Root>(copy, alpha, beta, td.rootDepth, td, ss);

				if (time.stop) break;

				if (score <= alpha) {
					beta = (alpha + beta) / 2;
					alpha = std::max(alpha - delta, -INFINITY_SCORE);
				}
				else if (score >= beta)
					beta = std::min(beta + delta, (int)INFINITY_SCORE);

				else break;

				delta += delta / 4;
			}
		}

		if (time.stop) break;

		prevScore = score;

		td.pv.clear();
		for (int i = 0; i < ss->pvSize; ++i)
			td.pv.push_back(ss->pv[i]);

		if (mainThread)
			std::cout << info(td, td.rootDepth, score) << std::endl;

		++td.rootDepth;
	}
	
	if (mainThread) {
		// stop the search
		stop();
		for (auto& t : threads)
			t.join();
		threads.clear();

		if (time.useNodeLimit || time.useMoveTimeLimit)
			std::cout << info(td, td.rootDepth, prevScore) << std::endl;
		return td.pv[0];
	}

	return Move();
}

template Move Search::bestMove<false>(Board& board, ThreadID id);
template Move Search::bestMove<true>(Board& board, ThreadID id);

template<SearchType searchType, bool skipHashMove>
Score Search::alphaBeta(Board& board, Score alpha, Score beta, Depth depth, ThreadData& td, Stack* ss) {
	constexpr bool rootNode = searchType == Root;
	constexpr bool pvNode = searchType != NonPV;
	
	if (!pvNode) assert(beta - alpha == 1);
	assert(alpha < beta);

	bool mainThread = td.id == 0;

	if (mainThread &&
		depth >= 5) time.update(nodeCount());

	if (time.stop) return STOP_SCORE;
	
	if (board.isDraw()) return DRAW_SCORE;
	
	if (depth <= 0)
		return quiescence<pvNode ? PV : NonPV>(board, alpha, beta, td, ss);

	++td.nodeCount;

	if (pvNode)
		td.selDepth = std::max(td.selDepth, Depth(ss->ply+1));

	// mate distance pruning
	if (!rootNode) {
		alpha = std::max(-MATE_SCORE + ss->ply, (int)alpha);
		beta = std::min(MATE_SCORE - ss->ply - 1, (int)beta);
		if (alpha >= beta) return alpha;
	}
	
	// transposition table lookup
	Key key = board.key();
	TTEntry tte;
	bool ttHit = tt.probe(key, tte);
	Score ttScore = TranspositionTable::scoreFromTT(tte.score, ss->ply);
	
	if (!pvNode && 
		!skipHashMove && 
		ttHit && 
		depth <= tte.depth) {
	
		if (tte.nodeType == PV_NODE ||
			tte.nodeType == CUT_NODE && ttScore >= beta ||
			tte.nodeType == ALL_NODE && ttScore <= alpha) {
			if (board.st->fiftyMoveCount < 90)
				return ttScore;
		}
	}

	bool isInCheck = board.isInCheck();
	Score staticEval, eval;

	if (ttHit && depth <= tte.depth) {
		staticEval = TranspositionTable::scoreFromTT(tte.eval, ss->ply);

		if (tte.nodeType == CUT_NODE && ttScore > staticEval ||
			tte.nodeType == ALL_NODE && ttScore < staticEval ||
			tte.nodeType == PV_NODE)
			eval = ttScore;

		else eval = staticEval;
	}

	else eval = staticEval = evaluate(board);

	// reset killer moves
	td.histories.killer[(ss+1)->ply][0] = td.histories.killer[(ss+1)->ply][1] = Move();
	
	if (!rootNode && !isInCheck) {

		// futility pruning
		if (!pvNode &&
			eval >= beta + 171 * depth &&
			eval < MIN_MATE_SCORE)
			return eval;

		// null move pruning
		if (!pvNode &&
			!skipHashMove &&
			(ss->ply >= td.nmpMinPly || board.sideToMove != td.nmpColor) &&
			(ss-1)->move &&
			board.nonPawnMaterial(board.sideToMove) &&
			eval >= beta + 30) {

			Depth r = (14 + depth) / 5;
			Depth nullDepth = depth - r;

			board.applyNullMove();
			Score nullScore = -alphaBeta<NonPV>(board, -beta, -alpha, nullDepth, td, ss+1);
			board.undoNullMove();

			if (nullScore >= beta) {
				if (nullScore > MIN_MATE_SCORE)
					nullScore = beta;

				if (td.nmpMinPly) return nullScore;

				td.nmpMinPly = ss->ply + 4 * nullDepth / 5;
				td.nmpColor = board.sideToMove;

				Score s = alphaBeta<NonPV>(board, alpha, beta, nullDepth, td, ss);

				td.nmpMinPly = 0;

				if (s >= beta) return nullScore;
			}
		}

		// If the position is not in TT, then the position is probably bad.
		if (pvNode &&
			!ttHit)
			--depth;

		if (depth <= 0)
			return quiescence<pvNode ? PV : NonPV>(board, alpha, beta, td, ss);
	}

	Move hashMove = (pvNode && td.pv.size() > ss->ply) ? td.pv[ss->ply] :
		ttHit && (tte.nodeType == PV_NODE || tte.nodeType == CUT_NODE) ? tte.move : Move();
	
	MoveSorter moveSorter(board, ss, td.histories, hashMove, isInCheck);
	Score bestScore = -INFINITY_SCORE;
	Score score;
	Move bestMove = Move();
	int moveCount = 0;
	
	for (;;) {
		Move m = moveSorter.next();
		if (!m) break;
	
		assert(board.isPseudoLegal(m));
		assert(board.isLegal(m));
	
		if (pvNode) (ss+1)->pvSize = 0;
	
		++moveCount;
	
		if (mainThread && rootNode && time.elapsed() >= MIN_DISPLAY_TIME) {
			std::cout << "info depth " << depth <<
				" currmove " << move::toString(m) <<
				" currmovenumber " << moveCount << std::endl;
		}
	
		if (skipHashMove && m == ss->hashMove) continue;
	
		Square from = move::from(m);
		Square to = move::to(m);
	
		ss->moved = board.pieceOn(from);
		ss->move = m;
	
		bool isCapture = board.isCapture(m);
		Piece moved = board.pieceOn(from);
		Depth newDepth = depth - 1;
		Depth extension = 0;
		Depth r = 0;
		bool lmr = false;
	
		// singular extension
		if (!rootNode &&
			!skipHashMove &&
			moveCount == 1 &&
			depth >= 8 &&
			m == hashMove &&
			(tte.nodeType == CUT_NODE || tte.nodeType == PV_NODE) &&
			tte.depth + 3 >= depth &&
			std::abs(eval) < MIN_MATE_SCORE) {
	
			assert(m == hashMove);
	
			Score singularBeta = std::min(Score(eval - 2 * depth), beta);
			Depth singularDepth = depth / 2;
	
			ss->hashMove = m;
			score = alphaBeta<NonPV, true>(board, singularBeta-1, singularBeta, singularDepth, td, ss);
	
			if (score < singularBeta) extension = 1;
		}

		newDepth += extension;

		// Late Move Reductions
		if (depth >= 2 &&
			moveCount >= 2 &&
			!(pvNode && isCapture)) {

			lmr = true;

			r = lateMoveReductions[depth][moveCount];

			if (isCapture) {
				Square capSq = move::pieceType(m) == move::EN_PASSANT ? to - direction::pawnPush(board.sideToMove) : to;
				PieceType capPt = move::pieceType(board.pieceOn(capSq));
				Score hScore = td.histories.capture[moved][capSq][capPt] / 61 - 1155;

				r -= hScore;
			}

			else {
				Score hScore = td.histories.butterfly[board.sideToMove][from][to] / 126 +
					td.histories.continuation[(ss-1)->moved][move::to((ss-1)->move)][moved][to] / 66 +
					td.histories.continuation[(ss-2)->moved][move::to((ss-2)->move)][moved][to] / 89 +
					td.histories.continuation[(ss-4)->moved][move::to((ss-4)->move)][moved][to] / 93 +
					-1176;

				r -= hScore;
			}

			if (pvNode)
				r -= 1057 + 11026 / (3 + depth);

			if (hashMove && board.isCapture(hashMove))
				r += 839;

			if (isInCheck)
				r -= 722;

			if (board.givesCheck(m))
				r -= 796;
		}

		board.applyMove(m);

		if (lmr) {
			r = std::round(r / (double)LMR_FACTOR);
			Depth d = r > 0 ? newDepth-r : newDepth;
			score = -alphaBeta<NonPV>(board, -alpha-1, -alpha, d, td, ss+1);

			if (r > 0 && score > alpha)
				score = -alphaBeta<NonPV>(board, -alpha-1, -alpha, newDepth, td, ss+1);
		}

		else if (!pvNode || moveCount > 1)
			score = -alphaBeta<NonPV>(board, -alpha-1, -alpha, newDepth, td, ss+1);

		if (pvNode && (moveCount == 1 || (score > alpha && (rootNode || score < beta))))
			score = -alphaBeta<PV>(board, -beta, -alpha, newDepth, td, ss+1);
	
		board.undoMove();
	
		if (time.stop) return STOP_SCORE;
	
		if (score > bestScore) {
			bestScore = score;
	
			if (pvNode) {
				ss->pv[0] = m;
				std::memcpy(ss->pv+1, (ss+1)->pv, (ss+1)->pvSize * sizeof(Move));
				ss->pvSize = (ss+1)->pvSize + 1;
			}
	
			if (score > alpha) {
				bestMove = m;
				if (score < beta) alpha = score;
				else break;
			}
		}
	}

	if (skipHashMove && moveCount == 1) return alpha;
	
	// checkmate or stalemate
	if (!moveCount)
		return isInCheck ? -MATE_SCORE + ss->ply : DRAW_SCORE;
	
	if (!skipHashMove) {
		NodeType nodeType = bestScore >= beta ? CUT_NODE :
			pvNode && bestMove ? PV_NODE : ALL_NODE;
	
		assert(nodeType == CUT_NODE || nodeType == PV_NODE == bool(bestMove));

		if (bestMove)
			td.histories.update(board, ss, bestMove, moveSorter.moves, depth, isInCheck);

		tt.save(key, TranspositionTable::scoreToTT(bestScore, ss->ply), 
			TranspositionTable::scoreToTT(eval, ss->ply), bestMove, depth, nodeType);
	}

	return bestScore;
}

// Quiescence search. 
// When in check, all moves are searched.
// When not in check, only captures and queen promotions are searched.
template<SearchType searchType>
Score Search::quiescence(Board& board, Score alpha, Score beta, ThreadData& td, Stack* ss) {
	constexpr bool pvNode = searchType == PV;
	
	++td.nodeCount;
	
	assert(pvNode || beta - alpha == 1);
	assert(alpha < beta);
	
	if (time.stop) return STOP_SCORE;
	if (board.isDraw()) return DRAW_SCORE;
	
	if (ss->ply >= MAX_PLY) 
		return board.isInCheck() ? DRAW_SCORE : evaluate(board);
	
	// transposition table lookup
	Key key = board.key();
	TTEntry tte;
	bool ttHit = tt.probe(key, tte);
	Score ttScore = TranspositionTable::scoreFromTT(tte.score, ss->ply);
	
	if (!pvNode && 
		ttHit && (
		tte.nodeType == PV_NODE ||
		tte.nodeType == CUT_NODE && ttScore >= beta ||
		tte.nodeType == ALL_NODE && ttScore <= alpha))
		return ttScore;

	bool isInCheck = board.isInCheck();
	Score bestScore, staticEval;

	if (isInCheck) {

		if (ttHit) {
			staticEval = TranspositionTable::scoreFromTT(tte.eval, ss->ply);

			if (tte.nodeType == CUT_NODE && ttScore > staticEval ||
				tte.nodeType == ALL_NODE && ttScore < staticEval ||
				tte.nodeType == PV_NODE) {

				bestScore = staticEval = ttScore;
			}

			else bestScore = -INFINITY_SCORE;
		}

		else {
			staticEval = evaluate(board);
			bestScore = -INFINITY_SCORE;
		}
	}

	else {
		if (ttHit) {
			staticEval = TranspositionTable::scoreFromTT(tte.eval, ss->ply);

			if (tte.nodeType == CUT_NODE && ttScore > staticEval ||
				tte.nodeType == ALL_NODE && ttScore < staticEval ||
				tte.nodeType == PV_NODE)
				staticEval = ttScore;
		}

		else staticEval = evaluate(board);

		bestScore = staticEval;

		if (bestScore >= beta) {
			if (!ttHit)
				tt.save(key, TranspositionTable::scoreToTT(bestScore, ss->ply), 
					TranspositionTable::scoreToTT(staticEval, ss->ply), Move(), 0, CUT_NODE);
			return bestScore;
		}

		if (pvNode && bestScore > alpha)
			alpha = bestScore;
	}

	// hash move
	Move bestMove = ttHit ? tte.move : Move();
	MoveSorter moveSorter(board, ss, td.histories, bestMove, isInCheck);
	Score score;

	for (;;) {
		Move m = moveSorter.next();
		if (!m) break;

		bool isCapture = board.isCapture(m);
		bool isQueenPromotion = board.isPromotion(m) && move::pieceType(m) == QUEEN;

		if (!isInCheck &&
			!(isCapture || isQueenPromotion)) 
			continue;

		assert(board.isPseudoLegal(m));
		assert(board.isLegal(m));

		// prune bad captures
		if (isCapture && !isInCheck) {
			Score see = board.see(m);
			if (see < 0)
				continue;
		}

		ss->moved = board.pieceOn(move::from(m));
		ss->move = m;

		board.applyMove(m);
		score = -quiescence<searchType>(board, -beta, -alpha, td, ss+1);
		board.undoMove();
	
		if (score > bestScore) {
			bestScore = score;
	
			if (score > alpha) {
				bestMove = m;
	
				if (pvNode && score < beta) 
					alpha = score;

				else break;
			}
		}
	}
	
	// checkmate or stalemate
	if (!moveSorter.moves.size())
		return isInCheck ? -MATE_SCORE + ss->ply : DRAW_SCORE;
	
	NodeType nodeType = bestScore >= beta ? CUT_NODE :
		pvNode && bestMove ? PV_NODE : ALL_NODE;

	tt.save(key, TranspositionTable::scoreToTT(bestScore, ss->ply), 
		TranspositionTable::scoreToTT(staticEval, ss->ply), bestMove, 0, nodeType);
	
	return bestScore;
}

void Search::init() {
	using std::log;
	for (int d = 1; d < MAX_DEPTH; ++d)
		for (int m = 1; m < MAX_MOVE; ++m)
			lateMoveReductions[d][m] = 492*log(d)*log(m) + 83*log(m);
}

void Search::stop() {
	time.stop = true;
}

void Search::clear() {
	tt.clear();

	while (threadData.size() > 0)
		delete threadData.back(), threadData.pop_back();

	for (ThreadID i = 0; i < numThreads; ++i)
		threadData.push_back(new ThreadData(i));
}

void Search::setTTSize(size_t MiB) {
	tt.setSize(MiB);
}

void Search::setNumThreads(ThreadID numThreads) {
	this->numThreads = std::clamp(
		numThreads,
		(ThreadID)1,
		std::min(std::thread::hardware_concurrency(), (ThreadID)MAX_THREADS)
	);

	while (threadData.size() > 0)
		delete threadData.back(), threadData.pop_back();

	for (ThreadID i = 0; i < this->numThreads; ++i)
		threadData.push_back(new ThreadData(i));
}

std::string Search::info(ThreadData& td, Depth depth, Score score) {
	std::stringstream ss;

	ss << "info" <<
		" depth " << depth <<
		" seldepth " << td.selDepth;

	if (std::abs(score) < MIN_MATE_SCORE)
		ss << " score cp " << score;

	else
		ss << " score mate " << (MATE_SCORE - std::abs(score) + 1) / 2 * (score < 0 ? -1 : 1);

	TimeManagement::TimePoint elapsed = time.elapsed() + 1;

	uint64_t nodes = nodeCount();

	ss << " nodes " << nodes <<
		" nps " << nodes * 1000 / elapsed <<
		" time " << elapsed <<
		" hashfull " << tt.usage() * 1000;
	
	if (td.pv.size()) {
		ss << " pv";
		for (size_t i = 0; i < td.pv.size(); ++i)
			ss << " " << move::toString(td.pv[i]);
	}

	return ss.str();
}

uint64_t Search::nodeCount() {
	uint64_t sum = 0;
	for (const auto& td : threadData)
		sum += td->nodeCount;
	return sum;
}