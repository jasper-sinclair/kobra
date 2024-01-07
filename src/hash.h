#pragma once
#include <memory>

#include "bitboard.h"

using NodeType = uint8_t;

struct HashEntry {
  Key key;

  union {
    struct {
      Score score;
      Score eval;
      uint16_t move;
      uint8_t depth;
      NodeType node_type;
    };
    uint64_t data;
  };
};

constexpr size_t kMaxHashSize = 1 << 20;

struct HashTable {
  std::unique_ptr<HashEntry[]> entries;
  uint64_t size;
  uint64_t mask;

  void SetSize(uint64_t mb);
  void clear();
  HashEntry* get(Key key) { return &entries[key & mask]; }
  bool probe(Key key, HashEntry& tte);
  void save(Key key, Score score, Score eval, uint16_t move, Depth depth);
  double usage();
};