#pragma once

#include <array>

//#include "movegen.h"

namespace eval {
constexpr std::array<Score, 7> kPValues = {0, 100, 325, 350, 550, 1000, 10000};
constexpr std::array<Score, 16> kPieceValues = {
    0,
    kPValues[1],
    kPValues[2],
    kPValues[3],
    kPValues[4],
    kPValues[5],
    kPValues[6],
    0,
    0,
    kPValues[1],
    kPValues[2],
    kPValues[3],
    kPValues[4],
    kPValues[5],
    kPValues[6],
    0,
};
}  // namespace eval