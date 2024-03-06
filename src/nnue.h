#pragma once
#ifdef _WIN32
#include <Windows.h>
#include <bit>
#include <string>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <immintrin.h>

#include "main.h"

#ifdef _WIN32
using Fd = HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
using MapT = HANDLE;
#else
typedef int Fd;
#define FD_ERR -1
typedef size_t MapT;
#endif

#ifdef _MSC_VER
#else
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

#if defined(__GNUC__) && (__GNUC__ < 9) && defined(_WIN32) && \
    !defined(__clang__) && !defined(__INTEL_COMPILER) && defined(USE_AVX2)
#define ALIGNMENT_HACK
#endif

#if defined(_MSC_VER)
inline int popcnt(const uint64_t bb) { return std::popcount(bb); }

inline Square Lsb(const uint64_t bb) {
  return static_cast<Square>(std::countr_zero(bb));
}

inline Square Msb(const uint64_t bb) {
  unsigned long idx;
  _BitScanReverse64(&idx, bb);
  return static_cast<Square>(idx);
}

inline void prefetch(void* address) {
  _mm_prefetch(static_cast<char*>(address), _MM_HINT_T0);
}

#elif defined(__GNUC__)
inline int popcnt(uint64_t bb) { return __builtin_popcountll(bb); }
inline Square Lsb(uint64_t bb) {
  return static_cast<Square>(__builtin_ctzll(bb));
}
inline Square Msb
(uint64_t bb) {
  return static_cast<Square>(63 ^ __builtin_clzll(bb));
}
inline void prefetch(void* address) { __builtin_prefetch(address); }
#endif

#define KING(c) ((c) ? kBking : kWking)
#define IS_KING(p) (((p) == kWking) || ((p) == kBking))
#ifdef _MSC_VER
#define USE_AVX2 1
#define USE_SSE41 1
#define USE_SSE3 1
#define USE_SSE2 1
#define USE_SSE 1
#define IS_64_BIT 1
#endif
#define TILE_HEIGHT (kNumRegs * kSimdWidth / 16)
#define CLAMP(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))

#define VEC_ADD_16(a, b) _mm256_add_epi16(a, b)
#define VEC_SUB_16(a, b) _mm256_sub_epi16(a, b)
#define VEC_PACKS(a, b) _mm256_packs_epi16(a, b)
#define VEC_MASK_POS(a) \
  _mm256_movemask_epi8(_mm256_cmpgt_epi8(a, _mm256_setzero_si256()))

#if !defined(_MSC_VER)
#define NNUE_EMBEDDED
#define NNUE_EVAL_FILE "kobra_1.0.nnue"
#endif

enum NnuePieces {
  kBlank = 0,
  kWking,
  kWqueen,
  kWrook,
  kWbishop,
  kWknight,
  kWpawn,
  kBking,
  kBqueen,
  kBrook,
  kBbishop,
  kBknight,
  kBpawn
};

enum {
  kPsWPawn = 1,
  kPsBPawn = 1 * 64 + 1,
  kPsWKnight = 2 * 64 + 1,
  kPsBKnight = 3 * 64 + 1,
  kPsWBishop = 4 * 64 + 1,
  kPsBBishop = 5 * 64 + 1,
  kPsWRook = 6 * 64 + 1,
  kPsBRook = 7 * 64 + 1,
  kPsWQueen = 8 * 64 + 1,
  kPsBQueen = 9 * 64 + 1,
  kPsEnd = 10 * 64 + 1
};

enum { kFvScale = 16, kShift = 6 };

enum {
  kHalfDimensions = 256,
  kFtInDims = 64 * kPsEnd,
  kFtOutDims = kHalfDimensions * 2
};

enum { kNumRegs = 16, kSimdWidth = 256 };

using DirtyPiece = struct DirtyPiece {
  int dirty_num;
  int pc[3];
  int from[3];
  int to[3];
};

using Accumulator = struct Accumulator {
  alignas(64) int16_t accumulation[2][256];
  int computed_accumulation;
};

using NnueData = struct NnueData {
  Accumulator accumulator;
  DirtyPiece dirty_piece;
};

using NnueBoard = struct NnueBoard {
  int player;
  int* pieces;
  int* squares;
  NnueData* nnue[3];
};

using Vec16T = __m256i;
using Vec8T = __m256i;
using MaskT = uint32_t;
using Mask2T = uint64_t;
using ClippedT = int8_t;
using WeightT = int8_t;

using IndexList = struct {
  size_t size;
  unsigned values[30];
};

inline std::string nnue_evalfile = "kobra_1.0.nnue";
inline const char* nnue_file = nnue_evalfile.c_str();
inline constexpr uint32_t kNnueVersion = 0x7AF32F16u;

inline int32_t hidden1_biases alignas(64)[32];
inline int32_t hidden2_biases alignas(64)[32];
inline int32_t output_biases[1];

inline WeightT hidden1_weights alignas(64)[64 * 512];
inline WeightT hidden2_weights alignas(64)[64 * 32];
inline WeightT output_weights alignas(64)[1 * 32];

inline uint32_t piece_to_index[2][14] = {
  {
    0, 0, kPsWQueen, kPsWRook, kPsWBishop, kPsWKnight, kPsWPawn, 0,
    kPsBQueen, kPsBRook, kPsBBishop, kPsBKnight, kPsBPawn, 0
  },
  {
    0, 0, kPsBQueen, kPsBRook, kPsBBishop, kPsBKnight, kPsBPawn, 0,
    kPsWQueen, kPsWRook, kPsWBishop, kPsWKnight, kPsWPawn, 0
  }
};

int nnue_evaluate_pos(const NnueBoard* pos);
int nnue_evaluate(int player, int* pieces, int* squares);
int nnue_init(const char* eval_file);

size_t file_size(Fd fd);

const void* map_file(Fd fd, MapT* map);
Fd open_file(const char* name);
void close_file(Fd fd);
void unmap_file(const void* data, MapT map);
