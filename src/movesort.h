#pragma once
#include <array>
#include "main.h"
#include "movegen.h"

struct search_stack;

template<size_t Size,size_t...Sizes> struct hist:std::array<hist<Sizes...>,Size>{
  void fill(const hist_entry value){
    const auto e=reinterpret_cast<hist_entry*>(this);
    std::fill_n(e,sizeof(*this)/sizeof(hist_entry),value);
  }

  static constexpr hist_entry max=30000;

  static hist_entry p(const i32 d){
    return std::min(153*d-133,1525);
  }
};

template<size_t Size> struct hist<Size>:std::array<hist_entry,Size>{};

struct capture_hist:hist<n_pieces,n_sqs,n_piece_types>{
  static constexpr hist_entry heinc=7;
  static constexpr hist_entry hedec=8;
  void increase(const board& pos,u16 move,i32 depth);
  void decrease(const board& pos,u16 move,i32 depth);
};

struct butterfly_hist:hist<n_colors,n_sqs,n_sqs>{
  static constexpr hist_entry heinc=10;
  static constexpr hist_entry hedec=5;
  void increase(const board& pos,u16 move,i32 depth);
  void decrease(const board& pos,u16 move,i32 depth);
};

struct continuation_hist:hist<n_pieces,n_sqs,n_pieces,n_sqs>{
  static constexpr hist_entry heinc1=2;
  static constexpr hist_entry heinc2=2;
  static constexpr hist_entry heinc4=2;
  static constexpr hist_entry hedec1=2;
  static constexpr hist_entry hedec2=2;
  static constexpr hist_entry hedec4=2;
  void increase(const board& pos,const search_stack* stack,u16 move,i32 depth);
  void decrease(const board& pos,const search_stack* stack,u16 move,i32 depth);
};

struct history{
  butterfly_hist butterfly;
  capture_hist capture;
  continuation_hist continuation;
  static constexpr hist_entry butterfly_fill=-425;
  static constexpr hist_entry capture_fill=-970;
  static constexpr hist_entry continuation_fill=-470;
  u16 counter[n_pieces][n_sqs];
  u16 killer[max_depth+1][2];
  void clear();
  void update(const board& pos,const search_stack* stack,u16 best_move,
    move_list& moves,i32 depth);

  history(){
    clear();
  }
};

struct move_sort{
  enum stages : u8{
    hashmove,normal_init,normal
  };

  board& position;
  bool is_in_check;
  history& hist;
  int idx;
  int stage;
  move_list moves;
  search_stack* ss;
  u16 hash_move;
  u16 next();
  void compute_scores();
  move_sort(board& pos,search_stack* stack,history& hist,u16 move,
    bool in_check);
};
