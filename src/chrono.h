#pragma once
#include <chrono>
#include "main.h"

struct chrono{
  bool stop;
  bool use_depth_limit;
  bool use_match_limit;
  bool use_move_limit;
  bool use_node_limit;

  chrono();
  i32 depth_limit;

  time_point begin;
  time_point match_time_limit;
  time_point move_time_limit;
  time_point time_to_use;
  time_point time[n_colors],inc[n_colors];
  [[nodiscard]] time_point elapsed() const;
  static time_point now();
  u64 node_limit;

  void init_time(bool side_to_move);
  void start();
  void update(u64 node_cnt);
};
