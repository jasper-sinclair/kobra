#include "chrono.h"

#include <algorithm>
#include <cstring>

#include "main.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#else
#endif

Chrono::Chrono() { std::memset(this, 0, sizeof(Chrono)); }

Chrono::TimePoint Chrono::now() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch())
    .count();
}

void Chrono::start() { begin = now(); }
Chrono::TimePoint Chrono::elapsed() const { return now() - begin; }

void Chrono::InitTimeToUse(const Color side_to_move) {
  match_time_limit = time[side_to_move];
  if (match_time_limit) {
    use_match_time_limit = true;
    constexpr double timefactor = 0.02;
    constexpr TimePoint overhead = 50;
    time_to_use = (match_time_limit < inc[side_to_move]
      ? match_time_limit
      : timefactor
      * (match_time_limit - inc[side_to_move])
      + inc[side_to_move])
      - overhead;
    time_to_use = std::max(time_to_use, static_cast<TimePoint>(10));
  }
  else
    time_to_use = std::numeric_limits<long long>::max();
}

void Chrono::update(const uint64_t node_cnt) {
  if (use_match_time_limit && elapsed() > time_to_use ||
    use_node_limit && node_cnt > node_limit ||
    use_move_time_limit && elapsed() > move_time_limit) {
    stop = true;
  }
}
