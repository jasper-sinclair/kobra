#include "uci.h"

#include <ostream>

#include "movegen.h"

void uci::loop() {
  board = Board(kStartFen);

  std::string line, token;
  std::istringstream ss;

  for (;;) {
    std::getline(std::cin, line);
    ss = std::istringstream(line);
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
    else if (token == "isready")
      std::cout << "readyok" << std::endl;
    else if (token == "position")
      position(ss);
    else if (token == "go")
      go(ss.str());

    // non-UCI commands
    else if (token == "d")
      std::cout << board;
    else if (token == "fen")
      std::cout << board.fen() << std::endl;
  }
}

void uci::uci() {
  std::cout << "id name Kobra" << std::endl;
  std::cout << "id author Jasper" << std::endl;
  std::cout << "option name Hash type spin default " << kDefaultHashSize
            << " min 1 max " << kMaxHashSize << std::endl;
  std::cout << "uciok" << std::endl;
}

void uci::ucinewgame() {}

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
          if (hash_size) search.tt.SetSize(hash_size);
        }
      }
    }
  }
}

void uci::position(std::istringstream& ss) {
  std::string token, fen;
  ss >> token;

  if (token == "startpos") {
    fen = kStartFen;
    ss >> token;
  } else if (token == "fen") {
    while (ss >> token && token != "moves") fen += token + " ";
  } else
    return;

  board = {fen};

  uint16_t move;
  while (ss >> token) {
    move = uci::StrToMove(token, board);

    // invalid move
    if (!move) {
      std::cout << "Invalid move: " << token << std::endl;
      break;
    }

    board.ApplyMove(move);
  }
}

void uci::go(const std::string& str) {
  stop();

  std::istringstream ss(str);
  std::string token;

  while (ss >> token) {
    if (token == "perft") {
      Depth depth;
      ss >> depth;

      std::chrono::steady_clock::time_point begin =
          std::chrono::steady_clock::now();

      uint64_t node_cnt = perft(board, depth);

      std::chrono::steady_clock::time_point end =
          std::chrono::steady_clock::now();

      std::cout << "Nodes " << node_cnt << std::endl;
      std::cout << "Time " << (end - begin).count() * 1e-9 << std::endl;

      return;
    }
  }

  // start the search
  search_thread = std::thread(GetBestMove);
}

void uci::stop() {
  if (search_thread.joinable()) search_thread.join();
}

uint16_t uci::StrToMove(const std::string& str, Board& board) {
  MoveList moves;
  GenerateMoves(board, moves);
  for (auto& m : moves) {
    if (move::ToString(m.move) == str) return m.move;
  }
  return uint16_t();
}

void uci::GetBestMove() {
  uint16_t best = search.RandMove(board);
  std::cout << "bestmove " << move::ToString(best) << std::endl;
}