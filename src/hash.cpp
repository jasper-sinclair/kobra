#include "hash.h"

#include <memory>

void HashTable::SetSize(uint64_t mb) {
  uint64_t bytes = mb * 1024 * 1024;
  uint64_t max_size = bytes / sizeof(HashEntry);

  size = 1;
  for (;;) {
    uint64_t new_size = 2 * size;
    if (new_size > max_size) break;
    size = new_size;
  }

  mask = size - 1;
  entries = std::make_unique<HashEntry[]>(size);
  clear();
}

void HashTable::clear() {
  std::memset(entries.get(), 0, size * sizeof(HashEntry));
}

double HashTable::usage() {
  int cnt = 0;
  int n = 1000;
  for (int i = 1; i < n + 1; ++i) {
    if (entries[i].key) cnt += 1;
  }
  return (double)cnt / n;
}

bool HashTable::probe(Key key, HashEntry& tte) {
  tte = *get(key);

  if ((tte.key ^ tte.data) == key)
    return true;

  else {
    tte = {};
    return false;
  }
}

void HashTable::save(Key key, Score score, Score eval, uint16_t move,
                     Depth depth) {
  HashEntry tmp;
  tmp.score = score;
  tmp.eval = eval;
  tmp.move = move;
  tmp.depth = static_cast<uint8_t>(depth);

  tmp.key = key ^ tmp.data;

  HashEntry* he = get(key);

  if (key != (he->key ^ he->data) || depth + 4 > he->depth) {
    *he = tmp;
  }
}
