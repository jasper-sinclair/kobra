#pragma once
#include <memory>
#include "main.h"

enum node : u8{
  none_node,pvnode,cutnode,allnode
};

struct hash_entry{
  u64 key;

  union data_union{
    struct entry_data{
      i32 depth;
      int eval;
      int score;
      node_type nt;
      u16 move;
    } entry_data;

    u64 data;
  } data_union;
};

constexpr size_t max_hash_size=1<<20;

struct hash_table{
  hash_entry* get(const u64 key){
    return &entries[key&mask];
  }

  bool probe(u64 key,hash_entry& entry);
  static int score_from_hash(int score,i32 ply);
  static int score_to_hash(int score,i32 ply);
  std::unique_ptr<hash_entry[]> entries;
  u64 mask=0;
  u64 size=0;
  void clear() const;
  void save(u64 key,int score,int static_eval,u16 move,i32 depth,
    node_type nt);
  void set_size(u64 mb);
};
