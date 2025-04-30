#include "bitboard.h"
#include "main.h"
#include "attack.h"

namespace attack{
  void init(){
    for(u8 from=a1;from<n_sqs;++from){
      auto atts=bitboard();
      for(const auto& d:knight_dir){
        if(const u8 to=from+SCU8(d);is_valid(to)&&distance(from,to)<=2) atts.set(to);
      }
      knight_att[from]=atts;
    }
    for(u8 from=a1;from<n_sqs;++from){
      auto atts=bitboard();
      for(const auto& d:king_dir){
        if(const u8 to=from+SCU8(d);is_valid(to)&&distance(from,to)==1) atts.set(to);
      }
      king_att[from]=atts;
    }
    diag[0]=bitboard(0x8040201008040201);
    for(int i=1;i<8;++i){
      diag[i]=diag[0]<<SCU8(i);
      for(int j=0;j<i-1;++j) diag[i]-=ranks[7-j];
    }
    for(int i=1;i<8;++i){
      diag[i+7]=diag[0]>>SCU8(i);
      for(int j=0;j<i-1;++j) diag[i+7]-=ranks[j];
    }
    for(int i=0;i<15;++i) anti_diag[i]=mirrored(diag[i]);
    for(u8 sq=a1;sq<n_sqs;++sq){
      bitboard atts={};
      for(auto& b:diag){
        if(b.is_set(sq)){
          atts=b;
          break;
        }
      }
      for(auto& b:anti_diag){
        if(b.is_set(sq)){
          atts^=b;
          break;
        }
      }
      bishop_att[sq]=atts;
    }
    for(u8 sq=a1;sq<n_sqs;++sq){
      bitboard atts={};
      for(auto& b:files){
        if(b.is_set(sq)){
          atts=b;
          break;
        }
      }
      for(auto& b:ranks){
        if(b.is_set(sq)){
          atts^=b;
          break;
        }
      }
      rook_att[sq]=atts;
    }
    for(u8 sq=a1;sq<n_sqs;++sq){
      const i8 file=fmake(sq);
      const i8 rank=rmake(sq);
      ray_att[north][sq]=sq<64?bitboard(0x101010101010100ULL<<sq):bitboard(0);
      ray_att[northeast][sq]=sq<64?bitboard(0x8040201008040200ULL<<sq):bitboard(0);
      ray_att[south][sq]=bitboard(0x80808080808080)>>(63-sq);
      ray_att[east][sq]=bitboard(0xfe)<<sq;
      if(rank<rank_8) ray_att[east][sq]-=ranks[rank+1];
      ray_att[west][sq]=bitboard(0x7f00000000000000)>>(sq^7^56);
      if(rank>rank_1) ray_att[west][sq]-=ranks[rank-1];
      for(int i=0;i<file-rank-1;++i) ray_att[northeast][sq]-=ranks[7-i];
      ray_att[northwest][sq]=
        bitboard(0x102040810204000)>>fmake(sq^7)<<8*rank;
      for(int i=7;i>file;--i) ray_att[northwest][sq]-=files[i];
      ray_att[southeast][sq^56]=mirrored(ray_att[northeast][sq]);
      ray_att[southwest][sq^56]=mirrored(ray_att[northwest][sq]);
    }
    for(u8 sq=a1;sq<n_sqs;++sq){
      bitboard b=bitboard::from_sq(sq);
      pawn_att[white][sq]|=b.shift<7>()|b.shift<9>();
      pawn_att[black][sq]|=b.shift<-9>()|b.shift<-7>();
    }
    for(u8 i=a1;i<n_sqs;++i){
      for(u8 j=a1;j<n_sqs;++j){
        const bitboard b=bitboard::from_sq(j);
        for(const auto& arr:ray_att){
          if(arr[i]&b){
            in_between_sqs[i][j]=arr[i]-arr[j]-b;
            break;
          }
        }
      }
    }
    for(u8 i=0;i<64;++i){
      const bitboard b=bitboard::from_sq(i);
      for(const auto& d:diag){
        if(d&b){
          diag_by_sq[i]=d-b;
          break;
        }
      }
      for(const auto& d:anti_diag){
        if(d&b){
          anti_diag_by_sq[i]=d-b;
          break;
        }
      }
      for(const auto& d:files){
        if(d&b){
          files_by_sq[i]=d-b;
          break;
        }
      }
      for(const auto& d:ranks){
        if(d&b){
          ranks_by_sq[i]=d-b;
          break;
        }
      }
    }
    for(u8 i=0;i<8;++i){
      for(int j=0;j<64;++j){
        bitboard occ=bitboard(SCU64(j)<<1)-bitboard::from_sq(i);
        for(int k=i-1;;--k){
          if(k<0) break;
          first_rank_att[i][j].set(SCU8(k));
          if(occ.is_set(SCU8(k))) break;
        }
        for(int k=i+1;;++k){
          if(k>7) break;
          first_rank_att[i][j].set(SCU8(k));
          if(occ.is_set(SCU8(k))) break;
        }
        fill_up_att[i][j]=filea*first_rank_att[i][j];
        const u8 r=i^7;
        occ=bitboard(SCU64(j)<<1)-bitboard::from_sq(r);
        for(int k=r-1;;--k){
          if(k<0) break;
          a_file_att[i][j].set(SCU8(8*(k^7)));
          if(occ.is_set(SCU8(k))) break;
        }
        for(int k=r+1;;++k){
          if(k>7) break;
          a_file_att[i][j].set(SCU8(8*(k^7)));
          if(occ.is_set(SCU8(k))) break;
        }
      }
    }
  }
}
