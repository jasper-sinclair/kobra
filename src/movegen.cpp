#include "movegen.h"
#include <algorithm>
#include "attack.h"

template<bool C> void gen_pawn_moves(board& pos,move_list& movelist){
  u8 from;
  u8 to;
  constexpr i32 up_left=C==white?northwest:southwest;
  constexpr i32 up_right=C==white?northeast:southeast;
  constexpr i32 up=C==white?north:south;
  constexpr bool them=!C;
  constexpr bool us=C;
  constexpr bitboard relative_rank4_bb=C==white?rank4:rank5;
  constexpr bitboard relative_rank8_bb=C==white?rank8:rank1;
  bitboard empty=~pos.occupied();
  const bitboard our_pawns=pos.get_pieces(us,pawn);
  const bitboard single_pawn_push_targets=our_pawns.shift<up>()&empty;
  const bitboard their_team=pos.get_color(them);
  const bitboard up_left_bb=our_pawns.shift<up_left>();
  const bitboard up_left_captures=up_left_bb&their_team;
  const bitboard up_right_bb=our_pawns.shift<up_right>();
  const bitboard up_right_captures=up_right_bb&their_team;
  bitboard atts=single_pawn_push_targets-relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up);
    movelist.add(move::make(from,to));
  }
  atts=single_pawn_push_targets&relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up);
    movelist.add(move::make<knight>(from,to));
    movelist.add(move::make<bishop>(from,to));
    movelist.add(move::make<rook>(from,to));
    movelist.add(move::make<queen>(from,to));
  }
  atts=single_pawn_push_targets.shift<up>()&empty&relative_rank4_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(2*up);
    movelist.add(move::make(from,to));
  }
  atts=up_right_captures-relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up_right);
    movelist.add(move::make(from,to));
  }
  atts=up_left_captures-relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up_left);
    movelist.add(move::make(from,to));
  }
  atts=up_right_captures&relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up_right);
    movelist.add(move::make<knight>(from,to));
    movelist.add(move::make<bishop>(from,to));
    movelist.add(move::make<rook>(from,to));
    movelist.add(move::make<queen>(from,to));
  }
  atts=up_left_captures&relative_rank8_bb;
  while(atts){
    to=pop_lsb(atts);
    from=to-SCU8(up_left);
    movelist.add(move::make<knight>(from,to));
    movelist.add(move::make<bishop>(from,to));
    movelist.add(move::make<rook>(from,to));
    movelist.add(move::make<queen>(from,to));
  }
  if(pos.st->ep_sq!=no_sq){
    atts=up_right_bb&bitboard::from_sq(pos.st->ep_sq);
    if(atts){
      to=pos.st->ep_sq;
      from=to-SCU8(up_right);
      movelist.add(make(from,to,move::en_passant));
    }
    atts=up_left_bb&bitboard::from_sq(pos.st->ep_sq);
    if(atts){
      to=pos.st->ep_sq;
      from=to-SCU8(up_left);
      movelist.add(make(from,to,move::en_passant));
    }
  }
}

template<bool C,i32 Pt> void gen_piece_moves(board& pos,move_list& movelist){
  bitboard friendly=pos.get_color(C);
  bitboard get_pieces=pos.get_pieces(C,Pt);
  while(get_pieces){
    u8 from=pop_lsb(get_pieces);
    bitboard atts=
      (Pt==knight
        ?attack::knight_att[from]
        :attack::atts<Pt>(from,pos.occupied()))-
      friendly;
    while(atts) movelist.add(move::make(from,pop_lsb(atts)));
  }
}

template<bool C> void gen_king_moves(board& pos,move_list& movelist){
  const u8 ksq=pos.ksq(C);
  bitboard atts=attack::king_att[ksq]-pos.get_color(C);
  while(atts) movelist.add(move::make(ksq,pop_lsb(atts)));
  if(ksq==relative(C,e1)){
    const bitboard empty=~pos.occupied();
    constexpr bitboard path1=
      C==white?white_qs_path:black_qs_path;
    constexpr bitboard path2=
      C==white?white_ks_path:black_ks_path;
    if(pos.can_castle(C==white?white_qs:black_qs)&&
      (empty&path1)==path1)
      movelist.add(make(ksq,relative(C,c1),move::castle));
    if(pos.can_castle(C==white?white_ks:black_ks)&&
      (empty&path2)==path2)
      movelist.add(make(ksq,relative(C,g1),move::castle));
  }
}

void gen_moves(board& pos,move_list& movelist){
  if(!pos.st->king_attacinfo.computed) pos.gen_king_attack_info(pos.st->king_attacinfo);
  movelist.last=movelist.data;
  if(pos.side_to_move==white){
    if(!pos.st->king_attacinfo.double_check){
      gen_pawn_moves<white>(pos,movelist);
      gen_piece_moves<white,knight>(pos,movelist);
      gen_piece_moves<white,bishop>(pos,movelist);
      gen_piece_moves<white,rook>(pos,movelist);
      gen_piece_moves<white,queen>(pos,movelist);
    }
    gen_king_moves<white>(pos,movelist);
  } else{
    if(!pos.st->king_attacinfo.double_check){
      gen_pawn_moves<black>(pos,movelist);
      gen_piece_moves<black,knight>(pos,movelist);
      gen_piece_moves<black,bishop>(pos,movelist);
      gen_piece_moves<black,rook>(pos,movelist);
      gen_piece_moves<black,queen>(pos,movelist);
    }
    gen_king_moves<black>(pos,movelist);
  }
  movelist.last=std::ranges::remove_if(movelist,[&](const move_info& m){
    return !pos.is_legal(m.move);
  }).begin();
}
