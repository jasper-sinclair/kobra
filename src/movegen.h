#pragma once
#include <cstring>
#include <iostream>
#include "bitboard.h"

constexpr int max_moves=256;

struct move_info{
  u16 move;
  int score;

  bool operator==(const u16 mv) const{
    return this->move==mv;
  }
};

struct move_list{
  [[nodiscard]] move_info* end() const{
    return last;
  }

  [[nodiscard]] int score(const size_t idx) const{
    return data[idx].score;
  }

  [[nodiscard]] u16 move(const size_t idx) const{
    return data[idx].move;
  }

  [[nodiscard]] size_t size() const{
    return last-data;
  }

  move_info data[max_moves],* last;

  move_info* begin(){
    return data;
  }

  friend std::ostream& operator<<(std::ostream& os,const move_list& movelist);

  void add(const u16 move){
    last++->move=move;
  }

  void remove(size_t idx);
  move_list() : data{}, last(data){}
};

inline void move_list::remove(const size_t idx){
  std::memmove(begin()+idx,begin()+idx+1,
    (size()-idx-1)*sizeof(move_info));
  --last;
}

template<bool C> void gen_pawn_moves(board& pos,move_list& movelist);
template<bool C,i32 Pt> void gen_piece_moves(board& pos,move_list& movelist);
template<bool C> void gen_king_moves(board& pos,move_list& movelist);
void gen_moves(board& pos,move_list& movelist);

template<bool Root=true> u64 perft(board& pos,const int depth){
  u64 cnt,nodes=0;
  const bool leaf=depth==2;
  move_list moves;
  gen_moves(pos,moves);
  for(size_t i=0;i<moves.size();++i){
    if(Root&&depth<=1){
      cnt=1;
      ++nodes;
    } else{
      pos.apply_move(moves.move(i));
      move_list m;
      gen_moves(pos,m);
      cnt=leaf?m.size():perft<false>(pos,depth-1);
      nodes+=cnt;
      pos.undo_move();
    }
    if(Root){
      SO<<move::move_to_string(moves.move(i))<<" "<<cnt<<SE;
    }
  }
  return nodes;
}

inline std::ostream& operator<<(std::ostream& os,const move_list& movelist){
  for(int i=0;i<SCI(movelist.size());++i) os<<move::move_to_string(movelist.move(i))<<" ";
  return os;
}

inline void print(const std::vector<u16>& v){
  for(const auto& m:v)
    SO<<move::move_to_string(m)<<" "<<SE;
}
