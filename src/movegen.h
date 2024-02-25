#pragma once
#include <cstring>
#include <iostream>
#include <string>

#include "bitboard.h"

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#else
#endif

struct MoveData {
  Move move;
  int score;
  bool operator==(const Move mv) const { return this->move == mv; }
};

constexpr int kMaxMoves = 256;

struct MoveList {
  [[nodiscard]] MoveData* end() const { return last; }
  [[nodiscard]] int score(const size_t idx) const { return data[idx].score; }
  [[nodiscard]] Move move(const size_t idx) const { return data[idx].move; }
  [[nodiscard]] size_t size() const { return last - data; }

  MoveData data[kMaxMoves], * last;
  MoveData* begin() { return data; }

  friend std::ostream& operator<<(std::ostream& os, const MoveList& move_list);

  void add(const Move move) { last++->move = move; }
  void remove(size_t idx);
  MoveList() : data{}, last(data) {}
};

inline void MoveList::remove(const size_t idx) {
  std::memmove(begin() + idx, begin() + idx + 1,
    (size() - idx - 1) * sizeof(MoveData));
  --last;
}

template <Color C>
void GenPawnMoves(Board& board, MoveList& move_list);

template <Color C, PieceType Pt>
void GenPieceMoves(Board& board, MoveList& move_list);

template <Color C>
void GenKingMoves(Board& board, MoveList& move_list);

void GenMoves(Board& board, MoveList& move_list);

template <bool Root = true>
uint64_t perft(Board& board, const int depth) {
  uint64_t cnt, nodes = 0;
  const bool leaf = depth == 2;
  MoveList moves;
  GenMoves(board, moves);
  for (size_t i = 0; i < moves.size(); ++i) {
    if (Root && depth <= 1)
      cnt = 1, ++nodes;
    else {
      board.ApplyMove(moves.move(i));
      MoveList m;
      GenMoves(board, m);
      cnt = leaf ? m.size() : perft<false>(board, depth - 1);
      nodes += cnt;
      board.UndoMove();
    }
    if (Root) {
      std::cout << move::ToString(moves.move(i)) << " " << cnt << fflush(stdout)
        << '\n';
    }
  }
  return nodes;
}

inline std::ostream& operator<<(std::ostream& os, const MoveList& move_list) {
  for (int i = 0; i < static_cast<int>(move_list.size()); ++i)
    os << move::ToString(move_list.move(i)) << " ";
  return os;
}

inline int print(const std::vector<Move>& v) {
  for (const auto& m : v) std::cout << move::ToString(m) << " ";
  std::cout << "\n";
  return fflush(stdout);
}
