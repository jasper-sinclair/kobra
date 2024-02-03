#pragma once

#include<chrono>

#include"bitboard.h"

struct TimeManagement {
	using TimePoint = std::chrono::milliseconds::rep;

	bool stop;

	bool useMatchTimeLimit;
	bool useNodeLimit;
	bool useDepthLimit;
	bool useMoveTimeLimit;

	TimePoint matchTimeLimit;
	uint64_t nodeLimit;
	Depth depthLimit;
	TimePoint moveTimeLimit;

	TimePoint begin;
	TimePoint timeToUse;
	TimePoint time[N_COLORS], inc[N_COLORS];

	TimeManagement();

	static TimePoint now();
	void start();
	TimePoint elapsed() const;
	void initializeTimeToUse(Color sideToMove);
	void update(uint64_t nodeCnt);
};