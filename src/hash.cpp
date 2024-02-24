#include "hash.h"

#include <cstring>
#include <memory>
#include "main.h"

void HashTable::SetSize(const uint64_t mb) {
  const uint64_t bytes = mb * 1024 * 1024;
  const uint64_t max_size = bytes / sizeof(HashEntry);
  size = 1;
  for (;;) {
    const uint64_t new_size = 2 * size;
    if (new_size > max_size) break;
    size = new_size;
  }
  mask = size - 1;
  entries = std::make_unique<HashEntry[]>(size);
  clear();
}

void HashTable::clear() const {
  std::memset(entries.get(), 0, size * sizeof(HashEntry));
}

double HashTable::usage() const {
  int cnt = 0;
  constexpr int n = 1000;
  for (int i = 1; i < n + 1; ++i) {
    if (entries[i].key) cnt += 1;
  }
  return static_cast<double>(cnt) / n;
}

bool HashTable::probe(const Key key, HashEntry& hash_entry) {
  hash_entry = *get(key);
  if ((hash_entry.key ^ hash_entry.data) == key) return true;
  hash_entry = {};
  return false;
}

void HashTable::save(const Key key, const Score score,
                              const Score static_eval, const Move move,
                              const Depth depth, const NodeType node_type) {
  HashEntry tmp;
  tmp.score = score;
  tmp.static_eval = static_eval;
  tmp.move = move;
  tmp.depth = static_cast<uint8_t>(depth);
  tmp.node_type = node_type;
  tmp.key = key ^ tmp.data;
  if (HashEntry* hash_entry = get(key); node_type == PV_NODE ||
                               key != (hash_entry->key ^ hash_entry->data) ||
                               depth + 4 > hash_entry->depth) {
    *hash_entry = tmp;
  }
}

Score HashTable::ScoreToHash(const Score score, const Depth ply) {
  return static_cast<Score>(score >= MIN_MATE_SCORE    ? score + ply
                            : score <= -MIN_MATE_SCORE ? score - ply
                                                       : score);
}

Score HashTable::ScoreFromHash(const Score score, const Depth ply) {
  return static_cast<Score>(score >= MIN_MATE_SCORE    ? score - ply
                            : score <= -MIN_MATE_SCORE ? score + ply
                                                       : score);
}
