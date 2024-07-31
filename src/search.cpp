// search.cpp
#include "search.h"

#ifdef _MSC_VER
#pragma warning(disable : 4127 4244 4702)
#endif

template <bool MainThread>
Move Search::BestMove(Board& board, const ThreadId id) {
  if (MainThread) {
    for (const auto& td : thread_data) {
      td->pv.clear();
      td->node_count = 0;
      td->root_depth = 1;
      td->sel_depth = 1;
    }
    for (ThreadId i = 1; i < num_threads; ++i) {
      threads.emplace_back(&Search::BestMove<false>, this, std::ref(board), i);
    }
  }
  ThreadData& td = *thread_data[id];
  Board copy(board);
  Score alpha, beta, score = 0, prev_score = 0;
  constexpr Depth max_depth = kMaxDepth;

  for (int i = 0; i < kMaxPly + kContinuationPly; ++i) {
    td.stack[i].ply = i - kContinuationPly;
    td.stack[i].moved = NO_PIECE;
    td.stack[i].pv_size = 0;
    td.stack[i].move = Move();
  }

  Stack* ss = &td.stack[kContinuationPly];
  *ss = Stack();

  while (td.root_depth < max_depth) {
    if (MainThread && time.use_depth_limit && td.root_depth > time.depth_limit)
      break;

    Score delta = 17;
    if (td.root_depth == 1) {
      alpha = -INFINITY_SCORE;
      beta = INFINITY_SCORE;
    }
    else {
      alpha = static_cast<Score>(std::max(score - delta, -INFINITY_SCORE));
      beta = static_cast<Score>(std::min(score + delta, +INFINITY_SCORE));
    }

    while (true) {
      score = AlphaBeta<kRoot>(copy, alpha, beta, td.root_depth, td, ss);
      if (time.stop) break;

      if (score <= alpha) {
        beta = static_cast<Score>((alpha + beta) / 2);
        alpha = static_cast<Score>(std::max(alpha - delta, -INFINITY_SCORE));
      }
      else if (score >= beta) {
        beta = static_cast<Score>(std::min(beta + delta, +INFINITY_SCORE));
      }
      else {
        break;
      }
      delta += delta / 3;
    }

    if (time.stop) break;

    prev_score = score;
    td.pv.clear();
    for (int i = 0; i < ss->pv_size; ++i) {
      td.pv.push_back(ss->pv[i]);
    }

    if (MainThread) {
      std::cout << info(td, td.root_depth, score) << std::endl;
    }

    ++td.root_depth;
  }

  if (MainThread) {
    stop();
    for (auto& t : threads) {
      t.join();
    }
    threads.clear();

    if (time.use_node_limit || time.use_move_time_limit) {
      std::cout << info(td, td.root_depth, prev_score) << std::endl;
    }
    return td.pv[0];
  }
  return Move();
}

template Move Search::BestMove<true>(Board& board, ThreadId id);
template Move Search::BestMove<false>(Board& board, ThreadId id);

template <SearchType St, bool SkipHashMove>
Score Search::AlphaBeta(Board& board, Score alpha, Score beta, Depth depth, ThreadData& td, Stack* ss) {
  constexpr bool root_node = St == kRoot;
  constexpr bool pv_node = St != kNonPv;

  if (!pv_node) assert(beta - alpha == 1);
  assert(alpha < beta);
  assert(depth < kMaxDepth);

  const bool main_thread = td.id == 0;

  if (main_thread && depth >= 5) time.update(NodeCount());
  if (time.stop) return STOP_SCORE;
  if (board.IsDraw()) return DRAW_SCORE;
  if (depth <= 0) return quiescence<pv_node ? PV : kNonPv>(board, alpha, beta, td, ss);

  ++td.node_count;

  if (pv_node) {
    td.sel_depth = std::max(td.sel_depth, static_cast<Depth>(ss->ply + 1));
  }

  if (!root_node) {
    alpha = static_cast<Score>(std::max(-MATE_SCORE + ss->ply, static_cast<int>(alpha)));
    beta = static_cast<Score>(std::min(MATE_SCORE - ss->ply - 1, static_cast<int>(beta)));
    if (alpha >= beta) return alpha;
  }

  const Key key = board.key();
  HashEntry he;
  const bool hash_hit = hash.probe(key, he);
  const Score hash_score = HashTable::ScoreFromHash(he.score, static_cast<Depth>(ss->ply));

  if (!pv_node && !SkipHashMove && hash_hit && depth <= he.depth) {
    if (he.node_type == PV_NODE || (he.node_type == CUT_NODE && hash_score >= beta) ||
      (he.node_type == ALL_NODE && hash_score <= alpha)) {
      if (board.st->fifty_move_count < 90) return hash_score;
    }
  }

  const bool is_in_check = board.IsInCheck();
  Score eval;

  if (is_in_check) {
    eval = ss->static_eval = -INFINITY_SCORE;
  }
  else if (hash_hit) {
    ss->static_eval = he.static_eval;
    if (depth <= he.depth &&
      (he.node_type == PV_NODE ||
      (he.node_type == CUT_NODE && hash_score > ss->static_eval) ||
      (he.node_type == ALL_NODE && hash_score < ss->static_eval))) {
      eval = hash_score;
    }
    else {
      eval = ss->static_eval;
    }
  }
  else {
    eval = ss->static_eval = evaluate(board);
  }

  td.histories.killer[(ss + 1)->ply][0] = td.histories.killer[(ss + 1)->ply][1] = Move();

  if (!root_node && !is_in_check) {
    if (!pv_node && depth < 3 && eval >= beta + 171 * depth && eval < MIN_MATE_SCORE) return eval;

    if (!pv_node && !SkipHashMove && board.NonPawnMaterial(board.side_to_move) &&
      beta > -MIN_MATE_SCORE && eval >= ss->static_eval && eval >= beta &&
      ss->static_eval >= beta - 22 * depth + 400 && depth < 12) {
      assert((ss - 1)->move);
      const Depth r = static_cast<Depth>((13 + depth) / 5);
      ss->move = Move();
      board.ApplyNullMove();

      Score null_score = static_cast<Score>(-AlphaBeta<kNonPv>(
        board, static_cast<Score>(-beta), static_cast<Score>(-alpha), static_cast<Depth>(depth - r), td, ss + 1));

      board.UndoNullMove();
      if (null_score >= beta) {
        if (null_score >= MIN_MATE_SCORE) null_score = beta;
        return null_score;
      }
    }

    if (pv_node && !hash_hit) --depth;
    if (depth <= 0) return quiescence<pv_node ? PV : kNonPv>(board, alpha, beta, td, ss);
  }

  const Move hash_move = hash_hit ? he.move : Move();
  MoveSort move_sorter(board, ss, td.histories, hash_move, is_in_check);
  Score best_score = -INFINITY_SCORE;
  Score score = 0;
  Move best_move = Move();
  int move_count = 0;

  while (true) {
    const Move m = move_sorter.next();
    if (!m) break;
    assert(board.IsPseudoLegal(m));
    assert(board.IsLegal(m));
    if (pv_node) (ss + 1)->pv_size = 0;
    ++move_count;

    if (main_thread && root_node && time.elapsed() >= kMinDisplayTime) {
      std::cout << "info depth " << depth << " currmove " << move::ToString(m) << " currmovenumber " << move_count << std::endl;
    }

    if (SkipHashMove && m == ss->hash_move) continue;

    const bool is_capture = board.IsCapture(m);
    const Square from = move::from(m);
    const Square to = move::to(m);
    ss->moved = board.PieceOn(from);
    Depth new_depth = static_cast<Depth>(depth - 1);

    if (!root_node && std::abs(best_score) < MIN_MATE_SCORE && board.NonPawnMaterial(board.side_to_move)) {
      if (depth < 3 && move_count > move_count_pruning_table[depth]) continue;
      if (!is_capture && !is_in_check && !board.GivesCheck(m)) {
        const Score h_score = static_cast<Score>(
          td.histories.butterfly[board.side_to_move][from][to] / 106 +
          td.histories.continuation[(ss - 1)->moved][move::to((ss - 1)->move)][ss->moved][to] / 30 +
          td.histories.continuation[(ss - 2)->moved][move::to((ss - 2)->move)][ss->moved][to] / 34 +
          td.histories.continuation[(ss - 4)->moved][move::to((ss - 4)->move)][ss->moved][to] / 42);
        Depth pruning_depth = static_cast<Depth>(
          new_depth + (5 * h_score - forward_pruning_table[depth][move_count] - 506) / 1000);
        pruning_depth = static_cast<Depth>(std::max(+pruning_depth, -1));
        if (pruning_depth < 2 && ss->static_eval <= alpha - 200 - 172 * pruning_depth)
          continue;
      }
    }

    ss->move = m;
    Depth ext = 0;
    bool lmr = false;

    if (move_count == 1) {
      if (!root_node && !SkipHashMove && depth >= 8 && m == hash_move &&
        (he.node_type == CUT_NODE || he.node_type == PV_NODE) && he.depth + 3 >= depth &&
        std::abs(eval) < MIN_MATE_SCORE) {
        const Score singular_beta = std::min(static_cast<Score>(eval - 2 * depth), beta);
        const Depth singular_depth = static_cast<Depth>(depth / 2);
        ss->hash_move = m;
        score = AlphaBeta<kNonPv, true>(board, static_cast<Score>(singular_beta - 1), singular_beta, singular_depth, td, ss);
        if (score < singular_beta)
          ext = 1;
        else if (hash_score >= beta)
          ext = -1;
        new_depth += ext;
      }
    }
    else {
      if (depth >= 2 && !(pv_node && is_capture)) {
        lmr = true;
        ext = static_cast<Score>(-log_reduction_table[depth][move_count]);
        if (is_capture) {
          const Square cap_sq = move::move_type(m) == move::EN_PASSANT ? to - static_cast<Square>(static_cast<Score>(direction::PawnPush(board.side_to_move))) : to;
          const PieceType cap_pt = piece_type::make(board.PieceOn(cap_sq));
          const Score h_score = static_cast<Score>(td.histories.capture[ss->moved][cap_sq][cap_pt] / 21 + 43);
          ext += h_score;
        }
        else {
          const Score h_score = static_cast<Score>(
            td.histories.butterfly[board.side_to_move][from][to] / 106 +
            td.histories.continuation[(ss - 1)->moved][move::to((ss - 1)->move)][ss->moved][to] / 30 +
            td.histories.continuation[(ss - 2)->moved][move::to((ss - 2)->move)][ss->moved][to] / 34 +
            td.histories.continuation[(ss - 4)->moved][move::to((ss - 4)->move)][ss->moved][to] / 42);
          ext += h_score;
        }
        if (pv_node) ext += 1057 + 11026 / (3 + depth);
        if (hash_move && board.IsCapture(hash_move)) ext += -839;
        if (is_in_check) ext += 722;
        if (board.GivesCheck(m)) ext += 796;
        if (m == td.histories.killer[ss->ply][0])
          ext += 1523;
        else if (m == td.histories.killer[ss->ply][1])
          ext += 1488;
      }
    }

    board.ApplyMove(m);

    if (lmr) {
      ext = static_cast<Depth>(std::round(ext / static_cast<double>(lmr_factor)));
      const Depth d = static_cast<Depth>(std::clamp(new_depth + ext, 0, +new_depth + 1));
      score = static_cast<Score>(-AlphaBeta<kNonPv>(board, static_cast<Score>(-alpha - 1), static_cast<Score>(-alpha), d, td, ss + 1));
      if (ext < 0 && score > alpha)
        score = static_cast<Score>(-AlphaBeta<kNonPv>(board, static_cast<Score>(-alpha - 1), static_cast<Score>(-alpha), new_depth, td, ss + 1));
    }
    else if (!pv_node || move_count > 1)
      score = static_cast<Score>(-AlphaBeta<kNonPv>(board, static_cast<Score>(-alpha - 1), static_cast<Score>(-alpha), new_depth, td, ss + 1));

    if (pv_node && (move_count == 1 || (score > alpha && (root_node || score < beta))))
      score = static_cast<Score>(-AlphaBeta<PV>(board, static_cast<Score>(-beta), static_cast<Score>(-alpha), new_depth, td, ss + 1));

    board.UndoMove();
    if (time.stop) return STOP_SCORE;

    if (score > best_score) {
      best_score = score;
      if (pv_node) {
        ss->pv[0] = m;
        std::memcpy(ss->pv + 1, (ss + 1)->pv, (ss + 1)->pv_size * sizeof(Move));
        ss->pv_size = (ss + 1)->pv_size + 1;
      }
      if (score > alpha) {
        best_move = m;
        if (score < beta)
          alpha = score;
        else
          break;
      }
    }
  }

  if (SkipHashMove && move_count == 1) return alpha;
  if (!move_count) return is_in_check ? -MATE_SCORE + ss->ply : DRAW_SCORE;

  if (!SkipHashMove) {
    const NodeType node_type = best_score >= beta ? CUT_NODE : pv_node && best_move ? PV_NODE : ALL_NODE;
    assert(node_type == CUT_NODE || node_type == PV_NODE == static_cast<bool>(best_move));
    if (best_move) {
      td.histories.update(board, ss, best_move, move_sorter.moves, depth);
    }
    hash.save(key, HashTable::ScoreToHash(best_score, static_cast<Depth>(ss->ply)), ss->static_eval, best_move, depth, node_type);
  }
  return best_score;
}

template <SearchType St>
Score Search::quiescence(Board& board, Score alpha, const Score beta, ThreadData& td, Stack* ss) {
  constexpr bool pv_node = St == PV;
  ++td.node_count;
  assert(pv_node || beta - alpha == 1);
  assert(alpha < beta);
  if (time.stop) return STOP_SCORE;
  if (board.IsDraw()) return DRAW_SCORE;
  if (ss->ply >= kMaxPly) return static_cast<Score>(board.IsInCheck() ? DRAW_SCORE : evaluate(board));
  const Key key = board.key();
  HashEntry he;
  const bool hash_hit = hash.probe(key, he);
  const Score hash_score = HashTable::ScoreFromHash(he.score, static_cast<Depth>(ss->ply));

  if (!pv_node && hash_hit && (he.node_type == PV_NODE || (he.node_type == CUT_NODE && hash_score >= beta) || (he.node_type == ALL_NODE && hash_score <= alpha))) return hash_score;

  const bool is_in_check = board.IsInCheck();
  Score best_score;
  if (is_in_check)
    best_score = ss->static_eval = -INFINITY_SCORE;
  else
    best_score = ss->static_eval = hash_hit ? he.static_eval : evaluate(board);

  if (hash_hit && (he.node_type == PV_NODE || (he.node_type == CUT_NODE && hash_score > best_score) || (he.node_type == ALL_NODE && hash_score < best_score)))
    best_score = hash_score;

  if (!is_in_check) {
    if (best_score >= beta) {
      if (!hash_hit)
        hash.save(key, HashTable::ScoreToHash(best_score, static_cast<Depth>(ss->ply)), ss->static_eval, Move(), 0, CUT_NODE);
      return best_score;
    }
    if (pv_node && best_score > alpha) alpha = best_score;
  }

  Move best_move = hash_hit ? he.move : Move();
  MoveSort move_sorter(board, ss, td.histories, best_move, is_in_check);

  while (true) {
    const Move m = move_sorter.next();
    if (!m) break;
    const bool is_capture = board.IsCapture(m);
    if (!is_in_check && !(is_capture || (board.IsPromotion(m) && move::GetPieceType(m) == QUEEN))) continue;
    assert(board.IsPseudoLegal(m));
    assert(board.IsLegal(m));
    if (is_capture && !is_in_check) {
      if (const Score see = board.see(m); see < eval::kPtValues[KNIGHT] - eval::kPtValues[BISHOP]) continue;
    }
    ss->moved = board.PieceOn(move::from(m));
    ss->move = m;
    board.ApplyMove(m);
    const Score score = -quiescence<St>(board, -beta, -alpha, td, ss + 1);
    board.UndoMove();
    if (score > best_score) {
      best_score = score;
      if (score > alpha) {
        best_move = m;
        if (pv_node && score < beta)
          alpha = score;
        else
          break;
      }
    }
  }

  if (move_sorter.moves.size() == 0)
    return static_cast<Score>(is_in_check ? -MATE_SCORE + ss->ply : DRAW_SCORE);

  const NodeType node_type = best_score >= beta ? CUT_NODE : pv_node && best_move ? PV_NODE : ALL_NODE;
  hash.save(key, HashTable::ScoreToHash(best_score, static_cast<Depth>(ss->ply)), ss->static_eval, best_move, 0, node_type);
  return best_score;
}

std::string Search::info(const ThreadData& td, const Depth depth, const Score score) const {
  std::stringstream ss;
  ss << "info"
    << " depth " << depth << " seldepth " << td.sel_depth;

  if (std::abs(score) < MIN_MATE_SCORE)
    ss << " score cp " << score;
  else
    ss << " score mate " << (MATE_SCORE - std::abs(score) + 1) / 2 * (score < 0 ? -1 : 1);

  const Chrono::TimePoint elapsed = time.elapsed() + 1;
  const uint64_t nodes = NodeCount();

  ss << " nodes " << nodes << " nps " << nodes * 1000 / elapsed << " time " << elapsed << " hashfull " << hash.usage() * 1000;

  if (!td.pv.empty()) {
    ss << " pv";
    for (const unsigned short i : td.pv) {
      ss << " " << move::ToString(i);
    }
  }
  return ss.str();
}

void Search::init() {
  using std::log;
  for (int d = 1; d < kMaxDepth; ++d) {
    for (int m = 1; m < kMaxMoves; ++m) {
      log_reduction_table[d][m] = static_cast<Depth>(492 * log(d) * log(m) + 83 * log(m) + 42 * log(d) + 1126);
    }
  }
  move_count_pruning_table[1] = 4;
  move_count_pruning_table[2] = 11;
  for (int d = 1; d < kMaxDepth; ++d) {
    for (int m = 1; m < kMaxMoves; ++m) {
      forward_pruning_table[d][m] = static_cast<Depth>(410 * log(d) * log(m) + 69 * log(m) + 35 * log(d));
    }
  }
}

void Search::stop() {
  time.stop = true;
}

void Search::SetHashSize(const size_t mb) {
  hash.SetSize(mb);
}

uint64_t Search::NodeCount() const {
  uint64_t sum = 0;
  for (const auto& td : thread_data) {
    sum += td->node_count;
  }
  return sum;
}

void Search::clear() {
  hash.clear();
  while (!thread_data.empty()) {
    delete thread_data.back();
    thread_data.pop_back();
  }
  for (ThreadId i = 0; i < num_threads; ++i) {
    thread_data.push_back(new ThreadData(i));
  }
}

void Search::SetNumThreads(const ThreadId threadnum) {
  this->num_threads = std::clamp(threadnum, static_cast<ThreadId>(1),
    std::min(std::thread::hardware_concurrency(), static_cast<ThreadId>(kMaxThreads)));
  while (!thread_data.empty()) {
    delete thread_data.back();
    thread_data.pop_back();
  }
  for (ThreadId i = 0; i < this->num_threads; ++i) {
    thread_data.push_back(new ThreadData(i));
  }
}