#include "hash.h"
#include <cstring>
#include <memory>
#include "main.h"

void hash_table::set_size(const u64 mb){
  const u64 bytes=mb*1024*1024;
  const u64 max_size=bytes/sizeof(hash_entry);
  size=1;
  for(;;){
    const u64 new_size=2*size;
    if(new_size>max_size) break;
    size=new_size;
  }
  mask=size-1;
  entries=std::make_unique<hash_entry[]>(size);
  clear();
}

void hash_table::clear() const{
  std::memset(entries.get(),0,size*sizeof(hash_entry));
}

bool hash_table::probe(const u64 key,hash_entry& entry){
  entry=*get(key);
  if((entry.key^entry.data_union.data)==key) return true;
  entry={};
  return false;
}

void hash_table::save(const u64 key,const int score,const int static_eval,const u16 move,
  const i32 depth,const node_type nt){
  hash_entry tmp;
  tmp.data_union.entry_data.score=score;
  tmp.data_union.entry_data.eval=static_eval;
  tmp.data_union.entry_data.move=move;
  tmp.data_union.entry_data.depth=SCU8(depth);
  tmp.data_union.entry_data.nt=nt;
  tmp.key=key^tmp.data_union.data;
  if(hash_entry* entry=get(key);nt==pvnode||
    key!=(entry->key^entry->data_union.data)||
    depth+4>entry->data_union.entry_data.depth){
    *entry=tmp;
  }
}

int hash_table::score_to_hash(const int score,const i32 ply){
  return score>=min_mate_score
    ?score+ply
    :score<=-min_mate_score
    ?score-ply
    :score;
}

int hash_table::score_from_hash(const int score,const i32 ply){
  return score>=min_mate_score
    ?score-ply
    :score<=-min_mate_score
    ?score+ply
    :score;
}
