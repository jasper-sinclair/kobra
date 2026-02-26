#pragma once
#include "bitboard.h"

namespace attack{
  extern int king_dir[8];
  extern int knight_dir[8];
  extern int bishop_dir[4];
  extern int rook_dir[4];
  extern bitboard a_file_att[8][64];
  extern bitboard anti_diag_by_sq[n_sqs];
  extern bitboard anti_diag[15];
  extern bitboard bishop_att[n_sqs];
  extern bitboard diag_by_sq[n_sqs];
  extern bitboard diag[15];
  extern bitboard files[8];
  extern bitboard ranks[8];
  extern bitboard files_by_sq[n_sqs];
  extern bitboard fill_up_att[8][64];
  extern bitboard first_rank_att[8][64];
  extern bitboard in_between_sqs[n_sqs][n_sqs];
  extern bitboard king_att[n_sqs];
  extern bitboard knight_att[n_sqs];
  extern bitboard pawn_att[n_colors][n_sqs];
  extern bitboard ranks_by_sq[n_sqs];
  extern bitboard ray_att[n_dirs][n_sqs];
  extern bitboard rook_att[n_sqs];

  enum directions : u8{
    northeast, north, northwest, east, southeast, south, southwest, west,
    n_dirs
  };

  inline bitboard diag_att(
    const u8 sq,
    bitboard occ){
    occ = diag_by_sq[sq] & occ;
    occ = occ * fileb >> 58;
    return fill_up_att[fmake(sq)][occ.data] & diag_by_sq[sq];
  }

  inline bitboard anti_diag_att(
    const u8 sq,
    bitboard occ){
    occ = anti_diag_by_sq[sq] & occ;
    occ = occ * fileb >> 58;
    return fill_up_att[fmake(sq)][occ.data] & anti_diag_by_sq[sq];
  }

  inline bitboard rank_att(
    const u8 sq,
    bitboard occ){
    occ = ranks_by_sq[sq] & occ;
    occ = occ * fileb >> 58;
    return fill_up_att[fmake(sq)][occ.data] & ranks_by_sq[sq];
  }

  inline bitboard file_att(
    const u8 sq,
    bitboard occ){
    occ = filea & occ >> fmake(sq);
    occ = occ * diag_c2_h7 >> 58;
    return a_file_att[rmake(sq)][occ.data] << fmake(sq);
  }

  template <i32 Pt>
  bitboard atts(
    const u8 sq,
    const bitboard occupied){
    switch (Pt){
    case bishop:
      return diag_att(sq,occupied) |
        anti_diag_att(sq,occupied);
    case rook:
      return file_att(sq,occupied) |
        rank_att(sq,occupied);
    case queen:
      return diag_att(sq,occupied) |
        anti_diag_att(sq,occupied) |
        file_att(sq,occupied) |
        rank_att(sq,occupied);
    default:
      return {};
    }
  }

  template <bool C>
  bitboard pawn_att_bb(
    const bitboard b){
    return C == white
      ?b.shift<7>() | b.shift<9>()
      :b.shift<-9>() | b.shift<-7>();
  }

  void init();
}
