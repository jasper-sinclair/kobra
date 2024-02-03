#include<iostream>

#include"chrono.h"

TimeManagement::TimeManagement() {
	std::memset(this, 0, sizeof(TimeManagement));
}

TimeManagement::TimePoint TimeManagement::now() {
	return std::chrono::duration_cast<std::chrono::milliseconds>
		(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TimeManagement::start() {
	begin = now();
}

TimeManagement::TimePoint TimeManagement::elapsed() const
{
	return now() - begin;
}

void TimeManagement::initializeTimeToUse(Color sideToMove) {
	matchTimeLimit = time[sideToMove];
	if (matchTimeLimit) {
		useMatchTimeLimit = true;
		constexpr TimePoint overhead = 50;
		timeToUse = (matchTimeLimit < inc[sideToMove] ? matchTimeLimit : 0.05 * (matchTimeLimit - inc[sideToMove]) + inc[sideToMove]) - overhead;
		timeToUse = std::max(timeToUse, static_cast<TimePoint>(10));
	}
	else
		timeToUse = std::numeric_limits<long long>::max();
}

void TimeManagement::update(uint64_t nodeCnt) {
	if (useMatchTimeLimit && elapsed() > timeToUse ||
		useNodeLimit && nodeCnt > nodeLimit ||
		useMoveTimeLimit && elapsed() > moveTimeLimit) 
	{
		stop = true;
	}
}