#pragma once

#include"history.h"

struct MoveSorter {
	Board& board;
	Stack* ss;
	Histories& histories;
	MoveList moves;
	Move hashMove;
	bool isInCheck;
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
		Move hashMove, 
		bool isInCheck
	);

	Move next();
	Score computeScore(Move m) const;
};