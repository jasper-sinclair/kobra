#include <array>
#include "bitboard.h"
#include "main.h"
#include "eval.h"
#include "attack.h"
#include "nnue.h"
#include "uci.h"

namespace eval{ namespace{
    int evaluate_mobility(const board& pos,const bool us){
      int mobility_score=0;
      for(i32 pt=knight;pt<=queen;++pt){
        bitboard get_pieces=pos.get_pieces(us,pt);
        while(get_pieces){
          const u8 sq=pop_lsb(get_pieces);
          bitboard legal_moves;
          switch(pt){
          case knight: legal_moves=attack::knight_att[sq];
            break;
          case bishop: legal_moves=attack::atts<bishop>(sq,pos.occupied());
            break;
          case rook: legal_moves=attack::atts<rook>(sq,pos.occupied());
            break;
          case queen: legal_moves=attack::atts<queen>(sq,pos.occupied());
            break;
          default: legal_moves=0;
            break;
          }
          legal_moves&=~pos.get_color(us);
          const int mobility=popcnt(legal_moves);
          switch(pt){
          case knight: mobility_score+=4*mobility;
            break;
          case bishop: mobility_score+=5*mobility;
            break;
          case rook: mobility_score+=3*mobility;
            break;
          case queen: mobility_score+=2*mobility;
            break;
          default: break;
          }
        }
      }
      return mobility_score;
    }

    int evaluate_king_safety(const board& pos,const bool us){
      const u8 king_sq=pos.ksq(us);
      const bitboard king_zone=bitboard::from_sq(king_sq)
        |bitboard::from_sq(king_sq).shift<north>()
        |bitboard::from_sq(king_sq).shift<south>()
        |bitboard::from_sq(king_sq).shift<east>()
        |bitboard::from_sq(king_sq).shift<west>()
        |bitboard::from_sq(king_sq).shift<northeast>()
        |bitboard::from_sq(king_sq).shift<northwest>()
        |bitboard::from_sq(king_sq).shift<southeast>()
        |bitboard::from_sq(king_sq).shift<southwest>();
      const bitboard pawn_shield=pos.get_pieces(us,pawn)&king_zone;
      const int shield_count=popcnt(pawn_shield);
      int safety_score=0;
      if(shield_count==3) safety_score+=10;
      else if(shield_count==2) safety_score+=5;
      else if(shield_count==1) safety_score-=10;
      else safety_score-=40;
      const bitboard occupied=pos.occupied();
      const bitboard threats=pos.attackers_to(king_sq,occupied)&~pos.get_color(us);
      safety_score-=10*popcnt(threats);
      if(threats&pos.get_pieces(queen)) safety_score-=10;
      if(threats&pos.get_pieces(rook)) safety_score-=6;
      if(threats&pos.get_pieces(bishop)) safety_score-=3;
      if(threats&pos.get_pieces(knight)) safety_score-=1;
      if(us==white){
        if(pos.can_castle(white_ks)||pos.can_castle(white_qs)){
          safety_score+=15;
        }
      } else if(us==black){
        if(pos.can_castle(black_ks)||pos.can_castle(black_qs)){
          safety_score+=15;
        }
      }
      return safety_score;
    }
  }

  int hce(const board& pos){
    const bool us=pos.side_to_move;
    const bool them=!us;
    int result=
      piece_values[pawn]*(popcnt(pos.get_pieces(us,pawn))-popcnt(pos.get_pieces(them,pawn)))+
      piece_values[knight]*(popcnt(pos.get_pieces(us,knight))-popcnt(pos.get_pieces(them,knight)))+
      piece_values[bishop]*(popcnt(pos.get_pieces(us,bishop))-popcnt(pos.get_pieces(them,bishop)))+
      piece_values[rook]*(popcnt(pos.get_pieces(us,rook))-popcnt(pos.get_pieces(them,rook)))+
      piece_values[queen]*(popcnt(pos.get_pieces(us,queen))-popcnt(pos.get_pieces(them,queen)));
    const bool w_queens=SCB(pos.get_pieces(white,queen));
    const bool b_queens=SCB(pos.get_pieces(black,queen));
    bool is_endgame;
    if(w_queens&&
      popcnt((pos.get_pieces(knight)|pos.get_pieces(bishop)|pos.get_pieces(rook))&pos.get_color(white))>1
      ||b_queens&&
      popcnt((pos.get_pieces(knight)|pos.get_pieces(bishop)|pos.get_pieces(rook))&pos.get_color(white))>1)
      is_endgame=false;
    else is_endgame=true;
    const game_phase game_phase=is_endgame?endgame:midgame;
    int psq_score=0;
    for(u8 sq=a1;sq<n_sqs;++sq){
      const i32 pc=pos.piece_on(sq);
      if(pc==no_piece) continue;
      const i32 pt=ptmake(pc);
      const bool c=make(pc);
      const int s=psq_table[c][pt][game_phase][sq];
      psq_score+=c==us?s:-s;
    }
    result+=psq_score;
    result+=evaluate_king_safety(pos,us);
    result-=evaluate_king_safety(pos,them);
    result+=evaluate_mobility(pos,us);
    result-=evaluate_mobility(pos,them);
    result+=uci::contempt*10*(pos.side_to_move==white?1:-1);
    return result;
  }

  namespace{
    constexpr std::array piece_map={
    0,6,5,4,3,2,1,0,
    0,12,11,10,9,8,7,0
    };

    int nnue_evaluate(const int player,int* pieces,int* squares){
      nnue_data nnue;
      nnue.accumulator.computed_accumulation=0;
      nnboard pos{};
      pos.nnue[0]=&nnue;
      pos.nnue[1]=nullptr;
      pos.nnue[2]=nullptr;
      pos.player=player;
      pos.pieces=pieces;
      pos.squares=squares;
      return nnue::evaluate_pos(&pos);
    }

    int evaluate_nnue(const board& pos){
      std::array<int,33> pieces{};
      std::array<int,33> squares{};
      int index=2;
      for(uint8_t sq=0;sq<64;++sq){
        const int piece=pos.piece_on(sq);
        if(const int mapped=piece_map[piece];mapped==1){
          pieces[0]=1;
          squares[0]=sq;
        } else if(mapped==7){
          pieces[1]=7;
          squares[1]=sq;
        } else if(mapped!=0){
          pieces[index]=mapped;
          squares[index]=sq;
          ++index;
        }
      }
      const int nnue_score=nnue_evaluate(pos.side_to_move,pieces.data(),squares.data());
      return nnue_score;
    }
  }

  int evaluate(const board& pos){
    if(uci::use_nnue) return evaluate_nnue(pos);
    return hce(pos);
  }
}
