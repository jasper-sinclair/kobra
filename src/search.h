#pragma once

#include<thread>

#include"movesort.h"
#include"chrono.h"
#include"hash.h"

using ThreadID = uint32_t;

constexpr int MIN_DISPLAY_TIME = 5000;
constexpr int MAX_THREADS = 256;

enum SearchType {
	NonPV, PV, Root
};

struct Stack {
	Move pv[MAX_DEPTH];
	int pvSize;
	int ply;
	Piece moved;
	Move move;
	Move hashMove;
};

struct ThreadData {
	ThreadID id;
	Histories histories;
	Stack stack[MAX_PLY + CONTINUATION_PLY];
	std::vector<Move>pv;
	uint64_t nodeCount;
	Depth rootDepth;
	Depth selDepth;
	Depth nmpMinPly;
	Color nmpColor;

	ThreadData() : id(0), stack{}, nodeCount(0), rootDepth(0), selDepth(0), nmpMinPly(0), nmpColor(false)
  {
  }

	ThreadData(ThreadID id) : id(id), stack{}, nodeCount(0), rootDepth(0), selDepth(0), nmpMinPly(0),
                            nmpColor(false)
  {
  }
};

struct Search {
	TranspositionTable tt;
	TimeManagement time;

	std::vector<std::thread>threads;
	std::vector<ThreadData*>threadData;
	ThreadID numThreads;

	static constexpr int16_t LMR_FACTOR = 1000;
	inline static Depth lateMoveReductions[MAX_DEPTH][MAX_MOVE];
	
	// Initialize LMR table
	static void init();

	template<bool mainThread=true> 
	Move bestMove(Board& board, ThreadID id=0);

	template<SearchType searchType, bool skipHashMove=false>
	Score alphaBeta(Board& board, Score alpha, Score beta, Depth depth, ThreadData& td, Stack* ss);

	template<SearchType searchType>
	Score quiescence(Board& board, Score alpha, Score beta, ThreadData& td, Stack* ss);

	void stop();
	void clear();
	void setTTSize(size_t MiB);
	void setNumThreads(ThreadID numThreads);

	// UCI command
	std::string info(ThreadData& td, Depth depth, Score score);

	uint64_t nodeCount();
};