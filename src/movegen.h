#pragma once

#include <iostream>

#include "bitboard.h"
#ifdef _MSC_VER
#pragma warning(disable : 4127)
#else
#endif

struct Move {
  uint16_t move;
  Score score;

  bool operator==(uint16_t move) { return this->move == move; }
};

constexpr int kMaxMove = 256;

struct MoveList {
  Move data[kMaxMove], *last;

  MoveList() : last(data) {}

  Move* begin() { return data; }
  Move* end() { return last; }
  size_t size() { return last - data; }

  uint16_t move(size_t idx) { return data[idx].move; }
  Score score(size_t idx) { return data[idx].score; }
  void add(uint16_t move) { last++->move = move; }
  void remove(size_t idx);

  friend std::ostream& operator<<(std::ostream& os, MoveList& move_list);
};

inline void MoveList::remove(size_t idx) {
  std::memmove(begin() + idx, begin() + idx + 1,
               (size() - idx - 1) * sizeof(Move));
  --last;
}

template <Color C>
void GeneratePawnMoves(Board& board, MoveList& move_list);
template <Color C, int32_t Pt>
void GeneratePieceMoves(Board& board, MoveList& move_list);
template <Color C>
void GenerateKingMoves(Board& board, MoveList& move_list);
void GenerateMoves(Board& board, MoveList& move_list);

template <bool Root = true>
inline uint64_t perft(Board& board, int depth) {
  uint64_t cnt, nodes = 0;
  const bool leaf = (depth == 2);

  MoveList moves;
  GenerateMoves(board, moves);
  for (int i = 0; i < moves.size(); ++i) {
    if (Root && depth <= 1)
      cnt = 1, ++nodes;
    else {
      board.ApplyMove(moves.move(i));
      MoveList m;
      GenerateMoves(board, m);
      cnt = leaf ? m.size() : perft<false>(board, depth - 1);
      nodes += cnt;
      board.UndoMove();
    }
    if (Root) std::cout << move::ToString(moves.move(i)) << " " << cnt << "\n";
  }
  return nodes;
}

inline std::ostream& operator<<(std::ostream& os, MoveList& move_list) {
  for (int i = 0; i < move_list.size(); ++i)
    os << move::ToString(move_list.move(i)) << " ";
  return os;
}

inline void print(std::vector<uint16_t>& v) {
  for (auto& m : v) std::cout << move::ToString(m) << " ";
  std::cout << "\n";
}