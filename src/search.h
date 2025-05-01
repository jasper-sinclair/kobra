#pragma once
#include <thread>
#include "chrono.h"
#include "hash.h"
#include "movesort.h"

constexpr int min_display_time=5000;
constexpr int max_threads=256;
inline constexpr int continuation_ply=6;

enum search_type : u8{
  non_pv,node_pv,root
};

struct search_stack{
  i32 moved;
  int ply;
  int pv_size;
  int static_eval;
  u16 hash_move;
  u16 move;
  u16 pv[max_depth];
};

struct thread_data{
  explicit thread_data(const thread_id id) : root_depth(0), stack{}, id(id), node_count(0){}
  history histories;
  i32 root_depth;
  search_stack stack[max_ply+continuation_ply];
  std::vector<u16> pv;
  thread_data() : root_depth(0), stack{}, id(0), node_count(0){}
  thread_id id;
  u64 node_count;
};

struct search_info{
  [[nodiscard]] std::string info(const thread_data& td,i32 depth,int score) const;
  [[nodiscard]] u64 node_count() const;
  chrono time;
  constexpr static i16 lmr_factor=1000;
  hash_table hash;
  inline static i32 forward_pruning_table[max_depth][max_moves];
  inline static i32 log_reduction_table[max_depth][max_moves];
  inline static i32 move_count_pruning_table[max_depth];
  static void init();
  std::vector<std::thread> threads;
  std::vector<thread_data*> thread_info;
  template<bool MainThread=true> u16 best_move(board& pos,thread_id id=0);
  template<search_type St,bool SkipHashMove=false> int alpha_beta(board& pos,int alpha,int beta,i32 depth,
    thread_data& td,search_stack* ss);
  template<search_type St> int quiescence(board& pos,int alpha,int beta,thread_data& td,search_stack* ss);
  thread_id num_threads=1;
  void clear();
  void set_hash_size(size_t mb);
  void set_num_threads(thread_id threadnum);
  void stop();
};

extern template u16 search_info::best_move<true>(board& pos,thread_id id);
extern template u16 search_info::best_move<false>(board& pos,thread_id id);
