#pragma once
#include <cstdint>
#include <iostream>
#include "main.h"

#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef _WIN64
#include <Windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <immintrin.h>

#ifdef _WIN64
using fd=HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
using map_t=HANDLE;
#else
typedef int fd;
#define FD_ERR -1
typedef size_t map_t;
#endif

inline fd open_file(const char* name){
#ifndef _WIN64
  return open(name, O_RDONLY);
#else
  return CreateFile(name, GENERIC_READ, FILE_SHARE_READ,nullptr, OPEN_EXISTING,
    FILE_FLAG_RANDOM_ACCESS,nullptr);
#endif
}

inline void close_file(fd fd){
#ifndef _WIN64
  close(fd);
#else
  CloseHandle(fd);
#endif
}

inline size_t file_size(fd fd){
#ifndef _WIN64
  struct stat statbuf;
  fstat(fd, &statbuf);
  return statbuf.st_size;
#else
  DWORD size_high;
  const DWORD size_low=GetFileSize(fd,&size_high);
  return static_cast<uint64_t>(size_high)<<32|size_low;
#endif
}

inline const void* map_file(fd fd,map_t* map){
#ifndef _WIN64
  * map=file_size(fd);
  void* data=mmap(NULL, *map, PROT_READ, MAP_SHARED, fd, 0);
  return data == MAP_FAILED ? NULL : data;
#else
  DWORD size_high;
  const DWORD size_low=GetFileSize(fd,&size_high);
  *map=CreateFileMapping(fd,nullptr, PAGE_READONLY,size_high,size_low,
    nullptr);
  if(*map==nullptr) return nullptr;
  return MapViewOfFile(*map, FILE_MAP_READ,0,0,0);
#endif
}

inline void unmap_file(const void* data,map_t map){
  if(!data) return;
#ifndef _WIN64
  munmap((void*)data, map);
#else
  UnmapViewOfFile(data);
  CloseHandle(map);
#endif
}

enum nnue_pieces : uint8_t{
  blank=0,wking,wqueen,wrook,wbishop,wknight,wpawn,bking,
  bqueen,brook,bbishop,bknight,bpawn
};

enum : uint16_t{
  ps_w_pawn=1,ps_b_pawn=1*64+1,ps_w_knight=2*64+1,ps_b_knight=3*64+1,ps_w_bishop=4*64+1,ps_b_bishop=5*64+1,ps_w_rook=6*64+1,ps_b_rook=7*64+1,
  ps_w_queen=8*64+1,ps_b_queen=9*64+1,ps_end=10*64+1
};

enum : uint8_t{
  fv_scale=16
};

enum : uint16_t{
  k_half_dimensions=256,ft_in_dims=64*ps_end,ft_out_dims=k_half_dimensions*2
};

enum : uint16_t{
  num_regs=16,simd_width=256
};

enum{
  transformer_start=3*4+177,network_start=transformer_start+4+2*256+2*256*64*641
};

using vec16_t=__m256i;
using vec8_t=__m256i;
using mask_t=uint32_t;
using mask2_t=uint64_t;
using clipped_t=int8_t;
using weight_t=int8_t;

using dirty=struct dirty_piece{
  int dirty_num;
  int pc[3];
  int from[3];
  int to[3];
};

using accu=struct accumulate{
  alignas(64) int16_t accumulation[2][256];
  int computed_accumulation;
};

using nnue_data=struct nnue_data{
  accu accumulator;
  dirty dirty_piece;
};

using nnboard=struct nnboard{
  int player;
  int* pieces;
  int* squares;
  nnue_data* nnue[3];
};

using index_list=struct{
  size_t size;
  unsigned values[30];
};

struct net_data{
  alignas(64) clipped_t input[ft_out_dims];
  clipped_t hidden1_out[32];
  int8_t hidden2_out[32];
};

inline constexpr uint32_t nnue_version=0x7AF32F16u;

inline int16_t ft_biases alignas(64)[k_half_dimensions];
inline int16_t ft_weights alignas(64)[k_half_dimensions*ft_in_dims];
inline int32_t hidden1_biases alignas(64)[32];
inline int32_t hidden2_biases alignas(64)[32];
inline weight_t hidden1_weights alignas(64)[64*512];
inline weight_t hidden2_weights alignas(64)[64*32];
inline weight_t output_weights alignas(64)[1*32];
inline int32_t output_biases[1];

inline uint32_t piece_to_index[2][14]={
{
0,0,ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0,
ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0
},
{
0,0,ps_b_queen,ps_b_rook,ps_b_bishop,ps_b_knight,ps_b_pawn,0,
ps_w_queen,ps_w_rook,ps_w_bishop,ps_w_knight,ps_w_pawn,0
}
};

class nnue{
public:
  static nnue& instance(){
    static nnue engine("kobra_2.0.nnue");
    return engine;
  }

  static int evaluate_pos(const nnboard* pos){
    alignas(8) mask_t input_mask[ft_out_dims/(8*sizeof(mask_t))];
    alignas(8) mask_t hidden1_mask[8/sizeof(mask_t)]={};
    net_data buf;
    transform(pos,buf.input,input_mask);
    affine_txfm(buf.input,buf.hidden1_out,ft_out_dims,hidden1_biases,
      hidden1_weights,input_mask,hidden1_mask,
      true);
    affine_txfm(buf.hidden1_out,buf.hidden2_out,32,hidden2_biases,
      hidden2_weights,hidden1_mask,nullptr,
      false);
    const int32_t out=affine_propagate(buf.hidden2_out,output_biases,output_weights);
    return out/fv_scale;
  }

  static int evaluate(const int player,int* pieces,int* squares){
    nnue_data nnue;
    nnue.accumulator.computed_accumulation=0;
    nnboard pos{};
    pos.nnue[0]=&nnue;
    pos.nnue[1]=nullptr;
    pos.nnue[2]=nullptr;
    pos.player=player;
    pos.pieces=pieces;
    pos.squares=squares;
    return evaluate_pos(&pos);
  }

  nnue(const nnue&) = delete;
  nnue& operator=(const nnue&) = delete;
private:
  explicit nnue(const char* net_path){
    map_t mapping;
    const fd file=open_file(net_path);
    if(file==FD_ERR){
      std::cerr<<"Failed to open "<<net_path<<SE;
      return;
    }
    const void* eval_data=map_file(file,&mapping);
    const size_t size=file_size(file);
    close_file(file);
    if(!verify_net(eval_data,size)){
      std::cerr<<"Invalid NNUE file: "<<net_path<<SE;
      if(mapping) unmap_file(eval_data,mapping);
      return;
    }
    init_weights(eval_data);
    if(mapping) unmap_file(eval_data,mapping);
    SO<<"NNUE loaded: "<<net_path<<SE;
  }

  static bool next_idx(unsigned*,unsigned*,mask2_t*,mask_t*,unsigned);
  static bool update_accumulator(const nnboard* pos);
  static bool verify_net(const void*,size_t);
  static const char* read_hidden_weights(weight_t*,unsigned,const char*);
  static int16_t readu_le_u16(const void*);
  static int32_t readu_le_u32(const void*);
  static unsigned make_index(int color,int sq,int pc,int ksq);
  static unsigned orient(int color,int square);
  static unsigned wt_idx(unsigned,unsigned,unsigned);
  static void affine_txfm(int8_t*,void*,unsigned,unsigned,int32_t*,weight_t*,mask_t*,mask_t*,bool);
  static void append_active_indices(const nnboard* pos,index_list active[2]);
  static void append_changed_indices(const nnboard* pos,index_list removed[2],index_list added[2],bool reset[2]);
  static void half_kp_append_active_indices(const nnboard* pos,int color,index_list* active);
  static void half_kp_append_changed_indices(const nnboard* pos,int color,const dirty_piece* dp,index_list* removed,index_list* added);
  static void init_weights(const void*);
  static void permute_biases(int32_t*);
  static void read_output_weights(weight_t*,const char*);
  static void refresh_accumulator(const nnboard* pos);

  static bool is_king(const int p){
    return p==white_king||p==black_king;
  }

  static int32_t affine_propagate(clipped_t* input,const int32_t* biases,weight_t* weights){
    const auto iv=reinterpret_cast<__m256i*>(input);
    const auto row=reinterpret_cast<__m256i*>(weights);
    __m256i prod=_mm256_maddubs_epi16(iv[0],row[0]);
    prod=_mm256_madd_epi16(prod,_mm256_set1_epi16(1));
    __m128i sum=_mm_add_epi32(
      _mm256_castsi256_si128(prod),
      _mm256_extracti128_si256(prod,1)
      );
    sum=_mm_add_epi32(sum,_mm_shuffle_epi32(sum,0x1b));
    return _mm_cvtsi128_si32(sum)+_mm_extract_epi32(sum,1)+biases[0];
  }

  static void affine_txfm(
    int8_t* input,void* output,unsigned in_dims,
    int32_t* biases,weight_t* weights,mask_t* in_mask,
    mask_t* out_mask,bool pack8_and_calc_mask
    ){
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
      __m256i mul=_mm256_set1_epi16(factor);
      __m256i prod=_mm256_maddubs_epi16(mul,_mm256_unpacklo_epi8(first,second));
      __m256i signs=_mm256_cmpgt_epi16(k_zero,prod);
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

  static void transform(const nnboard* pos,clipped_t* output,mask_t* out_mask){
    if(!update_accumulator(pos)) refresh_accumulator(pos);
    auto& accumulation=pos->nnue[0]->accumulator.accumulation;
    const int perspectives[2]={pos->player,!pos->player};
    for(unsigned p=0;p<2;p++){
      const unsigned offset=k_half_dimensions*p;
      constexpr unsigned num_chunks=16*k_half_dimensions/simd_width;
      const auto out=reinterpret_cast<vec8_t*>(&output[offset]);
      for(unsigned i=0;i<num_chunks/2;i++){
        const vec16_t s0=reinterpret_cast<vec16_t*>(accumulation[perspectives[p]])[i*2];
        const vec16_t s1=reinterpret_cast<vec16_t*>(accumulation[perspectives[p]])[i*2+1];
        out[i]=_mm256_packs_epi16(s0,s1);
        *out_mask++=_mm256_movemask_epi8(_mm256_cmpgt_epi8(out[i],_mm256_setzero_si256()));
      }
    }
  }
};
