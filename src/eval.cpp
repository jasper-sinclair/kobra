#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "bitboard.h"
#include "eval.h"

#ifdef NNUE_EMBEDDED
INCBIN(Network, NNUE_EVAL_FILE);
#endif

Score evaluate(const Board& pos) {
  int pieces[33]{};
  int squares[33]{};
  int index = 2;
  for (uint8_t i = 0; i < 64; i++) {
    if (pos.PieceOn(i) == 6) {
      pieces[0] = 1;
      squares[0] = i;
    }
    else if (pos.PieceOn(i) == 14) {
      pieces[1] = 7;
      squares[1] = i;
    }
    else if (pos.PieceOn(i) == 1) {
      pieces[index] = 6;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 2) {
      pieces[index] = 5;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 3) {
      pieces[index] = 4;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 4) {
      pieces[index] = 3;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 5) {
      pieces[index] = 2;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 9) {
      pieces[index] = 12;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 10) {
      pieces[index] = 11;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 11) {
      pieces[index] = 10;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 12) {
      pieces[index] = 9;
      squares[index] = i;
      index++;
    }
    else if (pos.PieceOn(i) == 13) {
      pieces[index] = 8;
      squares[index] = i;
      index++;
    }
}
  const int nnue_score = nnue_evaluate(pos.side_to_move, pieces, squares);
  return static_cast<Score>(nnue_score);
}

Fd open_file(const char* name) {
#ifndef _WIN32
  return open(name, O_RDONLY);
#else
  return CreateFile(name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
    FILE_FLAG_RANDOM_ACCESS, nullptr);
#endif
}

void close_file(Fd fd) {
#ifndef _WIN32
  close(fd);
#else
  CloseHandle(fd);
#endif
}

size_t file_size(Fd fd) {
#ifndef _WIN32
  struct stat statbuf;
  fstat(fd, &statbuf);
  return statbuf.st_size;
#else
  DWORD size_high;
  const DWORD size_low = GetFileSize(fd, &size_high);
  return static_cast<uint64_t>(size_high) << 32 | size_low;
#endif
}

const void* map_file(Fd fd, MapT* map) {
#ifndef _WIN32
  * map = file_size(fd);
  void* data = mmap(NULL, *map, PROT_READ, MAP_SHARED, fd, 0);
#ifdef MADV_RANDOM
  madvise(data, *map, MADV_RANDOM);
#endif
  return data == MAP_FAILED ? NULL : data;
#else
  DWORD size_high;
  const DWORD size_low = GetFileSize(fd, &size_high);
  *map = CreateFileMapping(fd, nullptr, PAGE_READONLY, size_high, size_low,
    nullptr);
  if (*map == nullptr) return nullptr;
  return MapViewOfFile(*map, FILE_MAP_READ, 0, 0, 0);
#endif
}

void unmap_file(const void* data, MapT map) {
  if (!data) return;
#ifndef _WIN32
  munmap((void*)data, map);
#else
  UnmapViewOfFile(data);
  CloseHandle(map);
#endif
}

inline int orient(const int c, const int s) {
  return s ^ (c == WHITE ? 0x00 : 0x3f);
}

inline unsigned make_index(const int c, const int s, const int pc,
  const int ksq) {
  return orient(c, s) + piece_to_index[c][pc] + kPsEnd * ksq;
}

namespace anonymous {
  void half_kp_append_active_indices(const NnueBoard* pos, const int c,
    IndexList* active) {
    int ksq = pos->squares[c];
    ksq = orient(c, ksq);
    for (int i = 2; pos->pieces[i]; i++) {
      const int sq = pos->squares[i];
      const int pc = pos->pieces[i];
      active->values[active->size++] = make_index(c, sq, pc, ksq);
    }
  }

  void half_kp_append_changed_indices(const NnueBoard* pos, const int c,
    const DirtyPiece* dp, IndexList* removed,
    IndexList* added) {
    int ksq = pos->squares[c];
    ksq = orient(c, ksq);
    for (int i = 0; i < dp->dirty_num; i++) {
      const int pc = dp->pc[i];
      if (IS_KING(pc)) continue;
      if (dp->from[i] != 64)
        removed->values[removed->size++] = make_index(c, dp->from[i], pc, ksq);
      if (dp->to[i] != 64)
        added->values[added->size++] = make_index(c, dp->to[i], pc, ksq);
    }
  }

  void append_active_indices(const NnueBoard* pos, IndexList active[2]) {
    for (int c = 0; c < 2; c++)
      half_kp_append_active_indices(pos, c, &active[c]);
  }

  void append_changed_indices(const NnueBoard* pos, IndexList removed[2],
    IndexList added[2], bool reset[2]) {
    const DirtyPiece* dp = &pos->nnue[0]->dirty_piece;
    if (pos->nnue[1]->accumulator.computed_accumulation) {
      for (int c = 0; c < 2; c++) {
        reset[c] = dp->pc[0] == static_cast<int>(KING(c));
        if (reset[c])
          half_kp_append_active_indices(pos, c, &added[c]);
        else
          half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
      }
    }
    else {
      const DirtyPiece* dp2 = &pos->nnue[1]->dirty_piece;
      for (int c = 0; c < 2; c++) {
        reset[c] = dp->pc[0] == static_cast<int>(KING(c)) ||
          dp2->pc[0] == static_cast<int>(KING(c));
        if (reset[c])
          half_kp_append_active_indices(pos, c, &added[c]);
        else {
          half_kp_append_changed_indices(pos, c, dp, &removed[c], &added[c]);
          half_kp_append_changed_indices(pos, c, dp2, &removed[c], &added[c]);
        }
      }
    }
  }
} // namespace anonymous

inline int32_t affine_propagate(ClippedT* input, const int32_t* biases,
  WeightT* weights) {
  const auto iv = reinterpret_cast<__m256i*>(input);
  const auto row = reinterpret_cast<__m256i*>(weights);
  __m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
  prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
  __m128i sum = _mm_add_epi32(_mm256_castsi256_si128(prod),
    _mm256_extracti128_si256(prod, 1));
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
  return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
}

inline bool next_idx(unsigned* idx, unsigned* offset, Mask2T* v, MaskT* mask,
  const unsigned in_dims) {
  while (*v == 0) {
    *offset += 8 * sizeof(Mask2T);
    if (*offset >= in_dims) return false;
    memcpy(v, reinterpret_cast<char*>(mask) + *offset / 8, sizeof(Mask2T));
  }
  *idx = *offset + Lsb(*v);
  *v &= *v - 1;
  return true;
}

inline void affine_txfm(int8_t* input, void* output, unsigned in_dims,
  unsigned out_dims, const int32_t* biases,
  const WeightT* weights, MaskT* in_mask, MaskT* out_mask,
  const bool pack8_and_calc_mask) {
  assert(out_dims == 32);
  (void)out_dims;
  const __m256i zero = _mm256_setzero_si256();
  __m256i out_0 = ((__m256i*)biases)[0];
  __m256i out_1 = ((__m256i*)biases)[1];
  __m256i out_2 = ((__m256i*)biases)[2];
  __m256i out_3 = ((__m256i*)biases)[3];
  __m256i first, second;
  Mask2T v;
  unsigned idx;
  memcpy(&v, in_mask, sizeof(Mask2T));

  for (unsigned offset = 0; offset < in_dims;) {
    if (!next_idx(&idx, &offset, &v, in_mask, in_dims)) break;
    first = ((__m256i*)weights)[idx];
    uint16_t factor = input[idx];
    if (next_idx(&idx, &offset, &v, in_mask, in_dims)) {
      second = ((__m256i*)weights)[idx];
      factor |= input[idx] << 8;
    }
    else {
      second = zero;
    }

    __m256i mul = _mm256_set1_epi16(factor), prod, signs;
    prod = _mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
    signs = _mm256_cmpgt_epi16(zero, prod);
    out_0 = _mm256_add_epi32(out_0, _mm256_unpacklo_epi16(prod, signs));
    out_1 = _mm256_add_epi32(out_1, _mm256_unpackhi_epi16(prod, signs));
    prod = _mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
    signs = _mm256_cmpgt_epi16(zero, prod);
    out_2 = _mm256_add_epi32(out_2, _mm256_unpacklo_epi16(prod, signs));
    out_3 = _mm256_add_epi32(out_3, _mm256_unpackhi_epi16(prod, signs));
  }

  __m256i out16_0 = _mm256_srai_epi16(_mm256_packs_epi32(out_0, out_1), kShift);
  __m256i out16_1 = _mm256_srai_epi16(_mm256_packs_epi32(out_2, out_3), kShift);

  auto out_vec = static_cast<__m256i*>(output);
  out_vec[0] = _mm256_packs_epi16(out16_0, out16_1);

  if (pack8_and_calc_mask)
    out_mask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(out_vec[0], zero));
  else
    out_vec[0] = _mm256_max_epi8(out_vec[0], zero);
}

namespace anonymous {
  int16_t ft_biases alignas(64)[kHalfDimensions];
  int16_t ft_weights alignas(64)[kHalfDimensions * kFtInDims];
} // namespace anonymous

inline void refresh_accumulator(const NnueBoard* pos) {
  Accumulator* accumulator = &pos->nnue[0]->accumulator;
  IndexList active_indices[2];
  active_indices[0].size = active_indices[1].size = 0;
  anonymous::append_active_indices(pos, active_indices);

  for (unsigned c = 0; c < 2; c++) {
    for (unsigned i = 0; i < kHalfDimensions / TILE_HEIGHT; i++) {
      const Vec16T* ft_biases_tile =
        reinterpret_cast<Vec16T*>(&anonymous::ft_biases[i * TILE_HEIGHT]);
      const auto acc_tile = reinterpret_cast<Vec16T*>(
        &accumulator->accumulation[c][i * TILE_HEIGHT]);
      Vec16T acc[kNumRegs];
      for (unsigned j = 0; j < kNumRegs; j++) acc[j] = ft_biases_tile[j];
      for (size_t k = 0; k < active_indices[c].size; k++) {
        const unsigned index = active_indices[c].values[k];
        const unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;
        const Vec16T* column =
          reinterpret_cast<Vec16T*>(&anonymous::ft_weights[offset]);
        for (unsigned j = 0; j < kNumRegs; j++)
          acc[j] = VEC_ADD_16(acc[j], column[j]);
      }
      for (unsigned j = 0; j < kNumRegs; j++) acc_tile[j] = acc[j];
    }
  }
  accumulator->computed_accumulation = 1;
}

inline bool update_accumulator(const NnueBoard* pos) {
  Accumulator* accumulator = &pos->nnue[0]->accumulator;
  if (accumulator->computed_accumulation) return true;
  Accumulator* prev_acc;

  if ((!pos->nnue[1] ||
    !(prev_acc = &pos->nnue[1]->accumulator)->computed_accumulation) &&
    (!pos->nnue[2] ||
      !(prev_acc = &pos->nnue[2]->accumulator)->computed_accumulation))
    return false;

  IndexList removed_indices[2], added_indices[2];
  removed_indices[0].size = removed_indices[1].size = 0;
  added_indices[0].size = added_indices[1].size = 0;
  bool reset[2];
  anonymous::append_changed_indices(pos, removed_indices, added_indices, reset);

  for (unsigned i = 0; i < kHalfDimensions / TILE_HEIGHT; i++) {
    for (unsigned c = 0; c < 2; c++) {
      const auto acc_tile = reinterpret_cast<Vec16T*>(
        &accumulator->accumulation[c][i * TILE_HEIGHT]);
      Vec16T acc[kNumRegs];
      if (reset[c]) {
        const Vec16T* ft_b_tile =
          reinterpret_cast<Vec16T*>(&anonymous::ft_biases[i * TILE_HEIGHT]);
        for (unsigned j = 0; j < kNumRegs; j++) acc[j] = ft_b_tile[j];
      }
      else {
        const Vec16T* prev_acc_tile = reinterpret_cast<Vec16T*>(
          &prev_acc->accumulation[c][i * TILE_HEIGHT]);
        for (unsigned j = 0; j < kNumRegs; j++) acc[j] = prev_acc_tile[j];
        for (unsigned k = 0; k < removed_indices[c].size; k++) {
          const unsigned index = removed_indices[c].values[k];
          const unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;
          const Vec16T* column =
            reinterpret_cast<Vec16T*>(&anonymous::ft_weights[offset]);
          for (unsigned j = 0; j < kNumRegs; j++)
            acc[j] = VEC_SUB_16(acc[j], column[j]);
        }
      }
      for (unsigned k = 0; k < added_indices[c].size; k++) {
        const unsigned index = added_indices[c].values[k];
        const unsigned offset = kHalfDimensions * index + i * TILE_HEIGHT;
        const Vec16T* column =
          reinterpret_cast<Vec16T*>(&anonymous::ft_weights[offset]);
        for (unsigned j = 0; j < kNumRegs; j++)
          acc[j] = VEC_ADD_16(acc[j], column[j]);
      }
      for (unsigned j = 0; j < kNumRegs; j++) acc_tile[j] = acc[j];
    }
  }
  accumulator->computed_accumulation = 1;
  return true;
}

inline void transform(const NnueBoard* pos, ClippedT* output, MaskT* out_mask) {
  if (!update_accumulator(pos)) refresh_accumulator(pos);
  int16_t(*accumulation)[2][256] = &pos->nnue[0]->accumulator.accumulation;
  (void)out_mask;
  const int perspectives[2] = { pos->player, !pos->player };

  for (unsigned p = 0; p < 2; p++) {
    const unsigned offset = kHalfDimensions * p;
    constexpr unsigned num_chunks = 16 * kHalfDimensions / kSimdWidth;
    const auto out = reinterpret_cast<Vec8T*>(&output[offset]);
    for (unsigned i = 0; i < num_chunks / 2; i++) {
      const Vec16T s0 =
        reinterpret_cast<Vec16T*>((*accumulation)[perspectives[p]])[i * 2];
      const Vec16T s1 = reinterpret_cast<Vec16T*>(
        (*accumulation)[perspectives[p]])[i * 2 + 1];
      out[i] = VEC_PACKS(s0, s1);
      *out_mask++ = VEC_MASK_POS(out[i]);
    }
  }
}

struct NetData {
  alignas(64) ClippedT input[kFtOutDims];
  ClippedT hidden1_out[32];
  int8_t hidden2_out[32];
};

int nnue_evaluate_pos(const NnueBoard* pos) {
  alignas(8) MaskT input_mask[kFtOutDims / (8 * sizeof(MaskT))];
  alignas(8) MaskT hidden1_mask[8 / sizeof(MaskT)] = {};
  NetData buf;
#define B(x) (buf.x)
  transform(pos, B(input), input_mask);
  affine_txfm(B(input), B(hidden1_out), kFtOutDims, 32, hidden1_biases,
    hidden1_weights, input_mask, hidden1_mask, true);
  affine_txfm(B(hidden1_out), B(hidden2_out), 32, 32, hidden2_biases,
    hidden2_weights, hidden1_mask, nullptr, false);
  const int32_t out_value =
    affine_propagate(B(hidden2_out), output_biases, output_weights);
  return out_value / kFvScale;
}

inline unsigned wt_idx(const unsigned r, unsigned c, const unsigned dims) {
  (void)dims;
  if (dims > 32) {
    unsigned b = c & 0x18;
    b = b << 1 | b >> 1;
    c = (c & ~0x18) | (b & 0x18);
  }
  return c * 32 + r;
}

namespace anonymous {
  const char* read_hidden_weights(WeightT* w, const unsigned dims,
    const char* d) {
    for (unsigned r = 0; r < 32; r++)
      for (unsigned c = 0; c < dims; c++) w[wt_idx(r, c, dims)] = *d++;
    return d;
  }

  void permute_biases(int32_t* biases) {
    const auto b = reinterpret_cast<__m128i*>(biases);
    __m128i tmp[8];
    tmp[0] = b[0];
    tmp[1] = b[4];
    tmp[2] = b[1];
    tmp[3] = b[5];
    tmp[4] = b[2];
    tmp[5] = b[6];
    tmp[6] = b[3];
    tmp[7] = b[7];
    memcpy(b, tmp, 8 * sizeof(__m128i));
  }
} // namespace anonymous

enum {
  kTransformerStart = 3 * 4 + 177,
  kNetworkStart = kTransformerStart + 4 + 2 * 256 + 2 * 256 * 64 * 641
};

inline uint32_t readu_le_u32(const void* p) {
  const auto q = static_cast<const uint8_t*>(p);
  return q[0] | q[1] << 8 | q[2] << 16 | q[3] << 24;
}

inline uint16_t readu_le_u16(const void* p) {
  const auto q = static_cast<const uint8_t*>(p);
  return q[0] | q[1] << 8;
}

namespace anonymous {
  void read_output_weights(WeightT* w, const char* d) {
    for (unsigned i = 0; i < 32; i++) {
      const unsigned c = i;
      w[c] = *d++;
    }
  }

  bool verify_net(const void* eval_data, const size_t size) {
    if (size != 21022697) return false;
    const auto d = static_cast<const char*>(eval_data);
    if (readu_le_u32(d) != kNnueVersion) return false;
    if (readu_le_u32(d + 4) != 0x3e5aa6eeU) return false;
    if (readu_le_u32(d + 8) != 177) return false;
    if (readu_le_u32(d + kTransformerStart) != 0x5d69d7b8) return false;
    if (readu_le_u32(d + kNetworkStart) != 0x63337156) return false;
    return true;
  }

  void init_weights(const void* eval_data) {
    const char* d = static_cast<const char*>(eval_data) + kTransformerStart + 4;
    for (unsigned i = 0; i < kHalfDimensions; i++, d += 2)
      ft_biases[i] = readu_le_u16(d);
    for (unsigned i = 0; i < kHalfDimensions * kFtInDims; i++, d += 2)
      ft_weights[i] = readu_le_u16(d);
    d += 4;
    for (unsigned i = 0; i < 32; i++, d += 4) hidden1_biases[i] = readu_le_u32(d);
    d = read_hidden_weights(hidden1_weights, 512, d);
    for (unsigned i = 0; i < 32; i++, d += 4) hidden2_biases[i] = readu_le_u32(d);
    d = read_hidden_weights(hidden2_weights, 32, d);
    for (unsigned i = 0; i < 1; i++, d += 4) output_biases[i] = readu_le_u32(d);
    read_output_weights(output_weights, d);
    permute_biases(hidden1_biases);
    permute_biases(hidden2_biases);
  }

  bool load_eval_file(const char* eval_file) {
    const void* eval_data;
    MapT mapping;
    size_t size;
#if !defined(_MSC_VER) && defined(NNUE_EMBEDDED)
    if (strcmp(eval_file, NNUE_EVAL_FILE) == 0) {
      eval_data = gNetworkData;
      mapping = 0;
      size = gNetworkSize;
    }
    else
#endif
    {
      const Fd fd = open_file(eval_file);
      if (fd == FD_ERR) return false;
      eval_data = map_file(fd, &mapping);
      size = file_size(fd);
      close_file(fd);
    }
    const bool success = verify_net(eval_data, size);
    if (success) init_weights(eval_data);
    if (mapping) unmap_file(eval_data, mapping);
    return success;
  }
} // namespace anonymous

int nnue_init(const char* eval_file) {
  if (anonymous::load_eval_file(eval_file)) {
    std::cout << eval_file << " loaded" << std::endl;
    return fflush(stdout);
  }
  std::cout << eval_file << " not found" << std::endl;
  return fflush(stdout);
}

int nnue_evaluate(const int player, int* pieces, int* squares) {
  NnueData nnue;
  nnue.accumulator.computed_accumulation = 0;
  NnueBoard pos;
  pos.nnue[0] = &nnue;
  pos.nnue[1] = nullptr;
  pos.nnue[2] = nullptr;
  pos.player = player;
  pos.pieces = pieces;
  pos.squares = squares;
  return nnue_evaluate_pos(&pos);
}