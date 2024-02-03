#pragma once

#include<iostream>

#include"bitboard.h"
#include"board.h"

struct ExtMove {
	Move move;
	Score score;

	bool operator==(Move move) { return this->move == move; }
};

constexpr int MAX_MOVE = 256;

struct MoveList {;
	ExtMove data[MAX_MOVE], *last;

	MoveList() :last(data) {}

	ExtMove* begin()        { return data; }
	ExtMove* end()          { return last; }
	size_t size()           { return last - data; }

	Move move(size_t idx)   { return data[idx].move; }
	Score score(size_t idx) { return data[idx].score; }
	void add(Move move)     { last++->move = move; }
	void remove(size_t idx);

	friend std::ostream& operator<<(std::ostream& os, MoveList& moveList);
};

inline void MoveList::remove(size_t idx) {
	std::memmove(begin() + idx, begin() + idx + 1, (size() - idx - 1) * sizeof(ExtMove));
	--last;
}

template<Color c>
void generatePawnMoves(Board& board, MoveList& moveList);
template<Color c, PieceType pt>
void generatePieceMoves(Board& board, MoveList& moveList);
template<Color c>
void generateKingMoves(Board& board, MoveList& moveList);
void generateMoves(Board& board, MoveList& moveList);

template<bool Root=true>
inline uint64_t perft(Board& board, int depth) {
	uint64_t cnt, nodes = 0;
	const bool leaf = (depth == 2);

	MoveList moves;
	generateMoves(board, moves);
	for (int i = 0; i < moves.size(); ++i) {
		if (Root && depth <= 1)
			cnt = 1, ++nodes;
		else {
			board.applyMove(moves.move(i));
			MoveList m;
			generateMoves(board, m);
			cnt = leaf ? m.size() : perft<false>(board, depth - 1);
			nodes += cnt;
			board.undoMove();
		}
		if (Root)
			std::cout << move::toString(moves.move(i)) << " " << cnt << "\n";
	}
	return nodes;
}

inline std::ostream& operator<<(std::ostream& os, MoveList& moveList) {
	for (int i = 0; i < moveList.size(); ++i)
		os << move::toString(moveList.move(i)) << " ";
	return os;
}

inline void print(std::vector<Move>& v) {
	for (auto& m : v)
		std::cout << move::toString(m) << " ";
	std::cout << "\n";
}