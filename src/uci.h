#pragma once

#include<thread>

#include"search.h"

namespace uci {
	constexpr size_t DEFAULT_TT_SIZE = 256;
	constexpr ThreadID DEFAULT_THREADS = 1;

	inline Board board;
	inline Search search;
	inline std::thread searchThread;

	void loop();

	void position(std::istringstream& ss);
	void go(const std::string& str);
	void ucinewgame();
	void uci();
	void setoption(std::istringstream& ss);
	void stop();

	Move toMove(const std::string& str, Board& board);
	void searchAndPrint();
};