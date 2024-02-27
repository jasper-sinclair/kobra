#pragma once
#include <thread>

#include "chrono.h"
#include "hash.h"
#include "movesort.h"

using ThreadId = uint32_t;
constexpr int kMinDisplayTime = 5000;
constexpr int kMaxThreads = 256;

enum SearchType { kNonPv, PV, kRoot };

struct Stack {
  Move pv[kMaxDepth];

  int ply;
  int pv_size;

  Move hash_move;
  Move move;
  Piece moved;
  Score static_eval;
};

struct ThreadData {
  Depth root_depth;
  Depth sel_depth;

  Histories histories;
  Stack stack[kMaxPly + kContinuationPly];
  std::vector<Move> pv;
  ThreadId id;
  uint64_t node_count;
  ThreadData() : root_depth(0), sel_depth(0), stack{}, id(0), node_count(0) {}

  explicit ThreadData(const ThreadId id)
    : root_depth(0), sel_depth(0), stack{}, id(id), node_count(0) {}
};

struct Search {
  [[nodiscard]] std::string info(const ThreadData& td, Depth depth,
                                 Score score) const;
  [[nodiscard]] uint64_t NodeCount() const;

  Chrono time;
  constexpr static int16_t lmr_factor = 1000;

  inline static Depth forward_pruning_table[kMaxDepth][kMaxMoves];
  inline static Depth log_reduction_table[kMaxDepth][kMaxMoves];
  inline static Depth move_count_pruning_table[kMaxDepth];

  static void init();

  std::vector<std::thread> threads;
  std::vector<ThreadData*> thread_data;

  template <bool MainThread = true>
  Move BestMove(Board& board, ThreadId id = 0);
  template <SearchType St, bool SkipHashMove = false>
  Score AlphaBeta(Board& board, Score alpha, Score beta, Depth depth,
                  ThreadData& td, Stack* ss);
  template <SearchType St>
  Score quiescence(Board& board, Score alpha, Score beta, ThreadData& td,
                   Stack* ss);

  ThreadId num_threads = 1;
  HashTable hash;

  void clear();
  void SetNumThreads(ThreadId threadnum);
  void SetHashSize(size_t mb);
  void stop();
};
