#include "uci.h"

#include <ostream>

#include "eval.h"
#include "movegen.h"

int uci::loop() {
  const char* filename = evalfile.c_str();
  nnue_init(filename);
  constexpr int ret = 0;
  board = Board(kStartFen);
  search.SetHashSize(kDefaultHash);
  search.SetNumThreads(kDefaultThreads);
  std::string line, token;

  for (;;) {
    std::getline(std::cin, line);
    auto ss = std::istringstream(line);
    token.clear();
    ss >> token;
    if (token == "quit") {
      stop();
      break;
    }
    if (token == "stop")
      stop();
    else if (token == "uci")
      uci();
    else if (token == "ucinewgame")
      ucinewgame();
    else if (token == "setoption")
      setoption(ss);
    else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    }
    else if (token == "position")
      position(ss);
    else if (token == "go")
      go(ss.str());
    else if (token == "d") {
      std::cout << board;
      std::cout << board.fen() << std::endl;
    }
    else if (token == "perft") {
      Depth depth;
      ss >> depth;
      const std::chrono::steady_clock::time_point begin =
        std::chrono::steady_clock::now();
      const uint64_t node_cnt = perft(board, depth);
      const std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
      std::cout << "node " << node_cnt << std::endl;
      std::cout << "time " << static_cast<double>((end - begin).count()) * 1e-9
        << std::endl;
    }
  }
  return ret;
}

int uci::uci() {
  std::cout << "id name kobra 1.1" << '\n';
  std::cout << "id author jasper" << '\n';
  std::cout << "option name Hash type spin default " << kDefaultHash
    << " min 1 max " << kMaxHashSize << '\n';
  std::cout << "option name Threads type spin default " << kDefaultThreads
    << " min 1 max " << kMaxThreads << '\n';
  std::cout << "uciok" << '\n';
  return fflush(stdout);
}

void uci::ucinewgame() { search.clear(); }

void uci::setoption(std::istringstream& ss) {
  std::string token;
  while (ss >> token) {
    if (token == "name") {
      ss >> token;
      if (token == "Hash") {
        ss >> token;
        if (token == "value") {
          uint64_t hash_size;
          ss >> hash_size;
          if (hash_size) search.hash.SetSize(hash_size);
        }
      }
      else if (token == "Threads") {
        ss >> token;
        if (token == "value") {
          ss >> token;
          const ThreadId count = static_cast<ThreadId>(std::stoi(token));
          search.SetNumThreads(count);
        }
      }
    }
  }
}

int uci::position(std::istringstream& ss) {
  constexpr int ret = 0;
  std::string token, fen;
  ss >> token;
  if (token == "startpos") {
    fen = kStartFen;
    ss >> token;
  }
  else if (token == "fen") {
    while (ss >> token && token != "moves") fen += token + " ";
  }
  else
    return 0;
  board = {fen};

  while (ss >> token) {
    const Move move = ToMove(token, board);
    if (!move) {
      std::cout << "Invalid move: " << token << std::endl;
      break;
    }
    board.ApplyMove(move);
  }
  return ret;
}

void uci::go(const std::string& str) {
  stop();
  search.time = {};
  search.time.start();
  std::istringstream ss(str);
  std::string token;

  while (ss >> token) {
    if (token == "wtime")
      ss >> search.time.time[WHITE];
    else if (token == "btime")
      ss >> search.time.time[BLACK];
    else if (token == "winc")
      ss >> search.time.inc[WHITE];
    else if (token == "binc")
      ss >> search.time.inc[BLACK];
    else if (token == "nodes") {
      search.time.use_node_limit = true;
      ss >> search.time.node_limit;
    }
    else if (token == "depth") {
      search.time.use_depth_limit = true;
      ss >> search.time.depth_limit;
    }
    else if (token == "movetime") {
      search.time.use_move_time_limit = true;
      ss >> search.time.move_time_limit;
    }
  }

  search.time.InitTimeToUse(board.side_to_move);
  thread = std::thread(GetBestMove);
}

void uci::stop() {
  search.stop();
  if (thread.joinable()) thread.join();
}

Move uci::ToMove(const std::string& str, Board& pos) {
  MoveList moves;
  GenMoves(pos, moves);
  for (const auto& [move, score] : moves) {
    if (move::ToString(move) == str) return move;
  }
  return Move();
}

int uci::GetBestMove() {
  const Move best = search.BestMove(board);
  std::cout << "bestmove " << move::ToString(best) << '\n';
  return fflush(stdout);
}
