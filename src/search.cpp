#include "search.h"
#include <cassert>
#include <sstream>
#include "eval.h"
#include "hash.h"

template<bool MainThread> u16 search_info::best_move(board& pos,const thread_id id){
  if(MainThread){
    for(const auto& td:thread_info){
      td->pv.clear();
      td->node_count=0;
      td->root_depth=1;
    }
    for(thread_id i=1;i<num_threads;++i) threads.emplace_back(&search_info::best_move<false>,this,std::ref(pos),i);
  }
  thread_data& td=*thread_info[id];
  board copy(pos);
  int alpha;
  int beta;
  int score=0;
  int prev_score=0;
  for(int i=0;i<max_ply+continuation_ply;++i){
    td.stack[i].ply=i-continuation_ply;
    td.stack[i].moved=no_piece;
    td.stack[i].pv_size=0;
    td.stack[i].move=u16();
  }
  search_stack* ss=&td.stack[continuation_ply];
  *ss=search_stack();
  while(td.root_depth<max_depth){
    if(MainThread&&time.use_depth_limit&&td.root_depth>time.depth_limit) break;
    int delta=17;
    if(td.root_depth==1){
      alpha=-infinite_score;
      beta=infinite_score;
    } else{
      alpha=SCI(std::max(score-delta,-infinite_score));
      beta=SCI(std::min(score+delta,+infinite_score));
    }
    for(;;){
      score=alpha_beta<root>(copy,alpha,beta,td.root_depth,td,ss);
      if(time.stop) break;
      if(score<=alpha){
        beta=(alpha+beta)/2;
        alpha=SCI(std::max(alpha-delta,-infinite_score));
      } else if(score>=beta) beta=SCI(std::min(beta+delta,+infinite_score));
      else break;
      delta+=delta/3;
    }
    if(time.stop) break;
    prev_score=score;
    td.pv.clear();
    for(int i=0;i<ss->pv_size;++i) td.pv.push_back(ss->pv[i]);
    if(MainThread){
      SO<<info(td,td.root_depth,score)<<SE;
    }
    ++td.root_depth;
  }
  if(MainThread){
    stop();
    for(auto& t:threads) t.join();
    threads.clear();
    if(time.use_node_limit||time.use_move_limit){
      SO<<info(td,td.root_depth,prev_score)<<SE;
    }
    return td.pv[0];
  }
  return u16();
}

template u16 search_info::best_move<true>(board& pos,thread_id id);
template u16 search_info::best_move<false>(board& pos,thread_id id);

template<search_type St,bool SkipHashMove> int search_info::alpha_beta(board& pos,int alpha,int beta,i32 depth,
  thread_data& td,search_stack* ss){
  constexpr bool root_node=St==root;
  constexpr bool pv_node=St!=non_pv;
  if(const bool main_thread=td.id==0;main_thread&&depth>=5) time.update(node_count());
  if(time.stop) return stop_score;
  if(pos.is_draw()) return draw_score;
  if(depth<=0) return quiescence<pv_node?node_pv:non_pv>(pos,alpha,beta,td,ss);
  ++td.node_count;
  if(!root_node){
    alpha=SCI(
      std::max(-mate_score+ss->ply,alpha));
    beta=SCI(
      std::min(mate_score-ss->ply-1,beta));
    if(alpha>=beta) return alpha;
  }
  const u64 key=pos.key();
  hash_entry he;
  const bool hash_hit=hash.probe(key,he);
  const int hash_score=
    hash_table::score_from_hash(he.data_union.entry_data.score,ss->ply);
  if(!pv_node&&!SkipHashMove&&hash_hit&&depth<=he.data_union.entry_data.depth){
    if(he.data_union.entry_data.nt==pvnode||
      he.data_union.entry_data.nt==cutnode&&hash_score>=beta||
      he.data_union.entry_data.nt==allnode&&hash_score<=alpha){
      if(pos.st->fifty_move_count<90) return hash_score;
    }
  }
  const bool is_in_check=pos.is_in_check();
  int eval;
  if(is_in_check) eval=ss->static_eval=-infinite_score;
  else if(hash_hit){
    ss->static_eval=he.data_union.entry_data.eval;
    if(depth<=he.data_union.entry_data.depth&&
      (he.data_union.entry_data.nt==pvnode||
        he.data_union.entry_data.nt==cutnode&&hash_score>ss->static_eval||
        he.data_union.entry_data.nt==allnode&&hash_score<ss->static_eval)){
      eval=hash_score;
    } else eval=ss->static_eval;
  } else eval=ss->static_eval=eval::evaluate(pos);
  td.histories.killer[(ss+1)->ply][0]=
    td.histories.killer[(ss+1)->ply][1]=u16();
  if(!root_node&&!is_in_check){
    if(!pv_node&&depth<3&&eval>=beta+171*depth&&
      eval<min_mate_score)
      return eval;
    if(!pv_node&&!SkipHashMove&&pos.non_pawn_material(pos.side_to_move)&&
      beta>-min_mate_score&&eval>=ss->static_eval&&eval>=beta&&
      ss->static_eval>=beta-22*depth+400&&depth<12){
      const i32 r=(13+depth)/5;
      ss->move=u16();
      pos.apply_null_move();
      int null_score=-alpha_beta<non_pv>(
        pos,-beta,-alpha,
        depth-r,td,ss+1);
      pos.undo_null_move();
      if(null_score>=beta){
        if(null_score>=min_mate_score) null_score=beta;
        return null_score;
      }
    }
    if(pv_node&&!hash_hit) --depth;
    if(depth<=0) return quiescence<pv_node?node_pv:non_pv>(pos,alpha,beta,td,ss);
  }
  const u16 hash_move=hash_hit?he.data_union.entry_data.move:u16();
  move_sort move_sorter(pos,ss,td.histories,hash_move,is_in_check);
  int best_score=-infinite_score;
  int score=0;
  u16 best_move=u16();
  int move_count=0;
  for(;;){
    const u16 m=move_sorter.next();
    if(!m) break;
    if(pv_node) (ss+1)->pv_size=0;
    ++move_count;
    if(SkipHashMove&&m==ss->hash_move) continue;
    const bool is_capture=pos.is_capture(m);
    const u8 from=move::from(m);
    const u8 to=move::to(m);
    ss->moved=pos.piece_on(from);
    i32 new_depth=depth-1;
    if(!root_node&&std::abs(best_score)<min_mate_score&&
      pos.non_pawn_material(pos.side_to_move)){
      if(depth<3&&move_count>move_count_pruning_table[depth]) continue;
      if(!is_capture&&!is_in_check&&!pos.gives_check(m)){
        const int h_score=td.histories.butterfly[pos.side_to_move][from][to]/106+
          td.histories.continuation[(ss-1)->moved][move::to((ss-1)->move)]
          [ss->moved][to]/
          30+
          td.histories.continuation[(ss-2)->moved][move::to((ss-2)->move)]
          [ss->moved][to]/
          34+
          td.histories.continuation[(ss-4)->moved][move::to((ss-4)->move)]
          [ss->moved][to]/
          42;
        i32 pruning_depth=new_depth+
          (5*h_score-forward_pruning_table[depth][move_count]-506)/1000;
        pruning_depth=SCI32(std::max(+pruning_depth,-1));
        if(pruning_depth<2&&
          ss->static_eval<=alpha-200-172*pruning_depth)
          continue;
      }
    }
    ss->move=m;
    int ext=0;
    bool lmr=false;
    if(move_count==1){
      if(!root_node&&!SkipHashMove&&depth>=8&&m==hash_move&&
        (he.data_union.entry_data.nt==cutnode||he.data_union.entry_data.nt==pvnode)&&
        he.data_union.entry_data.depth+3>=depth&&std::abs(eval)<min_mate_score){
        const int singular_beta=
          std::min(eval-2*depth,beta);
        const i32 singular_depth=depth/2;
        ss->hash_move=m;
        score=
          alpha_beta<non_pv,true>(pos,singular_beta-1,
            singular_beta,singular_depth,td,ss);
        if(score<singular_beta) ext=1;
        else if(hash_score>=beta) ext=-1;
        new_depth+=ext;
      }
    } else{
      if(depth>=2&&!(pv_node&&is_capture)){
        lmr=true;
        ext=-log_reduction_table[depth][move_count];
        if(is_capture){
          const u8 cap_sq=
            move::mt(m)==move::en_passant
            ?to-SCU8(pawn_push(pos.side_to_move))
            :to;
          const i32 cap_pt=ptmake(pos.piece_on(cap_sq));
          const int h_score=td.histories.capture[ss->moved][cap_sq][cap_pt]/21+43;
          ext+=h_score;
        } else{
          const int h_score=td.histories.butterfly[pos.side_to_move][from][to]/106+
            td.histories.continuation[(ss-1)->moved][move::to(
              (ss-1)->move)][ss->moved][to]/
            30+
            td.histories.continuation[(ss-2)->moved][move::to(
              (ss-2)->move)][ss->moved][to]/
            34+
            td.histories.continuation[(ss-4)->moved][move::to(
              (ss-4)->move)][ss->moved][to]/
            42;
          ext+=h_score;
        }
        if(pv_node) ext+=1057+11026/(3+depth);
        if(hash_move&&pos.is_capture(hash_move)) ext+=-839;
        if(is_in_check) ext+=722;
        if(pos.gives_check(m)) ext+=796;
        if(m==td.histories.killer[ss->ply][0]) ext+=1523;
        else if(m==td.histories.killer[ss->ply][1]) ext+=1488;
      }
    }
    pos.apply_move(m);
    if(lmr){
      ext=
        SCI32(std::round(ext/SCDO(lmr_factor)));
      const i32 d=
        std::clamp(new_depth+ext,0,+new_depth+1);
      score=-alpha_beta<non_pv>(pos,-alpha-1,
        -alpha,d,td,ss+1);
      if(ext<0&&score>alpha)
        score=-alpha_beta<non_pv>(
          pos,-alpha-1,-alpha,
          new_depth,td,ss+1);
    } else if(!pv_node||move_count>1)
      score=-alpha_beta<non_pv>(pos,-alpha-1,
        -alpha,new_depth,td,ss+1);
    if(pv_node&&
      (move_count==1||(score>alpha&&(root_node||score<beta))))
      score=-alpha_beta<node_pv>(pos,-beta,
        -alpha,new_depth,td,ss+1);
    pos.undo_move();
    if(time.stop) return stop_score;
    if(score>best_score){
      best_score=score;
      if(pv_node){
        ss->pv[0]=m;
        std::memcpy(ss->pv+1,(ss+1)->pv,(ss+1)->pv_size*sizeof(u16));
        ss->pv_size=(ss+1)->pv_size+1;
      }
      if(score>alpha){
        best_move=m;
        if(score<beta) alpha=score;
        else break;
      }
    }
  }
  if(SkipHashMove&&move_count==1) return alpha;
  if(!move_count) return is_in_check?-mate_score+ss->ply:draw_score;
  if(!SkipHashMove){
    const node_type nt=best_score>=beta
      ?cutnode
      :pv_node&&best_move
      ?pvnode
      :allnode;
    if(best_move) td.histories.update(pos,ss,best_move,move_sorter.moves,depth);
    hash.save(key,hash_table::score_to_hash(best_score,ss->ply),
      ss->static_eval,best_move,depth,nt);
  }
  return best_score;
}

template<search_type St> int search_info::quiescence(board& pos,int alpha,const int beta,thread_data& td,
  search_stack* ss){
  constexpr bool pv_node=St==node_pv;
  ++td.node_count;
  if(time.stop) return stop_score;
  if(pos.is_draw()) return draw_score;
  if(ss->ply>=max_ply) return pos.is_in_check()?draw_score:eval::evaluate(pos);
  const u64 key=pos.key();
  hash_entry he;
  const bool hash_hit=hash.probe(key,he);
  const int hash_score=hash_table::score_from_hash(he.data_union.entry_data.score,ss->ply);
  if(!pv_node&&hash_hit&&
    (he.data_union.entry_data.nt==pvnode||he.data_union.entry_data.nt==cutnode&&hash_score>=beta||
      he.data_union.entry_data.nt==allnode&&hash_score<=alpha))
    return hash_score;
  const bool is_in_check=pos.is_in_check();
  int best_score;
  if(is_in_check) best_score=ss->static_eval=-infinite_score;
  else best_score=ss->static_eval=hash_hit?he.data_union.entry_data.eval:eval::evaluate(pos);
  if(hash_hit&&(he.data_union.entry_data.nt==pvnode||
    he.data_union.entry_data.nt==cutnode&&hash_score>best_score||
    he.data_union.entry_data.nt==allnode&&hash_score<best_score))
    best_score=hash_score;
  if(!is_in_check){
    if(best_score>=beta){
      if(!hash_hit)
        hash.save(key,
          hash_table::score_to_hash(best_score,
            ss->ply),
          ss->static_eval,u16(),0,cutnode);
      return best_score;
    }
    if(pv_node&&best_score>alpha) alpha=best_score;
  }
  u16 best_move=hash_hit?he.data_union.entry_data.move:u16();
  move_sort move_sorter(pos,ss,td.histories,best_move,is_in_check);
  for(;;){
    const u16 m=move_sorter.next();
    if(!m) break;
    const bool is_capture=pos.is_capture(m);
    if(const bool is_queen_promotion=
      board::is_promotion(m)&&move::get_piece_type(m)==queen;!is_in_check&&!(is_capture||is_queen_promotion))
      continue;
    if(is_capture&&!is_in_check){
      if(const int see=pos.see(m);see<eval::pt_values[knight]-eval::pt_values[bishop]) continue;
    }
    ss->moved=pos.piece_on(move::from(m));
    ss->move=m;
    pos.apply_move(m);
    const int score=-quiescence<St>(pos,-beta,-alpha,td,ss+1);
    pos.undo_move();
    if(score>best_score){
      best_score=score;
      if(score>alpha){
        best_move=m;
        if(pv_node&&score<beta) alpha=score;
        else break;
      }
    }
  }
  if(!move_sorter.moves.size()) return is_in_check?-mate_score+ss->ply:draw_score;
  const node_type nt=best_score>=beta
    ?cutnode
    :pv_node&&best_move
    ?pvnode
    :allnode;
  hash.save(key,hash_table::score_to_hash(best_score,ss->ply),
    ss->static_eval,best_move,0,nt);
  return best_score;
}

std::string search_info::info(const thread_data& td,const i32 depth,
  const int score) const{
  std::stringstream ss;
  ss<<"info"<<" depth "<<depth;
  const time_point elapsed=time.elapsed()+1;
  const u64 nodes=node_count();
  ss<<" nodes "<<nodes<<" time "<<elapsed<<" nps "<<nodes*1000/elapsed;
  if(std::abs(score)<min_mate_score) ss<<" score cp "<<score;
  else
    ss<<" score mate "
      <<(mate_score-std::abs(score)+1)/2*(score<0?-1:1);
  if(!td.pv.empty()){
    ss<<" pv";
    for(const unsigned short i:td.pv) ss<<" "<<move::move_to_string(i);
  }
  return ss.str();
}

void search_info::init(){
  for(int d=1;d<max_depth;++d)
    for(int m=1;m<max_moves;++m)
      log_reduction_table[d][m]=SCI32(
        492*log(d)*log(m)+83*log(m)+42*log(d)+1126);
  move_count_pruning_table[1]=4;
  move_count_pruning_table[2]=11;
  for(int d=1;d<max_depth;++d)
    for(int m=1;m<max_moves;++m)
      forward_pruning_table[d][m]=
        SCI32(410*log(d)*log(m)+69*log(m)+35*log(d));
}

void search_info::stop(){
  time.stop=true;
}

void search_info::set_hash_size(const size_t mb){
  hash.set_size(mb);
}

u64 search_info::node_count() const{
  u64 sum=0;
  for(const auto& td:thread_info) sum+=td->node_count;
  return sum;
}

void search_info::clear(){
  hash.clear();
  while(!thread_info.empty()){
    delete thread_info.back();
    thread_info.pop_back();
  }
  for(thread_id i=0;i<num_threads;++i) thread_info.push_back(new thread_data(i));
}

void search_info::set_num_threads(const thread_id threadnum){
  this->num_threads=std::clamp(threadnum, SCTI(1),
    std::min(std::thread::hardware_concurrency(),
      SCTI(max_threads)));
  while(!thread_info.empty()){
    delete thread_info.back();
    thread_info.pop_back();
  }
  for(thread_id i=0;i<this->num_threads;++i) thread_info.push_back(new thread_data(i));
}
