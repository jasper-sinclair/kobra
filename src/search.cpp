#include "search.h"
#include "movegen.h"

uint16_t Search::RandMove(Board& board, ThreadId id) {
  uint32_t num_moves = 0;
  Move randmove{};

  MoveList moves;
  GenerateMoves(board, moves);

  for (const auto& m : moves) {
    num_moves++;
  }

  const uint32_t r = rand_u32(1, num_moves);
  num_moves = 0;
  for (const auto& m : moves) {
    num_moves++;
    if (num_moves == r) randmove.move = m.move;
  }
  return randmove.move;
}
