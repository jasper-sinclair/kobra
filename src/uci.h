#pragma once

#include <thread>

#include "search.h"

namespace uci {
constexpr size_t kDefaultHashSize = 256;
constexpr ThreadId kDefaultThreads = 1;

inline Board board;
inline Search search;
inline std::thread search_thread;

void loop();

void position(std::istringstream& ss);
void go(const std::string& str);
void ucinewgame();
void uci();
void setoption(std::istringstream& ss);
void stop();

uint16_t StrToMove(const std::string& str, Board& board);
void GetBestMove();
};  // namespace uci