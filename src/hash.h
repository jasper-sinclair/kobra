#pragma once

#include"bitboard.h"

using NodeType = uint8_t;

enum NodeTypes {
	NONE_NODE, PV_NODE, CUT_NODE, ALL_NODE
};

struct TTEntry {
	Key key;

	union {
		struct {
			Score score;
			Score eval;
			Move move;
			uint8_t depth;
			NodeType nodeType;
		};
		uint64_t data;
	};
};

constexpr size_t MAX_TT_SIZE = 1 << 20;

struct TranspositionTable {
	std::unique_ptr<TTEntry[]>entries;
	uint64_t size;
	uint64_t mask;

	void setSize(uint64_t MiB);
	void clear();
	TTEntry* get(Key key) {
		return &entries[key & mask];
	}
	bool probe(Key key, TTEntry& tte);
	void save(Key key, Score score, Score eval, Move move, Depth depth, NodeType nodeType);
	double usage();
	static Score scoreToTT(Score score, Depth ply);
	static Score scoreFromTT(Score score, Depth ply);
};