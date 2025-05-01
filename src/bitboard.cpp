#include "bitboard.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include "attack.h"
#include "eval.h"

board::board(const std::string& fen){
  board_status.reserve(256);
  board_status.clear();
  board_status.emplace_back();
  st=get_board_status();
  std::istringstream ss(fen);
  std::string token;
  ss>>token;
  u8 sq=a8;
  i32 pc;
  for(const char c:token){
    if(c==' ') continue;
    if((pc=SCI32(piece_to_char.find(c)))!=SCI(std::string::npos)){
      set_piece<false>(pc,sq);
      sq+=east;
    } else if(isdigit(c)){
      sq+=(c-'0')*east;
    } else if(c=='/'){
      sq+=SCU8(2*south);
    }
  }
  ss>>token;
  side_to_move=token=="w"?white:black;
  ss>>token;
  for(const char c:token){
    switch(c){
    case 'K': st->castles.set(white_ks);
      break;
    case 'Q': st->castles.set(white_qs);
      break;
    case 'k': st->castles.set(black_ks);
      break;
    case 'q': st->castles.set(black_qs);
      break;
    default: break;
    }
  }
  ss>>token;
  st->ep_sq=token=="-"?no_sq:make(token);
  ss>>st->fifty_move_count;
  ss>>st->ply_count;
  st->ply_count=2*(st->ply_count-1)+side_to_move;
  st->zobrist=42;
}

board::board(const board& other){
  *this=other;
}

board& board::operator=(const board& other){
  if(this==&other) return *this;
  std::memcpy(pos,other.pos,n_sqs*sizeof(i32));
  std::memcpy(piece_bb,other.piece_bb,n_piece_types*sizeof(bitboard));
  std::memcpy(color_bb,other.color_bb,n_colors*sizeof(bitboard));
  occupied_bb=other.occupied_bb;
  side_to_move=other.side_to_move;
  board_status=other.board_status;
  st=get_board_status();
  return *this;
}

std::ostream& operator<<(std::ostream& os,const board& pos){
  const std::string sp=" ";
  for(i8 r=rank_8;r>=rank_1;--r){
    os<<'\n';
    for(i8 f=file_a;f<=file_h;++f){
      const i32 pc=pos.piece_on(make(f,r));
      char c=piece_to_char[pc];
      if(c==' ') c='.';
      std::string str=c+sp;
      os<<str;
    }
    os<<sp;
  }
  os<<'\n';
  return os;
}

std::string board::fen() const{
  std::stringstream ss;
  for(i8 r=rank_8;r>=rank_1;--r){
    int empty_count=0;
    for(i8 f=file_a;f<=file_h;++f){
      if(const i32 pc=piece_on(make(f,r))){
        if(empty_count) ss<<empty_count;
        ss<<piece_to_char[pc];
        empty_count=0;
      } else ++empty_count;
    }
    if(empty_count) ss<<empty_count;
    if(r!=rank_1) ss<<'/';
  }
  ss<<' '<<(side_to_move==white?'w':'b');
  ss<<' ';
  if(can_castle(white_ks)) ss<<'K';
  if(can_castle(white_qs)) ss<<'Q';
  if(can_castle(black_ks)) ss<<'k';
  if(can_castle(black_qs)) ss<<'q';
  if(cannot_castle()) ss<<'-';
  ss<<' '
    <<(st->ep_sq==no_sq?"-":sq_to_string(st->ep_sq));
  ss<<' '<<st->fifty_move_count;
  ss<<' '<<1+(st->ply_count-side_to_move)/2;
  return ss.str();
}

template<bool UpdateZobrist> void board::set_piece(const i32 pc,const u8 sq){
  pos[sq]=pc;
  piece_bb[ptmake(pc)].set(sq);
  color_bb[make(pc)].set(sq);
  occupied_bb.set(sq);
  if(UpdateZobrist) st->zobrist^=zobrist::psq[pc][sq];
}

template<bool UpdateZobrist> void board::remove_piece(const u8 sq){
  if(UpdateZobrist) st->zobrist^=zobrist::psq[piece_on(sq)][sq];
  piece_bb[ptmake(piece_on(sq))].clear(sq);
  color_bb[make(piece_on(sq))].clear(sq);
  pos[sq]=no_piece;
  occupied_bb.clear(sq);
}

template<bool UpdateZobrist> void board::move_piece(const u8 from,const u8 to){
  set_piece<UpdateZobrist>(piece_on(from),to);
  remove_piece<UpdateZobrist>(from);
}

void board::apply_move(const u16 m){
  const u8 from=move::from(m);
  const u8 to=move::to(m);
  const move::move_type mt=move::mt(m);
  i32 pc=piece_on(from);
  const i32 pt=ptmake(pc);
  const bool us=side_to_move;
  const bool them=!us;
  const i32 push=pawn_push(us);
  board_state bs;
  bs.ply_count=st->ply_count+1;
  bs.fifty_move_count=st->fifty_move_count+1;
  bs.castles=st->castles;
  bs.zobrist=st->zobrist;
  bs.ep_sq=no_sq;
  bs.captured=
    mt==move::en_passant?pmake(them,pawn):piece_on(to);
  bs.move=m;
  bs.king_attacinfo.computed=false;
  if(st->ep_sq){
    st->zobrist^=zobrist::en_passant[fmake(st->ep_sq)];
  }
  if(bs.captured){
    bs.fifty_move_count=0;
    u8 capsq=to;
    if(mt==move::en_passant) capsq-=SCU8(push);
    remove_piece(capsq);
    if(bs.captured==pmake(them,rook)){
      if(to==relative(us,a8)){
        st->zobrist^=zobrist::castle[bs.castles.data];
        bs.castles.reset(us?white_qs:black_qs);
        st->zobrist^=zobrist::castle[bs.castles.data];
      } else if(to==relative(us,h8)){
        st->zobrist^=zobrist::castle[bs.castles.data];
        bs.castles.reset(us?white_ks:black_ks);
        st->zobrist^=zobrist::castle[bs.castles.data];
      }
    }
  }
  if(pt==pawn){
    bs.fifty_move_count=0;
    if(to-from==2*push){
      bs.ep_sq=SCU8(to-push);
      st->zobrist^=zobrist::en_passant[fmake(bs.ep_sq)];
    }
  } else if(pt==king){
    st->zobrist^=zobrist::castle[bs.castles.data];
    bs.castles.reset(us?black_castle:white_castle);
    st->zobrist^=zobrist::castle[bs.castles.data];
    if(mt==move::castle){
      if(to==relative(us,c1)){
        const u8 rfrom=relative(us,a1);
        const u8 rto=relative(us,d1);
        move_piece(rfrom,rto);
      } else if(to==relative(us,g1)){
        const u8 rfrom=relative(us,h1);
        const u8 rto=relative(us,f1);
        move_piece(rfrom,rto);
      }
    }
  } else if(pt==rook){
    if(from==relative(us,a1)){
      st->zobrist^=zobrist::castle[bs.castles.data];
      bs.castles.reset(us?black_qs:white_qs);
      st->zobrist^=zobrist::castle[bs.castles.data];
    } else if(from==relative(us,h1)){
      st->zobrist^=zobrist::castle[bs.castles.data];
      bs.castles.reset(us?black_ks:white_ks);
      st->zobrist^=zobrist::castle[bs.castles.data];
    }
  }
  if(mt==move::promotion){
    remove_piece(from);
    pc=pmake(us,move::get_piece_type(m));
    set_piece(pc,to);
  } else{
    move_piece(from,to);
  }
  st->zobrist^=zobrist::side;
  side_to_move=!side_to_move;
  bs.repetitions=0;
  for(int i=SCI(board_status.size())-4;
      i>SCI(board_status.size())-bs.fifty_move_count-1;i-=2){
    if(i<=0) break;
    if(board_status[i].zobrist==st->zobrist){
      bs.repetitions=board_status[i].repetitions+1;
      break;
    }
  }
  std::swap(st->zobrist,bs.zobrist);
  board_status.push_back(bs);
  st=get_board_status();
}

void board::undo_move(){
  side_to_move=!side_to_move;
  const u8 from=move::from(st->move);
  u8 to=move::to(st->move);
  const move::move_type mt=move::mt(st->move);
  const bool us=side_to_move;
  const i32 push=pawn_push(us);
  if(mt==move::en_passant){
    move_piece<false>(to,from);
    to-=SCU8(push);
  } else if(mt==move::promotion){
    remove_piece<false>(to);
    set_piece<false>(pmake(us,pawn),from);
  } else if(mt==move::castle){
    const bool is_ks=to>from;
    const u8 rto=relative(us,is_ks?f1:d1);
    const u8 rfrom=relative(us,is_ks?h1:a1);
    move_piece<false>(rto,rfrom);
    move_piece<false>(to,from);
  } else move_piece<false>(to,from);
  if(st->captured) set_piece<false>(st->captured,to);
  board_status.pop_back();
  st=get_board_status();
}

void board::apply_null_move(){
  board_state bs;
  bs.ply_count=st->ply_count;
  bs.fifty_move_count=st->fifty_move_count;
  bs.castles=st->castles;
  bs.repetitions=0;
  bs.ep_sq=no_sq;
  bs.captured=no_piece;
  bs.move=0;
  bs.king_attacinfo.computed=false;
  board_status.push_back(bs);
  st=get_board_status();
  side_to_move=!side_to_move;
}

void board::undo_null_move(){
  side_to_move=!side_to_move;
  board_status.pop_back();
  st=get_board_status();
}

bool board::gives_check(const u16 m) const{
  const bitboard their_king_bb=get_pieces(!side_to_move,king);
  const u8 ksq=lsb(their_king_bb);
  const u8 from=move::from(m);
  const u8 to=move::to(m);
  const move::move_type mt=move::mt(m);
  i32 pt=ptmake(piece_on(from));
  bitboard occ=occupied_bb;
  if(mt==move::promotion){
    occ.clear(from);
    pt=move::get_piece_type(m);
  }
  bitboard attacked;
  switch(pt){
  case pawn:{
    attacked=attack::pawn_att[side_to_move][to];
    break;
  }
  case knight:{
    attacked=attack::knight_att[to];
    break;
  }
  case bishop:{
    attacked=attack::atts<bishop>(to,occ);
    break;
  }
  case rook:{
    attacked=attack::atts<rook>(to,occ);
    break;
  }
  case queen:{
    attacked=attack::atts<queen>(to,occ);
    break;
  }
  default: attacked={};
  }
  if(attacked&their_king_bb) return true;
  if(mt==move::castle){
    occ.clear(from);
    if(const u8 rsq=side_to_move==white
      ?to>from
      ?f1
      :d1
      :to>from
      ?f8
      :d8;attack::atts<rook>(rsq,occ)&their_king_bb)
      return true;
  } else if(mt==move::en_passant){
    occ.clear(from);
    occ.clear(SCU8(to-pawn_push(side_to_move)));
    occ.set(to);
    if(is_under_attack(!side_to_move,ksq)) return true;
  } else{
    occ.clear(from);
    occ.set(to);
    if(is_under_attack(!side_to_move,ksq)) return true;
  }
  return false;
}

bool board::is_pseudo_legal(const u16 m) const{
  if(!m) return false;
  const bool us=side_to_move;
  const bool them=!side_to_move;
  const u8 from=move::from(m);
  const u8 to=move::to(m);
  const move::move_type mt=move::mt(m);
  const i32 pc=piece_on(from);
  const i32 pt=ptmake(pc);
  if(pc==no_piece||make(pc)==them||
    pt!=pawn&&
    (mt==move::promotion||mt==move::en_passant)||
    pt!=king&&mt==move::castle)
    return false;
  if(pt==knight){
    if(get_color(us).is_set(to)||!attack::knight_att[from].is_set(to)) return false;
  } else if(pt==rook){
    if(get_color(us).is_set(to)||!attack::rook_att[from].is_set(to)||
      attack::in_between_sqs[from][to]&occupied_bb)
      return false;
  } else if(pt==bishop){
    if(get_color(us).is_set(to)||!attack::bishop_att[from].is_set(to)||
      attack::in_between_sqs[from][to]&occupied_bb)
      return false;
  } else if(pt==queen){
    if(get_color(us).is_set(to)||
      !(attack::rook_att[from].is_set(to)||
        attack::bishop_att[from].is_set(to))||
      attack::in_between_sqs[from][to]&occupied_bb)
      return false;
  } else if(pt==pawn){
    const int push=pawn_push(us);
    if(rmake(from)==(us==white?rank_7:rank_2)&&
      mt!=move::promotion)
      return false;
    if(rmake(from)==(us==white?rank_2:rank_7)&&
      to-from==2*push&&
      !occupied_bb.is_set(SCU8(from+push))&&
      !occupied_bb.is_set(to))
      return true;
    if(to-from==push){
      if(!occupied_bb.is_set(to)) return true;
    } else if(mt==move::en_passant){
      if(st->ep_sq==to&&
        piece_on(SCU8(to-push))==
        pmake(them,pawn)&&
        !occupied_bb.is_set(to)&&attack::pawn_att[us][from].is_set(to))
        return true;
    } else if(distance(from,to)==1&&get_color(them).is_set(to)){
      if(attack::pawn_att[us][from].is_set(to)) return true;
    }
    return false;
  } else if(pt==king){
    if(mt==move::castle){
      const bool c=rmake(to)==rank_1?white:black;
      if(const bool ks=to>from;c!=us||
        !can_castle(c==white
          ?ks
          ?white_ks
          :white_qs
          :ks
          ?black_ks
          :black_qs)||
        occupied_bb&
        (c==white
          ?ks
          ?white_ks_path
          :white_qs_path
          :ks
          ?black_ks_path
          :black_qs_path))
        return false;
    } else{
      if(get_color(us).is_set(to)||distance(from,to)!=1) return false;
    }
  }
  return true;
}

void board::gen_king_attack_info(king_attack_info& k) const{
  k.computed=true;
  const bool them=!side_to_move;
  const u8 ksq=this->ksq(side_to_move);
  const bitboard our_team=get_color(side_to_move);
  const bitboard their_team=get_color(them);
  k.pinned={};
  k.atts=
    their_team&(attack::pawn_att[side_to_move][ksq]&get_pieces(pawn)|
      attack::knight_att[ksq]&get_pieces(knight));
  int attacker_count=popcnt(k.atts);
  bitboard slider_attackers=
    their_team&
    (attack::atts<bishop>(ksq,their_team)&
      (get_pieces(bishop)|get_pieces(queen))|
      attack::atts<rook>(ksq,their_team)&(get_pieces(rook)|get_pieces(queen)));
  while(slider_attackers){
    const u8 s=pop_lsb(slider_attackers);
    const bitboard between=attack::in_between_sqs[ksq][s];
    const bitboard blockers=between&our_team;
    if(const auto num_blockers=popcnt(blockers);num_blockers==0){
      ++attacker_count;
      k.atts|=between;
      k.atts|=bitboard::from_sq(s);
    } else if(num_blockers==1) k.pinned.set(lsb(blockers));
  }
  k.double_check=attacker_count==2;
}

bool board::is_legal(const u16 m){
  if(!st->king_attacinfo.computed) gen_king_attack_info(st->king_attacinfo);
  const u8 from=move::from(m);
  const u8 to=move::to(m);
  const move::move_type mt=move::mt(m);
  const i32 pc=piece_on(from);
  if(const bool us=side_to_move;pc==pmake(us,king)){
    if(mt==move::castle){
      if(st->king_attacinfo.check()||
        to==relative(us,c1)&&
        (is_under_attack(us,relative(us,d1))||
          is_under_attack(us,relative(us,c1)))||
        to==relative(us,g1)&&
        (is_under_attack(us,relative(us,f1))||
          is_under_attack(us,relative(us,g1))))
        return false;
    } else{
      const bitboard occ_copy=occupied_bb;
      const i32 captured=ptmake(piece_on(to));
      occupied_bb.clear(from);
      occupied_bb.set(to);
      bool illegal;
      if(captured){
        piece_bb[captured].clear(to);
        illegal=is_under_attack(us,to);
        piece_bb[captured].set(to);
      } else illegal=is_under_attack(us,to);
      occupied_bb=occ_copy;
      if(illegal) return false;
    }
  } else if(st->king_attacinfo.check()&&
    (!st->king_attacinfo.atts.is_set(
        move::mt(m)==move::en_passant
        ?SCU8(to-pawn_push(us))
        :to)||
      st->king_attacinfo.pinned.is_set(from))||
    st->king_attacinfo.double_check)
    return false;
  else{
    if(st->king_attacinfo.pinned.is_set(from)){
      const u8 sq=ksq(us);
      const int dx_from=fmake(from)-fmake(sq);
      const int dy_from=rmake(from)-rmake(sq);
      const int dx_to=fmake(to)-fmake(sq);
      const int dy_to=rmake(to)-rmake(sq);
      if(dx_from==0||dx_to==0){
        if(dx_from!=dx_to) return false;
      } else if(dy_from==0||dy_to==0){
        if(dy_from!=dy_to) return false;
      } else if(dx_from*dy_to!=dy_from*dx_to) return false;
    }
    if(mt==move::en_passant){
      apply_move(m);
      const bool illegal=is_under_attack(us,ksq(us));
      undo_move();
      if(illegal) return false;
    }
  }
  return true;
}

bool board::is_under_attack(const bool us,const u8 sq) const{
  const bool them=!us;
  return attack::atts<rook>(sq,occupied_bb)&
    (get_pieces(them,rook)|get_pieces(them,queen))||
    attack::atts<bishop>(sq,occupied_bb)&
    (get_pieces(them,bishop)|get_pieces(them,queen))||
    attack::knight_att[sq]&get_pieces(them,knight)||
    attack::king_att[sq]&get_pieces(them,king)||
    attack::pawn_att[us][sq]&get_pieces(them,pawn);
}

bool board::is_in_check() const{
  return is_under_attack(side_to_move,ksq(side_to_move));
}

bitboard board::attackers_to(const u8 sq,const bitboard occupied) const{
  return attack::pawn_att[white][sq]&get_pieces(black,pawn)|
    attack::pawn_att[black][sq]&get_pieces(white,pawn)|
    attack::knight_att[sq]&get_pieces(knight)|
    attack::atts<bishop>(sq,occupied)&
    (get_pieces(bishop)|get_pieces(queen))|
    attack::atts<rook>(sq,occupied)&(get_pieces(rook)|get_pieces(queen))|
    attack::king_att[sq]&get_pieces(king);
}

template<i32 Pt> bitboard board::atts_by(const bool c){
  if constexpr(Pt==pawn)
    return c==white
      ?attack::pawn_att_bb<white>(get_pieces(c,pawn))
      :attack::pawn_att_bb<black>(get_pieces(c,pawn));
  else if constexpr(Pt==knight){
    bitboard attackers=get_pieces(c,knight);
    bitboard threats{};
    while(attackers) threats|=attack::knight_att[pop_lsb(attackers)];
    return threats;
  } else if constexpr(Pt==bishop){
    bitboard attackers=get_pieces(c,bishop);
    bitboard threats{};
    while(attackers) threats|=attack::atts<bishop>(pop_lsb(attackers),occupied());
    return threats;
  } else if constexpr(Pt==rook){
    bitboard attackers=get_pieces(c,rook);
    bitboard threats{};
    while(attackers) threats|=attack::atts<rook>(pop_lsb(attackers),occupied());
    return threats;
  } else if constexpr(Pt==queen){
    bitboard attackers=get_pieces(c,queen);
    bitboard threats{};
    while(attackers) threats|=attack::atts<queen>(pop_lsb(attackers),occupied());
    return threats;
  } else return {};
}

bitboard board::least_valuable_piece(const bitboard attacking,
  const bool attacker,i32& pc) const{
  for(i32 pt=pawn;pt<=king;++pt){
    if(const bitboard subset=attacking&get_pieces(pt)){
      pc=pmake(attacker,pt);
      return bitboard::from_sq(lsb(subset));
    }
  }
  return {};
}

int board::see(const u16 m) const{
  if(move::mt(m)!=move::normal) return 0;
  const u8 from=move::from(m);
  const u8 to=move::to(m);
  i32 from_pc=piece_on(from);
  const i32 to_pc=piece_on(to);
  bool attacker=make(from_pc);
  int gain[32],d=0;
  bitboard from_set=bitboard::from_sq(from);
  bitboard occ=occupied_bb;
  bitboard attacking=attackers_to(to,occ);
  gain[0]=eval::piece_values[to_pc];
  for(;;){
    ++d;
    attacker=!attacker;
    gain[d]=eval::piece_values[from_pc]-gain[d-1];
    if(std::max(-gain[d-1],gain[d])<0) break;
    attacking^=from_set;
    occ^=from_set;
    attacking|=
      occ&
      (attack::atts<bishop>(to,occ)&(get_pieces(bishop)|get_pieces(queen))|
        attack::atts<rook>(to,occ)&(get_pieces(rook)|get_pieces(queen)));
    from_set=least_valuable_piece(attacking,attacker,from_pc);
    if(!from_set) break;
  }
  while(--d) gain[d-1]=-std::max(-gain[d-1],gain[d]);
  return gain[0];
}

template bitboard board::atts_by<pawn>(bool c);
template bitboard board::atts_by<knight>(bool c);
template bitboard board::atts_by<bishop>(bool c);
template bitboard board::atts_by<rook>(bool c);
template bitboard board::atts_by<queen>(bool c);
