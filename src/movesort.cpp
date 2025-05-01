#include "movesort.h"
#include <algorithm>
#include "bitboard.h"
#include "eval.h"
#include "search.h"

move_sort::move_sort(board& pos,search_stack* stack,history& hist,
  const u16 move,const bool in_check) : position(pos),
is_in_check(in_check),
hist(hist),
idx{},
stage(hashmove),
ss(stack),
hash_move(move){}

u16 move_sort::next(){
  switch(stage){
  case hashmove: ++stage;
    if(position.is_pseudo_legal(hash_move)&&position.is_legal(hash_move)) return hash_move;
    [[fallthrough]];
  case normal_init: ++stage;
    gen_moves(position,moves);
    for(size_t i=0;i<moves.size();++i){
      if(moves.move(i)==hash_move){
        moves.remove(i);
        break;
      }
    }
    compute_scores();
    std::ranges::sort(moves,[](const move_info& m1,const move_info& m2){
      return m1.score>m2.score;
    });
    [[fallthrough]];
  case normal: if(SCSZ(idx)==moves.size()) return u16();
    return moves.move(idx++);
  default:;
  }
  return 0;
}

void move_sort::compute_scores(){
  const bool us=position.side_to_move;
  const bool them=!us;
  const bitboard threatened_by_pawn=position.atts_by<pawn>(them);
  const bitboard threatened_by_minor=threatened_by_pawn|
    position.atts_by<knight>(them)|
    position.atts_by<bishop>(them);
  const bitboard threatened_by_rook=
    threatened_by_minor|position.atts_by<rook>(them);
  const bitboard threatened_pieces=
    position.get_color(us)&
    (threatened_by_pawn&(position.get_pieces(knight)|position.get_pieces(bishop))|
      threatened_by_minor&position.get_pieces(rook)|
      threatened_by_rook&position.get_pieces(queen));
  for(auto& [move, score]:moves){
    const u16 m=move;
    int s=0;
    if(position.is_capture(m)){
      const int see=position.see(m);
      s+=see>=0?1000000:-1000000;
      const i32 moved=position.piece_on(move::from(m));
      const u8 to=
        move::mt(m)==move::en_passant
        ?move::to(m)-
        SCU8(pawn_push(position.side_to_move))
        :move::to(m);
      const i32 captured=ptmake(position.piece_on(to));
      s+=89*eval::piece_values[position.piece_on(move::to(m))]+
        hist.capture[moved][to][captured];
    } else{
      if(m==hist.killer[ss->ply][0]) s+=500004;
      else if(m==hist.killer[ss->ply][1]) s+=500003;
      else if(ss->ply>=2&&m==hist.killer[ss->ply-2][0]) s+=500002;
      else{
        if(m==hist.counter[(ss-1)->moved][move::to((ss-1)->move)]) s+=500001;
        else{
          const u8 from=move::from(m);
          const u8 to=move::to(m);
          s+=hist.butterfly[position.side_to_move][from][to]/155+
            hist.continuation[(ss-1)->moved][move::to((ss-1)->move)]
            [position.piece_on(from)][to]/
            52+
            hist.continuation[(ss-2)->moved][move::to((ss-2)->move)]
            [position.piece_on(from)][to]/
            61+
            hist.continuation[(ss-4)->moved][move::to((ss-4)->move)]
            [position.piece_on(from)][to]/
            64;
          if(threatened_pieces.is_set(from)){
            const i32 pt=ptmake(position.piece_on(from));
            const bool is_safe=
              (pt==knight||pt==bishop)&&!threatened_by_pawn.is_set(to)||
              (pt==rook&&!threatened_by_minor.is_set(to))||
              (pt==queen&&!threatened_by_rook.is_set(to));
            if(is_safe){
              s+=561;
            }
          }
        }
      }
    }
    score=s;
  }
}

void butterfly_hist::increase(const board& pos,const u16 move,
  const i32 depth){
  auto& e=data()[pos.side_to_move][move::from(move)][move::to(move)];
  e+=heinc*p(depth)*(max-e)/max;
}

void butterfly_hist::decrease(const board& pos,const u16 move,
  const i32 depth){
  auto& e=data()[pos.side_to_move][move::from(move)][move::to(move)];
  e-=hedec*p(depth)*(max+e)/max;
}

void capture_hist::increase(const board& pos,const u16 move,
  const i32 depth){
  const i32 moved=pos.piece_on(move::from(move));
  const u8 to=
    move::mt(move)==move::en_passant
    ?move::to(move)-
    SCU8(pawn_push(pos.side_to_move))
    :move::to(move);
  const i32 captured=ptmake(pos.piece_on(to));
  auto& e=data()[moved][to][captured];
  e+=heinc*p(depth)*(max-e)/max;
}

void capture_hist::decrease(const board& pos,const u16 move,
  const i32 depth){
  const i32 moved=pos.piece_on(move::from(move));
  const u8 to=
    move::mt(move)==move::en_passant
    ?move::to(move)-
    SCU8(pawn_push(pos.side_to_move))
    :move::to(move);
  const i32 captured=ptmake(pos.piece_on(to));
  auto& e=data()[moved][to][captured];
  e-=hedec*p(depth)*(max+e)/max;
}

void continuation_hist::increase(const board& pos,const search_stack* stack,
  const u16 move,const i32 depth){
  if((stack-1)->move){
    auto& e=data()[(stack-1)->moved][move::to((stack-1)->move)]
      [pos.piece_on(move::from(move))][move::to(move)];
    e+=heinc1*p(depth)*(max-e)/max;
    if((stack-2)->move){
      e=data()[(stack-2)->moved][move::to((stack-2)->move)]
        [pos.piece_on(move::from(move))][move::to(move)];
      e+=heinc2*p(depth)*(max-e)/max;
      if((stack-4)->move){
        e=data()[(stack-4)->moved][move::to((stack-4)->move)]
          [pos.piece_on(move::from(move))][move::to(move)];
        e+=heinc4*p(depth)*(max-e)/max;
      }
    }
  }
}

void continuation_hist::decrease(const board& pos,const search_stack* stack,
  const u16 move,const i32 depth){
  if((stack-1)->move){
    auto& e=data()[(stack-1)->moved][move::to((stack-1)->move)]
      [pos.piece_on(move::from(move))][move::to(move)];
    e-=hedec1*p(depth)*(max+e)/max;
    if((stack-2)->move){
      e=data()[(stack-2)->moved][move::to((stack-2)->move)]
        [pos.piece_on(move::from(move))][move::to(move)];
      e-=hedec2*p(depth)*(max+e)/max;
      if((stack-4)->move){
        e=data()[(stack-4)->moved][move::to((stack-4)->move)]
          [pos.piece_on(move::from(move))][move::to(move)];
        e-=hedec4*p(depth)*(max+e)/max;
      }
    }
  }
}

void history::update(const board& pos,const search_stack* stack,const u16 best_move,
  move_list& moves,const i32 depth){
  if(pos.is_capture(best_move)){
    capture.increase(pos,best_move,depth);
    for(const auto& [move, score]:moves){
      if(move!=best_move&&pos.is_capture(move)){
        capture.decrease(pos,move,depth);
      }
    }
  } else{
    if(best_move!=killer[stack->ply][0]){
      killer[stack->ply][1]=killer[stack->ply][0];
      killer[stack->ply][0]=best_move;
    }
    if((stack-1)->move) counter[(stack-1)->moved][move::to((stack-1)->move)]=best_move;
    butterfly.increase(pos,best_move,depth);
    continuation.increase(pos,stack,best_move,depth);
    for(const auto& [move, score]:moves){
      if(move!=best_move){
        if(pos.is_capture(move)){
          capture.decrease(pos,move,depth);
        } else{
          butterfly.decrease(pos,move,depth);
          continuation.decrease(pos,stack,move,depth);
        }
      }
    }
  }
}

void history::clear(){
  std::memset(killer,0,sizeof(killer));
  std::memset(counter,0,sizeof(counter));
  butterfly.fill(butterfly_fill);
  capture.fill(capture_fill);
  continuation.fill(continuation_fill);
}
