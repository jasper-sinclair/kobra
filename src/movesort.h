#pragma once

#include"history.h"

struct MoveSorter {
	Board& board;
	Stack* ss;
	Histories& histories;
	MoveList moves;
	uint16_t hash_move;
	bool is_in_check;
	int idx;
	int stage;

	enum Stages {
		HASH_MOVE,
		NORMAL_INIT,
		NORMAL
	};

	// Generate all legal moves and sort them
	MoveSorter(
		Board& board, 
		Stack* ss, 
		Histories& histories, 
		uint16_t hash_move, 
		bool is_in_check
	);

	uint16_t next();
	Score ComputeScore(uint16_t m);
};