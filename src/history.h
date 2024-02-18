#pragma once

#include<array>
#include<cstdint>

#include"movegen.h"

using HistoryEntry = int16_t;

template<size_t Size, size_t... Sizes>
struct History : public std::array<History<Sizes...>, Size> {

	void fill(HistoryEntry value) {
		HistoryEntry* e = reinterpret_cast<HistoryEntry*>(this);
		std::fill(e, e + sizeof(*this) / sizeof(HistoryEntry), value);
	}

	static constexpr HistoryEntry MAX = 30000;

	static HistoryEntry p(Depth d) {
		return std::min(153 * d - 133, 1525);
	}
};

template<size_t Size>
struct History<Size> : public std::array<HistoryEntry, Size> {};

struct CaptureHistory : public History<N_PIECES, N_SQUARES, N_PIECE_TYPES> {

	static constexpr HistoryEntry I_A = 4;
	static constexpr HistoryEntry D_A = 5;

	void increase(const Board& board, Move move, Depth d);
	void decrease(const Board& board, Move move, Depth d);
};

struct ButterflyHistory : public History<N_COLORS, N_SQUARES, N_SQUARES> {

	static constexpr HistoryEntry I_A = 6;
	static constexpr HistoryEntry D_A = 3;

	void increase(const Board& board, Move move, Depth d);
	void decrease(const Board& board, Move move, Depth d);
};

struct Stack;

struct ContinuationHistory : public History<N_PIECES, N_SQUARES, N_PIECES, N_SQUARES> {

	static constexpr HistoryEntry I_A1 = 2;
	static constexpr HistoryEntry I_A2 = 2;
	static constexpr HistoryEntry I_A4 = 2;
	static constexpr HistoryEntry D_A1 = 2;
	static constexpr HistoryEntry D_A2 = 2;
	static constexpr HistoryEntry D_A4 = 2;

	void increase(const Board& board, const Stack* ss, Move move, Depth d);
	void decrease(const Board& board, const Stack* ss, Move move, Depth d);
};

struct Histories {
	Move killer[MAX_DEPTH + 1][2];

	// [moving piece][to]
	Move counter[N_PIECES][N_SQUARES];

	// [color][from][to]
	ButterflyHistory butterfly;

	// [moving piece][to][captured piece type]
	CaptureHistory capture;

	// [past moving piece][past to][moving piece][to];
	ContinuationHistory continuation;

	static constexpr HistoryEntry BUTTERFLY_FILL = -425;
	static constexpr HistoryEntry CAPTURE_FILL = -415;
	static constexpr HistoryEntry CONTINUATION_FILL = -470;

	Histories() { clear(); }

	void update(
		Board& board,
		Stack* ss,
		Move bestMove,
		MoveList& moves,
		Depth depth,
		bool isInCheck
	);

	void clear();
};
