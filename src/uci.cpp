#include<ostream>

#include"movegen.h"
#include"uci.h"

void uci::loop() {
	board = Board(START_FEN);
	search.setTTSize(DEFAULT_TT_SIZE);
	search.setNumThreads(DEFAULT_THREADS);

	std::string line, token;

  for (;;) {
		std::getline(std::cin, line);
		std::istringstream ss = std::istringstream(line);
		token.clear();
		ss >> token;

		if (token == "quit") {
			stop();
			break;
		}

		if (token == "stop")            stop();
		else if (token == "uci")        uci();
		else if (token == "ucinewgame") ucinewgame();
		else if (token == "setoption")  setoption(ss);
		else if (token == "isready")    std::cout << "readyok" << std::endl;
		else if (token == "position")   position(ss);
		else if (token == "go")         go(ss.str());

		// non-UCI commands
		else if (token == "d") std::cout << board;
		else if (token == "fen") std::cout << board.fen() << std::endl;
	}
}

void uci::uci() {
	std::cout << "id name Kobra" << std::endl;
	std::cout << "id author Jasper" << std::endl;
	std::cout << "option name Hash type spin default " << DEFAULT_TT_SIZE << " min 1 max " << MAX_TT_SIZE << std::endl;
	std::cout << "option name Threads type spin default " << DEFAULT_THREADS << " min 1 max " << MAX_THREADS << std::endl;
	std::cout << "uciok" << std::endl;
}

void uci::ucinewgame() {
	search.clear();
}

void uci::setoption(std::istringstream& ss) {
	std::string token;
	while (ss >> token) {
		if (token == "name") {
			ss >> token;

			if (token == "Hash") {
				ss >> token;
				if (token == "value") {
					uint64_t hashSize;
					ss >> hashSize;
					if (hashSize) search.tt.setSize(hashSize);
				}
			}

			else if (token == "Threads") {
				ss >> token;
				if (token == "value") {
					ss >> token;
					ThreadID count = static_cast<ThreadID>(std::stoi(token));
					search.setNumThreads(count);
				}
			}
		}
	}
}

void uci::position(std::istringstream& ss) {
	std::string token, fen;
	ss >> token;

	if (token == "startpos") {
		fen = START_FEN;
		ss >> token;
	}
	else if (token == "fen") {
		while (ss >> token && token != "moves")
			fen += token + " ";
	}
	else
		return;

	board = { fen };

  while (ss >> token) {
		Move move = uci::toMove(token, board);
		
		// invalid move
		if (!move) {
			std::cout << "Invalid move: " << token << std::endl;
			break;
		}

		board.applyMove(move);
	}
}

void uci::go(const std::string& str) {
	stop();

	search.time = {};
	search.time.start();

	std::istringstream ss(str);
	std::string token;

	while (ss >> token) {
		if (token == "wtime") ss >> search.time.time[WHITE];
		else if (token == "btime") ss >> search.time.time[BLACK];
		else if (token == "winc") ss >> search.time.inc[WHITE];
		else if (token == "binc") ss >> search.time.inc[BLACK];

		else if (token == "nodes") {
			search.time.useNodeLimit = true;
			ss >> search.time.nodeLimit;
		}

		else if (token == "depth") {
			search.time.useDepthLimit = true;
			ss >> search.time.depthLimit;
		}

		else if (token == "movetime") {
			search.time.useMoveTimeLimit = true;
			ss >> search.time.moveTimeLimit;
		}

		else if (token == "perft") {
			Depth depth;
			ss >> depth;

			std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

			uint64_t nodeCnt = perft(board, depth);

			std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

			std::cout << "Nodes " << nodeCnt << std::endl;
			std::cout << "Time " << (end - begin).count() * 1e-9 << std::endl;

			return;
		}
	}

	search.time.initializeTimeToUse(board.sideToMove);

	// start the search
	searchThread = std::thread(searchAndPrint);
}

void uci::stop() {
	search.stop();
	if (searchThread.joinable())
		searchThread.join();
}

Move uci::toMove(const std::string& str, Board& board) {
	MoveList moves;
	generateMoves(board, moves);
	for (auto& m : moves) {
		if (move::toString(m.move) == str)
			return m.move;
	}
	return Move();
}

void uci::searchAndPrint() {
	Move best = search.bestMove(board);
	std::cout << "bestmove " << move::toString(best) << std::endl;
}