#pragma once
#include "history.h"

struct MoveSort {
  enum Stages { HASH_MOVE, NORMAL_INIT, NORMAL };

  Board& board;
  Histories& histories;

  bool is_in_check;
  int idx;
  int stage;

  Move hash_move;
  Move next();

  MoveList moves;
  MoveSort(Board& board, Stack* ss, Histories& histories, Move hash_move,
           bool is_in_check);
  Stack* ss;
  void ComputeScores();
};
