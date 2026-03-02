#pragma once
#include <cstdint>
#include <immintrin.h>
#include "main.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

enum nnue_pieces : uint8_t{
  blank = 0, wking, wqueen, wrook, wbishop, wknight, wpawn, bking,
  bqueen, brook, bbishop, bknight, bpawn
};

enum : uint16_t{
  ps_w_pawn = 1, ps_b_pawn = 1 * 64 + 1, ps_w_knight = 2 * 64 + 1, ps_b_knight = 3 * 64 + 1, ps_w_bishop = 4 * 64 + 1, ps_b_bishop = 5 * 64 + 1, ps_w_rook = 6 * 64 + 1, ps_b_rook = 7 * 64 + 1,
  ps_w_queen = 8 * 64 + 1, ps_b_queen = 9 * 64 + 1, ps_end = 10 * 64 + 1
};

enum : uint8_t{
  fv_scale = 16
};

enum : uint16_t{
  k_half_dimensions = 256, ft_in_dims = 64 * ps_end, ft_out_dims = k_half_dimensions * 2
};

enum : uint16_t{
  num_regs = 16, simd_width = 256
};

enum{
  transformer_start = 3 * 4 + 177, network_start = transformer_start + 4 + 2 * 256 + 2 * 256 * 64 * 641
};

using vec16_t = __m256i;
using vec8_t = __m256i;
using mask_t = uint32_t;
using mask2_t = uint64_t;
using clipped_t = int8_t;
using weight_t = int8_t;

struct dirty_piece{
  int dirty_num;
  int pc[3];
  int from[3];
  int to[3];
};

struct accu{
  alignas(64) int16_t accumulation[2][256];
  int computed_accumulation;
};

struct nnue_data{
  accu accumulator;
  dirty_piece dirty;
};

struct nnboard{
  int player;
  int* pieces;
  int* squares;
  nnue_data* nnue[3];
};

struct index_list{
  size_t size;
  unsigned values[30];
};

struct net_data{
  alignas(64) clipped_t input[ft_out_dims];
  clipped_t hidden1_out[32];
  int8_t hidden2_out[32];
};

inline constexpr uint32_t nnue_version = 0x7AF32F16u;
extern uint32_t piece_to_index[2][14];

class nnue{
public:
  static nnue& instance();

  alignas(64) static int16_t ft_biases[k_half_dimensions];
  alignas(64) static int16_t ft_weights[k_half_dimensions * ft_in_dims];
  alignas(64) static int32_t hidden1_biases[32];
  alignas(64) static int32_t hidden2_biases[32];
  alignas(64) static weight_t hidden1_weights[64 * 512];
  alignas(64) static weight_t hidden2_weights[64 * 32];
  alignas(64) static weight_t output_weights[32];
  static int32_t output_biases[1];

  static int evaluate_pos(
    const nnboard* pos){
    alignas(8) mask_t input_mask[ft_out_dims / (8 * sizeof(mask_t))];
    alignas(8) mask_t hidden1_mask[8 / sizeof(mask_t)] = {};
    net_data buf;
    transform(pos,buf.input,input_mask);
    affine_txfm(buf.input,buf.hidden1_out,ft_out_dims,hidden1_biases,
      hidden1_weights,input_mask,hidden1_mask,
      true);
    affine_txfm(buf.hidden1_out,buf.hidden2_out,32,hidden2_biases,
      hidden2_weights,hidden1_mask,nullptr,
      false);
    const int32_t out = affine_propagate(buf.hidden2_out,output_biases,output_weights);
    return out / fv_scale;
  }

  static int evaluate(
    const int player,
    int* pieces,
    int* squares){
    nnue_data nnue;
    nnue.accumulator.computed_accumulation = 0;
    nnboard pos{};
    pos.nnue[0] = &nnue;
    pos.nnue[1] = nullptr;
    pos.nnue[2] = nullptr;
    pos.player = player;
    pos.pieces = pieces;
    pos.squares = squares;
    return evaluate_pos(&pos);
  }

  nnue(const nnue&)=delete;
  nnue& operator=(const nnue&)=delete;
  nnue(nnue&&)=delete;
  nnue& operator=(nnue&&)=delete;
  ~nnue()=default;

private:
  #ifdef EMBED_NNUE
  nnue();
  #else
  #ifndef EMBED_NNUE
  explicit nnue(
    const char* net_path);
  #endif
  #endif
  static bool next_idx(
    unsigned*,
    unsigned*,
    mask2_t*,
    mask_t*,
    unsigned);

  static bool update_accumulator(
    const nnboard* pos);

  static bool verify_net(
    const void*,
    size_t);

  static const char* read_hidden_weights(
    weight_t*,
    unsigned,
    const char*);

  static int16_t readu_le_u16(
    const void*);

  static uint32_t readu_le_u32(
    const void*);

  static unsigned make_index(
    unsigned color,
    unsigned sq,
    unsigned pc,
    unsigned ksq);

  static unsigned orient(
    unsigned color,
    unsigned square);

  static unsigned wt_idx(
    unsigned,
    unsigned,
    unsigned);

  static void affine_txfm(
    int8_t*,
    void*,
    unsigned,
    unsigned,
    int32_t*,
    weight_t*,
    mask_t*,
    mask_t*,
    bool);

  static void append_active_indices(
    const nnboard* pos,
    index_list active[2]);

  static void append_changed_indices(
    const nnboard* pos,
    index_list removed[2],
    index_list added[2],
    bool reset[2]);

  static void half_kp_append_active_indices(
    const nnboard* pos,
    int color,
    index_list* active);

  static void half_kp_append_changed_indices(
    const nnboard* pos,
    int color,
    const dirty_piece* dp,
    index_list* removed,
    index_list* added);

  static void init_weights(
    const void*);

  static void permute_biases(
    int32_t*);

  static void read_output_weights(
    weight_t*,
    const char*);

  static void refresh_accumulator(
    const nnboard* pos);

  static bool is_king(
    const int p){
    return p == white_king || p == black_king;
  }

  static int32_t affine_propagate(
    clipped_t* input,
    const int32_t* biases,
    weight_t* weights){
    const auto iv = reinterpret_cast<__m256i*>(input);
    const auto row = reinterpret_cast<__m256i*>(weights);
    __m256i prod = _mm256_maddubs_epi16(iv[0],row[0]);
    prod = _mm256_madd_epi16(prod,_mm256_set1_epi16(1));
    __m128i sum = _mm_add_epi32(
      _mm256_castsi256_si128(prod),
      _mm256_extracti128_si256(prod,1)
    );
    sum = _mm_add_epi32(sum,_mm_shuffle_epi32(sum,0x1b));
    return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum,1) + biases[0];
  }

  static void affine_txfm(
    int8_t* input,
    void* output,
    unsigned in_dims,
    int32_t* biases,
    weight_t* weights,
    mask_t* in_mask,
    mask_t* out_mask,
    bool pack8_and_calc_mask
  ){
    __m256i k_zero = _mm256_setzero_si256();
    __m256i out_0 = reinterpret_cast<__m256i*>(biases)[0];
    __m256i out_1 = reinterpret_cast<__m256i*>(biases)[1];
    __m256i out_2 = reinterpret_cast<__m256i*>(biases)[2];
    __m256i out_3 = reinterpret_cast<__m256i*>(biases)[3];
    __m256i first, second;
    mask2_t v;
    unsigned idx;
    memcpy(&v,in_mask,sizeof(mask2_t));
    for (unsigned offset = 0; offset < in_dims;){
      if (! next_idx(&idx,&offset,&v,in_mask,in_dims)) break;
      first = reinterpret_cast<__m256i*>(weights)[idx];
      uint16_t factor = static_cast<unsigned char>(input[idx]);
      if (next_idx(&idx,&offset,&v,in_mask,in_dims)){
        second = reinterpret_cast<__m256i*>(weights)[idx];
        factor |= static_cast<uint16_t>(static_cast<uint8_t>(input[idx])) << 8;
      } else{
        second = k_zero;
      }
      const int16_t factor_s = static_cast<int16_t>(factor);
      __m256i mul = _mm256_set1_epi16(factor_s);
      __m256i prod = _mm256_maddubs_epi16(mul,_mm256_unpacklo_epi8(first,second));
      __m256i signs = _mm256_cmpgt_epi16(k_zero,prod);
      out_0 = _mm256_add_epi32(out_0,_mm256_unpacklo_epi16(prod,signs));
      out_1 = _mm256_add_epi32(out_1,_mm256_unpackhi_epi16(prod,signs));
      prod = _mm256_maddubs_epi16(mul,_mm256_unpackhi_epi8(first,second));
      signs = _mm256_cmpgt_epi16(k_zero,prod);
      out_2 = _mm256_add_epi32(out_2,_mm256_unpacklo_epi16(prod,signs));
      out_3 = _mm256_add_epi32(out_3,_mm256_unpackhi_epi16(prod,signs));
    }
    __m256i out16_0 = _mm256_srai_epi16(_mm256_packs_epi32(out_0,out_1),6);
    __m256i out16_1 = _mm256_srai_epi16(_mm256_packs_epi32(out_2,out_3),6);
    auto out_vec = static_cast<__m256i*>(output);
    out_vec[0] = _mm256_packs_epi16(out16_0,out16_1);
    if (pack8_and_calc_mask) out_mask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0],k_zero));
    else out_vec[0] = _mm256_max_epi8(out_vec[0],k_zero);
  }

  static void transform(
    const nnboard* pos,
    clipped_t* output,
    mask_t* out_mask){
    if (! update_accumulator(pos)) refresh_accumulator(pos);
    auto& accumulation = pos->nnue[0]->accumulator.accumulation;
    const int perspectives[2] = {pos->player,! pos->player};
    for (unsigned p = 0; p < 2; p++){
      const unsigned offset = k_half_dimensions * p;
      constexpr unsigned num_chunks = 16 * k_half_dimensions / simd_width;
      const auto out = reinterpret_cast<vec8_t*>(&output[offset]);
      for (unsigned i = 0; i < num_chunks / 2; i++){
        const auto acc_ptr = reinterpret_cast<const vec16_t*>(accumulation[perspectives[p]]);
        const size_t base = static_cast<size_t>(i) * 2u;
        const vec16_t s0 = acc_ptr[base];
        const vec16_t s1 = acc_ptr[base + 1];
        out[i] = _mm256_packs_epi16(s0,s1);
        *out_mask++ = _mm256_movemask_epi8(_mm256_cmpgt_epi8(out[i],_mm256_setzero_si256()));
      }
    }
  }
};
