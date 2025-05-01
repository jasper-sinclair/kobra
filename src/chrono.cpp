#include "chrono.h"
#include <algorithm>
#include "main.h"
#include "uci.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif

chrono::chrono(){
  std::memset(this,0,sizeof(chrono));
}

time_point chrono::now(){
  return std::chrono::duration_cast<milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch())
    .count();
}

void chrono::start(){
  begin=now();
}

time_point chrono::elapsed() const{
  return now()-begin;
}

void chrono::init_time(const bool side_to_move){
  match_time_limit=time[side_to_move];
  if(match_time_limit){
    use_match_limit=true;
    constexpr time_point overhead=30;
    time_to_use=match_time_limit/20+inc[side_to_move];
    if(match_time_limit<2000){
      time_to_use=std::max(time_to_use,SCTP(100));
    } else{
      time_to_use=std::max(time_to_use,SCTP(500));
    }
    time_to_use-=overhead;
  } else{
    time_to_use=std::numeric_limits<int64_t>::max();
  }
}

void chrono::update(const u64 node_cnt){
  if(use_match_limit&&elapsed()>time_to_use||
    use_node_limit&&node_cnt>node_limit||
    use_move_limit&&elapsed()>move_time_limit){
    stop=true;
  }
}
