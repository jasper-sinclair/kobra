#include "uci.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include "movegen.h"
#include "nnue.h"

void uci::init(){
  use_nnue=true;
  (void)nnue::instance();
  pos=board(start_fen);
  search.set_hash_size(default_hash);
  search.set_num_threads(default_threads);
  loop();
}

void uci::loop(){
  std::string line,token;
  std::unordered_map<std::string,std::function<void(std::istringstream&)>> commands={

  {"uci",[](std::istringstream&) {info(); }},
  {"ucinewgame",[](std::istringstream&) {newgame(); }},
  {"setoption",[](std::istringstream& ss) {setoption(ss); }},
  {"isready",[](std::istringstream&) {SO<<"readyok"<<NL; }},
  {"position",[](std::istringstream& ss) {position(ss); }},
  {"go",[](const std::istringstream& ss) {go(ss.str()); }},
  {"stop",[](std::istringstream&) {stop(); }},
  {"quit",[](std::istringstream&) {stop(); exit(0); }},
  {"print",[](std::istringstream&) {SO << pos << NL << pos.fen() << NL; }},
  {"perft",perft}};

  while(std::getline(std::cin,line)){
	    std::istringstream ss(line);
    ss>>token;
    if(auto it=commands.find(token);it!=commands.end()){it->second(ss);}
  }
}

void uci::info(){
  SO<<"id name "<<engname<<" "<<version<<NL;
  SO<<"id author "<<author<<NL;
  for(const auto& [name, type, default_value, min_value, max_value]:ucioptions){
    SO<<"option name "<<name<<" type "<<type<<" default "<<default_value<<" min "<<min_value<<" max "<<max_value<<NL;
  }
  SO<<"uciok"<<SE;
}

void uci::newgame(){
  search.clear();
}

void uci::setoption(std::istringstream& ss){
  std::string token,name,value;
  bool reading_name=false;
  bool reading_value=false;
  while(ss>>token){
    if(token=="name"){
      reading_name=true;
      reading_value=false;
    } else if(token=="value"){
      reading_name=false;
      reading_value=true;
    } else if(reading_name){
      if(!name.empty()) name+=" ";
      name+=token;
    } else if(reading_value){
      if(!value.empty()) value+=" ";
      value+=token;
    }
  }
  if(name=="Hash"){
    const u64 hash_size=std::stoull(value);
    search.hash.set_size(hash_size);
  } else if(name=="Threads"){
    search.set_num_threads(std::stoi(value));
  } else if(name=="Contempt"){
    contempt=std::stoi(value);
  } else if(name=="UseNNUE"){
    use_nnue=value=="true"||value=="1";
    SO<<"Set UseNNUE to "<<(use_nnue?"true":"false")<<SE;
  } else{
    SO<<"Unknown option: "<<name<<" with value: "<<value<<SE;
  }
}

void uci::position(std::istringstream& ss){
  std::string token,fen;
  ss>>token;
  if(token=="startpos"){
    fen=start_fen;
    ss>>token;
  } else if(token=="fen"){
    while(ss>>token&&token!="moves") fen+=token+" ";
  } else return;
  pos=board(fen);
  while(ss>>token){
    if(const u16 move=to_move(token,pos);move){
      pos.apply_move(move);
    } else{
      std::cerr<<"Invalid move: "<<token<<NL;
      break;
    }
  }
}

void uci::go(const std::string& str){
  std::scoped_lock lock(search_mutex);
  stop();
  search.time={};
  search.time.start();
  std::istringstream ss(str);
  std::string token;
  while(ss>>token){
    if(token=="wtime") ss>>search.time.time[white];
    else if(token=="btime") ss>>search.time.time[black];
    else if(token=="winc") ss>>search.time.inc[white];
    else if(token=="binc") ss>>search.time.inc[black];
    else if(token=="nodes"){
      search.time.use_node_limit=true;
      ss>>search.time.node_limit;
    } else if(token=="depth"){
      search.time.use_depth_limit=true;
      ss>>search.time.depth_limit;
    } else if(token=="movetime"){
      search.time.use_move_limit=true;
      ss>>search.time.move_time_limit;
    }
  }
  search.time.init_time(pos.side_to_move);
  thread=std::jthread(get_bestmove);
}

void uci::stop(){
  search.stop();
  if(thread.joinable()) thread.join();
}

void uci::perft(std::istringstream& ss){
  i32 depth;
  ss>>depth;
  const auto begin=std::chrono::steady_clock::now();
  const auto node_cnt=perft(pos,depth);
  const auto end=std::chrono::steady_clock::now();
  SO<<"node "<<node_cnt<<NL;
  SO<<"time "<<std::chrono::duration<double>(end-begin).count()<<NL;
}

u16 uci::to_move(const std::string& str,board& b){
  move_list moves;
  gen_moves(b,moves);
  for(const auto& [move, score]:moves){
    if(move::move_to_string(move)==str) return move;
  }
  return 0;
}

void uci::get_bestmove(){
  const u16 move=search.best_move(pos);
  SO<<"bestmove "<<move::move_to_string(move)<<SE;
}
