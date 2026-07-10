#pragma once
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include "search.h"

constexpr auto engine_name = "kobra";
constexpr auto engine_version = "2.2";
constexpr auto engine_author = "Jasper";

enum class option_type : uint8_t{
  spin, check
};

struct uci_option{
  std::string name;
  option_type type;
  int min = 0;
  int max = 0;
  int spin_value = 0;
  bool check_value = false;

  static uci_option spin(
    const std::string& name,
    const int def,
    const int min,
    const int max){
    uci_option o;
    o.name = name;
    o.type = option_type::spin;
    o.spin_value = def;
    o.min = min;
    o.max = max;
    return o;
  }

  static uci_option check(
    const std::string& name,
    const bool def){
    uci_option o;
    o.name = name;
    o.type = option_type::check;
    o.check_value = def;
    return o;
  }
};

namespace uci{
  constexpr int default_contempt = 1;
  constexpr size_t default_hash = 256;
  constexpr thread_id default_threads = 1;
  extern bool use_nnue;
  extern bool verbose;
  extern board pos;
  extern std::string start_fen;
  extern int contempt;
  extern search search_info;
  extern std::jthread thread;
  extern std::mutex search_mutex;
  extern std::unordered_map<std::string, uci_option> options;
  extern std::vector<std::string> option_order;

  void apply_option(
    const std::string& name);

  u16 to_move(
    const std::string& str,
    board& b);

  void get_bestmove();

  void info();

  void init();

  void loop();

  void newgame();

  void go(
    std::istringstream& ss);

  void perft(
    std::istringstream& ss);

  void bench(
    std::istringstream& ss);

  void position(
    std::istringstream& ss);

  void setoption(
    std::istringstream& ss);

  void stop();
}
