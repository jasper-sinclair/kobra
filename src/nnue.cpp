#include "nnue.h"
#include "bitboard.h"
#include "main.h"

void nnue::refresh_accumulator(const nnboard* pos){
  accu* accumulator=&pos->nnue[0]->accumulator;
  index_list active_indices[2];
  active_indices[0].size=active_indices[1].size=0;
  append_active_indices(pos,active_indices);
  for(unsigned c=0;c<2;c++){
    for(unsigned i=0;i<k_half_dimensions/(num_regs*simd_width/16);i++){
      const vec16_t* ft_biases_tile=
        reinterpret_cast<vec16_t*>(&ft_biases[i*(num_regs*simd_width/16)]);
      const auto acc_tile=reinterpret_cast<vec16_t*>(
        &accumulator->accumulation[c][i*(num_regs*simd_width/16)]);
      vec16_t acc[num_regs];
      for(unsigned j=0;j<num_regs;j++) acc[j]=ft_biases_tile[j];
      for(size_t k=0;k<active_indices[c].size;k++){
        const unsigned index=active_indices[c].values[k];
        const unsigned offset=k_half_dimensions*index+i*(num_regs*simd_width/16);
        const vec16_t* column=reinterpret_cast<vec16_t*>(&ft_weights[offset]);
        for(unsigned j=0;j<num_regs;j++) acc[j]=_mm256_add_epi16(acc[j],column[j]);
      }
      for(unsigned j=0;j<num_regs;j++) acc_tile[j]=acc[j];
    }
  }
  accumulator->computed_accumulation=1;
}

bool nnue::update_accumulator(const nnboard* pos){
  accu* accumulator=&pos->nnue[0]->accumulator;
  if(accumulator->computed_accumulation) return true;
  accu* prev_acc;
  if((!pos->nnue[1]||
      !(prev_acc=&pos->nnue[1]->accumulator)->computed_accumulation)&&
    (!pos->nnue[2]||
      !(prev_acc=&pos->nnue[2]->accumulator)->computed_accumulation))
    return false;
  index_list removed_indices[2],added_indices[2];
  removed_indices[0].size=removed_indices[1].size=0;
  added_indices[0].size=added_indices[1].size=0;
  bool reset[2];
  append_changed_indices(pos,removed_indices,added_indices,reset);
  for(unsigned i=0;i<k_half_dimensions/(num_regs*simd_width/16);i++){
    for(unsigned c=0;c<2;c++){
      const auto acc_tile=reinterpret_cast<vec16_t*>(
        &accumulator->accumulation[c][i*(num_regs*simd_width/16)]);
      vec16_t acc[num_regs];
      if(reset[c]){
        const vec16_t* ft_b_tile=
          reinterpret_cast<vec16_t*>(&ft_biases[i*(num_regs*simd_width/16)]);
        for(unsigned j=0;j<num_regs;j++) acc[j]=ft_b_tile[j];
      } else{
        const vec16_t* prev_acc_tile=reinterpret_cast<vec16_t*>(
          &prev_acc->accumulation[c][i*(num_regs*simd_width/16)]);
        for(unsigned j=0;j<num_regs;j++) acc[j]=prev_acc_tile[j];
        for(unsigned k=0;k<removed_indices[c].size;k++){
          const unsigned index=removed_indices[c].values[k];
          const unsigned offset=k_half_dimensions*index+i*(num_regs*simd_width/16);
          const vec16_t* column=
            reinterpret_cast<vec16_t*>(&ft_weights[offset]);
          for(unsigned j=0;j<num_regs;j++) acc[j]=_mm256_sub_epi16(acc[j],column[j]);
        }
      }
      for(unsigned k=0;k<added_indices[c].size;k++){
        const unsigned index=added_indices[c].values[k];
        const unsigned offset=k_half_dimensions*index+i*(num_regs*simd_width/16);
        const vec16_t* column=reinterpret_cast<vec16_t*>(&ft_weights[offset]);
        for(unsigned j=0;j<num_regs;j++) acc[j]=_mm256_add_epi16(acc[j],column[j]);
      }
      for(unsigned j=0;j<num_regs;j++) acc_tile[j]=acc[j];
    }
  }
  accumulator->computed_accumulation=1;
  return true;
}

inline void nnue::append_active_indices(const nnboard* pos,index_list active[2]){
  for(int c=0;c<2;c++) half_kp_append_active_indices(pos,c,&active[c]);
}

inline void nnue::append_changed_indices(
  const nnboard* pos,
  index_list removed[2],
  index_list added[2],
  bool reset[2]
  ){
  const dirty_piece* dp=&pos->nnue[0]->dirty_piece;
  if(pos->nnue[1]&&pos->nnue[1]->accumulator.computed_accumulation){
    for(int c=0;c<2;c++){
      reset[c]=dp->pc[0]==SCI(c==0?black_king:white_king);
      if(reset[c]) half_kp_append_active_indices(pos,c,&added[c]);
      else half_kp_append_changed_indices(pos,c,dp,&removed[c],&added[c]);
    }
  } else{
    const dirty_piece* dp2=&pos->nnue[1]->dirty_piece;
    for(int c=0;c<2;c++){
      const int match_piece=c==0?SCI(black):SCI(white_king);
      reset[c]=dp->pc[0]==match_piece||dp2->pc[0]==match_piece;
      if(reset[c]) half_kp_append_active_indices(pos,c,&added[c]);
      else{
        half_kp_append_changed_indices(pos,c,dp,&removed[c],&added[c]);
        half_kp_append_changed_indices(pos,c,dp2,&removed[c],&added[c]);
      }
    }
  }
}

inline void nnue::half_kp_append_active_indices(const nnboard* pos,const int color,index_list* active){
  const int ksq=orient(color,pos->squares[color]);
  for(int i=2;pos->pieces[i];i++){
    const int sq=pos->squares[i];
    const int pc=pos->pieces[i];
    active->values[active->size++]=make_index(color,sq,pc,ksq);
  }
}

inline void nnue::half_kp_append_changed_indices(
  const nnboard* pos,
  const int color,
  const dirty_piece* dp,
  index_list* removed,
  index_list* added
  ){
  const int ksq=orient(color,pos->squares[color]);
  for(int i=0;i<dp->dirty_num;i++){
    const int pc=dp->pc[i];
    if(is_king(pc)) continue;
    if(dp->from[i]!=64) removed->values[removed->size++]=make_index(color,dp->from[i],pc,ksq);
    if(dp->to[i]!=64) added->values[added->size++]=make_index(color,dp->to[i],pc,ksq);
  }
}

inline unsigned nnue::make_index(const int color,const int sq,const int pc,const int ksq){
  static constexpr uint32_t piece_to_ind[2][14]={
  {0,0,ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0,
  ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0
  },
  {0,0,ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0,
  ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0
  }
  };
  return orient(color,sq)+piece_to_ind[color][pc]+ps_end*ksq;
}

inline unsigned nnue::orient(const int color,const int square){
  return square^(color==0?0:0x3f);
}

bool nnue::next_idx(unsigned* idx,unsigned* offset,mask2_t* v,mask_t* mask,
  const unsigned in_dims){
  while(*v==0){
    *offset+=8*sizeof(mask2_t);
    if(*offset>=in_dims) return false;
    memcpy(v,reinterpret_cast<char*>(mask)+*offset/8,sizeof(mask2_t));
  }
  *idx=*offset+lsb(*v);
  *v&=*v-1;
  return true;
}

inline void nnue::affine_txfm(int8_t* input,void* output,unsigned in_dims,
  unsigned out_dims,int32_t* biases,
  weight_t* weights,mask_t* in_mask,
  mask_t* out_mask,const bool pack8_and_calc_mask){
  (void)out_dims;
  __m256i k_zero=_mm256_setzero_si256();
  __m256i out_0=reinterpret_cast<__m256i*>(biases)[0];
  __m256i out_1=reinterpret_cast<__m256i*>(biases)[1];
  __m256i out_2=reinterpret_cast<__m256i*>(biases)[2];
  __m256i out_3=reinterpret_cast<__m256i*>(biases)[3];
  __m256i first,second;
  mask2_t v;
  unsigned idx;
  memcpy(&v,in_mask,sizeof(mask2_t));
  for(unsigned offset=0;offset<in_dims;){
    if(!next_idx(&idx,&offset,&v,in_mask,in_dims)) break;
    first=reinterpret_cast<__m256i*>(weights)[idx];
    uint16_t factor=static_cast<unsigned char>(input[idx]);
    if(next_idx(&idx,&offset,&v,in_mask,in_dims)){
      second=reinterpret_cast<__m256i*>(weights)[idx];
      factor|=input[idx]<<8;
    } else{
      second=k_zero;
    }
    __m256i mul=_mm256_set1_epi16(factor),prod,signs;
    prod=_mm256_maddubs_epi16(mul,_mm256_unpacklo_epi8(first,second));
    signs=_mm256_cmpgt_epi16(k_zero,prod);
    out_0=_mm256_add_epi32(out_0,_mm256_unpacklo_epi16(prod,signs));
    out_1=_mm256_add_epi32(out_1,_mm256_unpackhi_epi16(prod,signs));
    prod=_mm256_maddubs_epi16(mul,_mm256_unpackhi_epi8(first,second));
    signs=_mm256_cmpgt_epi16(k_zero,prod);
    out_2=_mm256_add_epi32(out_2,_mm256_unpacklo_epi16(prod,signs));
    out_3=_mm256_add_epi32(out_3,_mm256_unpackhi_epi16(prod,signs));
  }
  __m256i out16_0=_mm256_srai_epi16(_mm256_packs_epi32(out_0,out_1),6);
  __m256i out16_1=_mm256_srai_epi16(_mm256_packs_epi32(out_2,out_3),6);
  auto out_vec=static_cast<__m256i*>(output);
  out_vec[0]=_mm256_packs_epi16(out16_0,out16_1);
  if(pack8_and_calc_mask) out_mask[0]=_mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0],k_zero));
  else out_vec[0]=_mm256_max_epi8(out_vec[0],k_zero);
}

inline unsigned nnue::wt_idx(const unsigned r,unsigned c,const unsigned dims){
  (void)dims;
  if(dims>32){
    unsigned b=c&0x18;
    b=b<<1|b>>1;
    c=c&~0x18|b&0x18;
  }
  return c*32+r;
}

inline const char* nnue::read_hidden_weights(weight_t* w,const unsigned dims,
  const char* d){
  for(unsigned r=0;r<32;r++) for(unsigned c=0;c<dims;c++) w[wt_idx(r,c,dims)]=*d++;
  return d;
}

inline void nnue::permute_biases(int32_t* biases){
  const auto b=reinterpret_cast<__m128i*>(biases);
  __m128i tmp[8];
  tmp[0]=b[0];
  tmp[1]=b[4];
  tmp[2]=b[1];
  tmp[3]=b[5];
  tmp[4]=b[2];
  tmp[5]=b[6];
  tmp[6]=b[3];
  tmp[7]=b[7];
  memcpy(b,tmp,8*sizeof(__m128i));
}

inline int32_t nnue::readu_le_u32(const void* p){
  const auto q=static_cast<const uint8_t*>(p);
  return q[0]|q[1]<<8|q[2]<<16|q[3]<<24;
}

inline int16_t nnue::readu_le_u16(const void* p){
  const auto q=static_cast<const uint8_t*>(p);
  return static_cast<int16_t>(q[0]|q[1]<<8);
}

inline void nnue::read_output_weights(weight_t* w,const char* d){
  for(unsigned i=0;i<32;i++){
    const unsigned c=i;
    w[c]=*d++;
  }
}

bool nnue::verify_net(const void* eval_data,const size_t size){
  if(size!=21022697) return false;
  const auto d=static_cast<const char*>(eval_data);
  if(readu_le_u32(d)!=nnue_version) return false;
  if(readu_le_u32(d+4)!=0x3e5aa6eeU) return false;
  if(readu_le_u32(d+8)!=177) return false;
  if(readu_le_u32(d+transformer_start)!=0x5d69d7b8) return false;
  if(readu_le_u32(d+network_start)!=0x63337156) return false;
  return true;
}

void nnue::init_weights(const void* eval_data){
  const char* d=static_cast<const char*>(eval_data)+transformer_start+4;
  for(unsigned i=0;i<k_half_dimensions;i++,d+=2) ft_biases[i]=readu_le_u16(d);
  for(unsigned i=0;i<k_half_dimensions*ft_in_dims;i++,d+=2) ft_weights[i]=readu_le_u16(d);
  d+=4;
  for(unsigned i=0;i<32;i++,d+=4) hidden1_biases[i]=readu_le_u32(d);
  d=read_hidden_weights(hidden1_weights,512,d);
  for(unsigned i=0;i<32;i++,d+=4) hidden2_biases[i]=readu_le_u32(d);
  d=read_hidden_weights(hidden2_weights,32,d);
  for(unsigned i=0;i<1;i++,d+=4) output_biases[i]=readu_le_u32(d);
  read_output_weights(output_weights,d);
  permute_biases(hidden1_biases);
  permute_biases(hidden2_biases);
}
