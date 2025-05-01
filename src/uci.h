#pragma once
#include <mutex>
#include <thread>
#include "search.h"

struct option{
  std::string name;
  std::string type;
  int default_value;
  int min_value;
  int max_value;
};

namespace uci{
  inline bool use_nnue=true;
  constexpr int default_contempt=1;
  constexpr size_t default_hash=256;
  constexpr thread_id default_threads=1;
  inline board pos;
  inline const std::string start_fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  inline int contempt=default_contempt;
  inline search_info search;
  inline std::jthread thread;
  inline std::mutex search_mutex;
  inline std::vector<option> ucioptions={
  {.name="Hash",.type="spin",.default_value=default_hash,.min_value=1,.max_value=max_hash_size},
  {.name="Threads",.type="spin",.default_value=default_threads,.min_value=1,.max_value=max_threads},
  {.name="Contempt",.type="spin",.default_value=default_contempt,.min_value=-100,.max_value=100},
  {.name="UseNNUE",.type="check",.default_value=true,.min_value=0,.max_value=1}
  };
  u16 to_move(const std::string& str,board& b);
  void get_bestmove();
  void go(const std::string& str);
  void info();
  void init();
  void loop();
  void newgame();
  void perft(std::istringstream& ss);
  void position(std::istringstream& ss);
  void setoption(std::istringstream& ss);
  void stop();
}

inline std::string engname="kobra";
inline std::string version="2.0";
inline std::string author="jasper";
