#pragma once

#include <thread>
#include <vector>
#include "board.h"
#include "hash.h"

using ThreadId = uint32_t;
constexpr int kMaxThreads = 256;

struct Stack {
  uint16_t pv[kMaxDepth];
  int pv_size;
  int ply;
  Piece moved;
  uint16_t move;
  uint16_t hash_move;
};

struct ThreadData {
  ThreadId id;
  Stack stack[kMaxPly];
  std::vector<uint16_t> pv;
  uint64_t node_count;
  Depth root_depth;
  ThreadData(ThreadId id) : id(id) {}
};

struct Search {
  HashTable tt;
  std::vector<std::thread> threads;
  std::vector<ThreadData*> thread_data;
  ThreadId num_threads = 1;
  uint16_t RandMove(Board& board, ThreadId id = 0);
};