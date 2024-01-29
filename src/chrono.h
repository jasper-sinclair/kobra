#pragma once
#include <chrono>

#include "main.h"

struct Chrono {
  using TimePoint = std::chrono::milliseconds::rep;
  bool stop;
  bool use_match_time_limit;
  bool use_node_limit;
  bool use_depth_limit;
  bool use_move_time_limit;

  uint64_t node_limit;
  Depth depth_limit;

  TimePoint begin;
  TimePoint time_to_use;
  TimePoint match_time_limit;
  TimePoint move_time_limit;
  TimePoint time[N_COLORS], inc[N_COLORS];

  Chrono();
  void start();
  static TimePoint now();
  void InitTimeToUse(Color side_to_move);
  void update(uint64_t node_cnt);
  [[nodiscard]] TimePoint elapsed() const;
};
