#include<memory>

#include"hash.h"

// mebibytes (1024^2 bytes)
void TranspositionTable::setSize(uint64_t MiB) {
   const uint64_t bytes = MiB * 1024 * 1024;
   const uint64_t maxSize = bytes / sizeof(TTEntry);

	size = 1;
	for (;;) {
      const uint64_t newSize = 2 * size;
		if (newSize > maxSize)
			break;
		size = newSize;
	}

	mask = size - 1;
	entries = std::make_unique<TTEntry[]>(size);
	clear();
}

void TranspositionTable::clear() {
	std::memset(entries.get(), 0, size * sizeof(TTEntry));
}

double TranspositionTable::usage() {
	int cnt = 0;
   constexpr int n = 1000;
	for (int i = 1; i < n + 1; ++i) {
		if (entries[i].key)
			cnt += 1;
	}
	return static_cast<double>(cnt) / n;
}

bool TranspositionTable::probe(Key key, TTEntry& tte) {
	tte = *get(key);

	if ((tte.key ^ tte.data) == key)
		return true;

	else {
		tte = {};
		return false;
	}
}

void TranspositionTable::save(Key key, Score score, Score eval, Move move, Depth depth, NodeType nodeType) {
	TTEntry tmp;
	tmp.score = score;
	tmp.eval = eval;
	tmp.move = move;
	tmp.depth = static_cast<uint8_t>(depth);
	tmp.nodeType = nodeType;

	tmp.key = key ^ tmp.data;

	TTEntry* tte = get(key);

	if (nodeType == PV_NODE ||
		key != (tte->key ^ tte->data) ||
		depth + 4 > tte->depth) 
	{
		*tte = tmp;
	}
}

Score TranspositionTable::scoreToTT(Score score, Depth ply) {
	return
		score >= MIN_MATE_SCORE ? score + ply :
		score <= -MIN_MATE_SCORE ? score - ply :
		score;
}

Score TranspositionTable::scoreFromTT(Score score, Depth ply) {
	return 
		score >= MIN_MATE_SCORE ? score - ply :
		score <= -MIN_MATE_SCORE ? score + ply :
		score;
}