#pragma once
#include <array>

#include "movegen.h"

#ifdef _MSC_VER
#define NOMINMAX
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <bit>
#include <string>
#include <Windows.h>
using Fd = HANDLE;
using MapT = HANDLE;
#define FD_ERR INVALID_HANDLE_VALUE
#else
#include <sys/mman.h>
#include <unistd.h>
typedef int Fd;
#define FD_ERR -1
typedef size_t MapT;
#endif

#include <immintrin.h>
#include "main.h"

#ifdef _MSC_VER
#else
#pragma GCC diagnostic ignored "-Wcast-qual"
#define NNUE_EMBEDDED
#define NNUE_EVAL_FILE "kobra_1.1.nnue"
#endif

#if defined(_MSC_VER)
inline int popcnt(const uint64_t bb) { return std::popcount(bb); }

inline Square Lsb(const uint64_t bb) {
  return static_cast<Square>(std::countr_zero(bb));
}
#elif defined(__GNUC__)
inline int popcnt(uint64_t bb) { return __builtin_popcountll(bb); }
inline Square Lsb(uint64_t bb) {
  return static_cast<Square>(__builtin_ctzll(bb));
}
#endif

#ifdef _MSC_VER
#define USE_AVX2 1
#define USE_SSE41 1
#define USE_SSE3 1
#define USE_SSE2 1
#define USE_SSE 1
#define IS_64_BIT 1
#endif

#define TILE_HEIGHT (kNumRegs * kSimdWidth / 16)
#define CLAMP(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))µ
#define KING(c) ((c) ? kBking : kWking)
#define IS_KING(p) (((p) == kWking) || ((p) == kBking))

#define VEC_ADD_16(a, b) _mm256_add_epi16(a, b)
#define VEC_SUB_16(a, b) _mm256_sub_epi16(a, b)
#define VEC_PACKS(a, b) _mm256_packs_epi16(a, b)
#define VEC_MASK_POS(a) _mm256_movemask_epi8(_mm256_cmpgt_epi8(a, _mm256_setzero_si256()))

#pragma once
#include <climits>

#if defined(__AVX512BW__) || defined(__AVX512CD__) || defined(__AVX512DQ__) || \
    defined(__AVX512ER__) || defined(__AVX512PF__) || defined(__AVX512VL__) || \
    defined(__AVX512F__)
#define INCBIN_ALIGNMENT_INDEX 6
#elif defined(__AVX__) || defined(__AVX2__)
#define INCBIN_ALIGNMENT_INDEX 5
#elif defined(__SSE__) || defined(__SSE2__) || defined(__SSE3__) || \
    defined(__SSSE3__) || defined(__SSE4_1__) || defined(__SSE4_2__) || \
    defined(__neon__)
#define INCBIN_ALIGNMENT_INDEX 4
#elif ULONG_MAX != 0xffffffffu
#define INCBIN_ALIGNMENT_INDEX 3
#else
#define INCBIN_ALIGNMENT_INDEX 2
#endif

#define INCBIN_ALIGN_SHIFT_0 1
#define INCBIN_ALIGN_SHIFT_1 2
#define INCBIN_ALIGN_SHIFT_2 4
#define INCBIN_ALIGN_SHIFT_3 8
#define INCBIN_ALIGN_SHIFT_4 16
#define INCBIN_ALIGN_SHIFT_5 32
#define INCBIN_ALIGN_SHIFT_6 64

#define INCBIN_ALIGNMENT                                        \
  INCBIN_CONCATENATE(INCBIN_CONCATENATE(INCBIN_ALIGN_SHIFT, _), \
                     INCBIN_ALIGNMENT_INDEX)
#define INCBIN_STR(X) #X
#define INCBIN_STRINGIZE(X) INCBIN_STR(X)
#define INCBIN_CAT(X, Y) X##Y
#define INCBIN_CONCATENATE(X, Y) INCBIN_CAT(X, Y)
#define INCBIN_EVAL(X) X
#define INCBIN_INVOKE(N, ...) INCBIN_EVAL(N(__VA_ARGS__))

#define INCBIN_MACRO ".incbin"

#ifndef _MSC_VER
#define INCBIN_ALIGN __attribute__((aligned(INCBIN_ALIGNMENT)))
#else
#define INCBIN_ALIGN __declspec(align(INCBIN_ALIGNMENT))
#endif

#ifdef __GNUC__
#define INCBIN_ALIGN_HOST ".balign " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#define INCBIN_ALIGN_BYTE ".balign 1\n"
#elif defined(INCBIN_ARM)
#define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT_INDEX) "\n"
#define INCBIN_ALIGN_BYTE ".align 0\n"
#else
#define INCBIN_ALIGN_HOST ".align " INCBIN_STRINGIZE(INCBIN_ALIGNMENT) "\n"
#define INCBIN_ALIGN_BYTE ".align 1\n"
#endif

#if defined(__cplusplus)
#define INCBIN_EXTERNAL extern "C"
#define INCBIN_CONST extern const
#else
#define INCBIN_EXTERNAL extern
#define INCBIN_CONST const
#endif

#if !defined(INCBIN_OUTPUT_SECTION)
#if defined(__APPLE__)
#define INCBIN_OUTPUT_SECTION ".const_data"
#else
#define INCBIN_OUTPUT_SECTION ".rodata"
#endif
#endif

#define INCBIN_SECTION ".section " INCBIN_OUTPUT_SECTION "\n"
#define INCBIN_GLOBAL(NAME) ".global " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME "\n"

#define INCBIN_INT ".int "

#if defined(__USER_LABEL_PREFIX__)
#define INCBIN_MANGLE INCBIN_STRINGIZE(__USER_LABEL_PREFIX__)
#else
#define INCBIN_MANGLE ""
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#define INCBIN_TYPE(NAME)
#else
#define INCBIN_TYPE(NAME) ".type " INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME ", @object\n"
#endif

#define INCBIN_BYTE ".byte "

#define INCBIN_STYLE_CAMEL 0
#define INCBIN_STYLE_SNAKE 1

#if !defined(INCBIN_PREFIX)
#define INCBIN_PREFIX g
#endif

#if !defined(INCBIN_STYLE)
#define INCBIN_STYLE INCBIN_STYLE_CAMEL
#endif

#define INCBIN_STYLE_0_DATA Data
#define INCBIN_STYLE_0_END End
#define INCBIN_STYLE_0_SIZE Size
#define INCBIN_STYLE_1_DATA _data
#define INCBIN_STYLE_1_END _end
#define INCBIN_STYLE_1_SIZE _size

#define INCBIN_STYLE_IDENT(TYPE)  \
  INCBIN_CONCATENATE(INCBIN_STYLE_, INCBIN_CONCATENATE(INCBIN_EVAL(INCBIN_STYLE), INCBIN_CONCATENATE(_, TYPE)))

#define INCBIN_STYLE_STRING(TYPE) INCBIN_STRINGIZE(INCBIN_STYLE_IDENT(TYPE))

#define INCBIN_GLOBAL_LABELS(NAME, TYPE) \
  INCBIN_INVOKE(INCBIN_GLOBAL, INCBIN_CONCATENATE(NAME, INCBIN_INVOKE(INCBIN_STYLE_IDENT, TYPE))) \
  INCBIN_INVOKE(INCBIN_TYPE, INCBIN_CONCATENATE(NAME, INCBIN_INVOKE(INCBIN_STYLE_IDENT, TYPE)))

#define INCBIN_EXTERN(NAME) INCBIN_EXTERNAL const INCBIN_ALIGN unsigned char INCBIN_CONCATENATE( \
  INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), INCBIN_STYLE_IDENT(DATA))[]; \
  INCBIN_EXTERNAL const INCBIN_ALIGN unsigned char *const INCBIN_CONCATENATE( \
  INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), INCBIN_STYLE_IDENT(END)); \
  INCBIN_EXTERNAL const unsigned int INCBIN_CONCATENATE(INCBIN_CONCATENATE(INCBIN_PREFIX, NAME), \
  INCBIN_STYLE_IDENT(SIZE))

#ifdef _MSC_VER
#define INCBIN(NAME, FILENAME) INCBIN_EXTERN(NAME)
#else
#define INCBIN(NAME, FILENAME) \
__asm__(INCBIN_SECTION INCBIN_GLOBAL_LABELS(NAME, DATA) INCBIN_ALIGN_HOST INCBIN_MANGLE \
INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) ":\n" INCBIN_MACRO " \"" FILENAME "\"\n" \
INCBIN_GLOBAL_LABELS(NAME, END) INCBIN_ALIGN_BYTE INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME \
INCBIN_STYLE_STRING(END) ":\n" INCBIN_BYTE "1\n" INCBIN_GLOBAL_LABELS(NAME, SIZE) \
INCBIN_ALIGN_HOST INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(SIZE) ":\n" \
INCBIN_INT INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(END) " - " \
INCBIN_MANGLE INCBIN_STRINGIZE(INCBIN_PREFIX) #NAME INCBIN_STYLE_STRING(DATA) "\n" INCBIN_ALIGN_HOST ".text\n"); \
INCBIN_EXTERN(NAME)
#endif

namespace eval {
enum GamePhase { MID_GAME, END_GAME, N_GAME_PHASES };

enum Ptype : uint8_t {
  kNoPiece,
  kWKing = 1,
  kWPawn = 2,
  kWKnight = 3,
  kWBishop = 4,
  kWRook = 5,
  kWQueen = 6,
  kBKing = 9,
  kBPawn = 10,
  kBKnight = 11,
  kBBishop = 12,
  kBRook = 13,
  kBQueen = 14,
};

constexpr std::array<Score, 7> kPtValues = {0, 100, 330, 350, 525, 1100, 8000};

constexpr std::array<Score, 16> kPieceValues = {
    0,
    kPtValues[1],
    kPtValues[2],
    kPtValues[3],
    kPtValues[4],
    kPtValues[5],
    kPtValues[6],
    0,
    0,
    kPtValues[1],
    kPtValues[2],
    kPtValues[3],
    kPtValues[4],
    kPtValues[5],
    kPtValues[6],
    0,
};
}    

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

inline std::string nnue_evalfile = "kobra_1.1.nnue";
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

Score evaluate(const Board& pos);
int nnue_evaluate_pos(const NnueBoard* pos);
int nnue_evaluate(int player, int* pieces, int* squares);
int nnue_init(const char* eval_file);

size_t file_size(Fd fd);

const void* map_file(Fd fd, MapT* map);
Fd open_file(const char* name);
void close_file(Fd fd);
void unmap_file(const void* data, MapT map);
