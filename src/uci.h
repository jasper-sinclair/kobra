#pragma once
#include <thread>

#include "search.h"

namespace uci {
  constexpr size_t kDefaultHash = 256;
  constexpr ThreadId kDefaultThreads = 1;

  inline Board board;
  inline Search search;
  inline std::string evalfile = "kobra_1.2.nnue";
  inline std::thread thread;

  int loop();
  int position(std::istringstream& ss);
  int GetBestMove();
  int uci();

  Move ToMove(const std::string& str, Board& pos);

  void go(const std::string& str);
  void setoption(std::istringstream& ss);
  void stop();
  void ucinewgame();
}
