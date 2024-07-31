#pragma once
#include <memory>

#include "bitboard.h"
#include "main.h"

#ifdef _MSC_VER
#pragma warning(disable : 4201)
#else
#endif

using NodeType = uint8_t;
enum NodeTypes { NONE_NODE, PV_NODE, CUT_NODE, ALL_NODE };

struct HashEntry {
  Key key;

  union {
    struct {
      Score score;
      Score static_eval;
      Move move;
      uint8_t depth;
      NodeType node_type;
    };

    uint64_t data;
  };
};

constexpr size_t kMaxHashSize = 1 << 20;

struct HashTable {
  [[nodiscard]] double usage() const;
  bool probe(Key key, HashEntry& hash_entry);

  static Score ScoreFromHash(Score score, Depth ply);
  static Score ScoreToHash(Score score, Depth ply);

  std::unique_ptr<HashEntry[]> entries;

  HashEntry* get(const Key key) { return &entries[key & mask]; }

  uint64_t mask = 0;
  uint64_t size = 0;

  void clear() const;
  void save(Key key, Score score, Score static_eval, Move move, Depth depth,
    NodeType node_type);
  void SetSize(uint64_t mb);
};
