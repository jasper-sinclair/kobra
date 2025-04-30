#pragma once
#include "bitboard.h"

namespace attack{
  inline bitboard files[8]={
  filea,fileb,filec,filed,
  filee,filef,fileg,fileh
  };
  inline bitboard ranks[8]={
  rank1,rank2,rank3,rank4,
  rank5,rank6,rank7,rank8
  };

  enum directions : u8{
    northeast,north,northwest,east,
    southeast,south,southwest,west,
    n_dirs
  };

  inline int king_dir[8]={8,9,1,-7,-8,-9,-1,7};
  inline int knight_dir[8]={17,10,-6,-15,-17,-10,6,15};
  inline int bishop_dir[4]={9,-7,-9,7};
  inline int rook_dir[4]={8,1,-8,-1};

  inline bitboard a_file_att[8][64];
  inline bitboard anti_diag_by_sq[n_sqs];
  inline bitboard anti_diag[15];
  inline bitboard bishop_att[n_sqs];
  inline bitboard diag_by_sq[n_sqs];
  inline bitboard diag[15];
  inline bitboard files_by_sq[n_sqs];
  inline bitboard fill_up_att[8][64];
  inline bitboard first_rank_att[8][64];
  inline bitboard in_between_sqs[n_sqs][n_sqs];
  inline bitboard king_att[n_sqs];
  inline bitboard knight_att[n_sqs];
  inline bitboard pawn_att[n_colors][n_sqs];
  inline bitboard ranks_by_sq[n_sqs];
  inline bitboard ray_att[n_dirs][n_sqs];
  inline bitboard rook_att[n_sqs];

  inline bitboard diag_att(const u8 sq,bitboard occ){
    occ=diag_by_sq[sq]&occ;
    occ=occ*fileb>>58;
    return fill_up_att[fmake(sq)][occ.data]&diag_by_sq[sq];
  }

  inline bitboard anti_diag_att(const u8 sq,bitboard occ){
    occ=anti_diag_by_sq[sq]&occ;
    occ=occ*fileb>>58;
    return fill_up_att[fmake(sq)][occ.data]&anti_diag_by_sq[sq];
  }

  inline bitboard rank_att(const u8 sq,bitboard occ){
    occ=ranks_by_sq[sq]&occ;
    occ=occ*fileb>>58;
    return fill_up_att[fmake(sq)][occ.data]&ranks_by_sq[sq];
  }

  inline bitboard file_att(const u8 sq,bitboard occ){
    occ=filea&occ>>fmake(sq);
    occ=occ*diag_c2_h7>>58;
    return a_file_att[rmake(sq)][occ.data]<<fmake(sq);
  }

  template<i32 Pt> bitboard atts(const u8 sq,const bitboard occupied){
    switch(Pt){
    case bishop: return diag_att(sq,occupied)|anti_diag_att(sq,occupied);
    case rook: return file_att(sq,occupied)|rank_att(sq,occupied);
    case queen: return diag_att(sq,occupied)|anti_diag_att(sq,occupied)|
        file_att(sq,occupied)|rank_att(sq,occupied);
    default:;
    }
    return {};
  }

  template<bool C> bitboard pawn_att_bb(const bitboard b){
    return C==white
      ?b.shift<7>()|b.shift<9>()
      :b.shift<-9>()|b.shift<-7>();
  }

  void init();
}
