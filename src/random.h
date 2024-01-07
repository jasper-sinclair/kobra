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
  std::mt19937 rng(curr_time());
  std::uniform_int_distribution<std::uint32_t> uni(low, high);
  return uni(rng);
}

namespace rng {
inline int seed = 0;
inline std::mt19937_64 rng(seed);
inline std::normal_distribution<float> normal(0, static_cast<float>(0.2));

inline float GetFloat() { return normal(rng); }

inline float GetFloat(uint64_t seed) {
  std::mt19937_64 gen(seed);
  std::normal_distribution<float> dist(0, static_cast<float>(0.2));
  return dist(gen);
}

inline uint64_t GetUll() { return rng(); }
}  // namespace rng