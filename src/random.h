#pragma once

#include <chrono>
#include <random>

using std::chrono::milliseconds;

typedef long TimeMs;

inline TimeMs curr_time() {
  return static_cast<long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}
inline std::uint32_t rand_u32(std::uint32_t low, std::uint32_t high) {
  std::mt19937 gen(curr_time());
  std::uniform_int_distribution<std::uint32_t> dis(low, high);
  return dis(gen);
}

inline uint64_t rand64() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;
  return dis(gen);
}
