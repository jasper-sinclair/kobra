#include "uci.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include "eval.h"
#include "movegen.h"
#include "nnue.h"
#include "selfplay.h"

namespace uci{
  board pos;
  bool use_nnue = true;
  bool verbose = true;
  int contempt = default_contempt;
  search search_info;
  std::jthread thread;
  std::mutex search_mutex;
  std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  std::unordered_map<std::string, uci_option> options = {
    {
      "Hash",uci_option::spin("Hash",
        default_hash,
        1,
        max_hash_size)},
    {
      "Threads",uci_option::spin("Threads",
        default_threads,
        1,
        max_threads)},
    {
      "Contempt",uci_option::spin("Contempt",
        default_contempt,
        -100,
        100)},
    {"UseNNUE",uci_option::check("UseNNUE",true)}
  };
  std::vector<std::string> option_order = {
    "Hash",
    "Threads",
    "Contempt",
    "UseNNUE"
  };

  void init(){
    (void)nnue::instance();

    pos = board(start_fen);

    search_info.set_hash_size(options["Hash"].spin_value);
    search_info.set_num_threads(options["Threads"].spin_value);
    contempt = options["Contempt"].spin_value;
    use_nnue = options["UseNNUE"].check_value;

    loop();
  }

  void info(){
    SO << "id name " << engine_name << " " << engine_version << NL;
    SO << "id author " << engine_author << NL;

    for (const auto& name : option_order){
      const auto& opt = options.at(name);

      SO << "option name " << name << " type ";

      if (opt.type == option_type::spin){
        SO << "spin default " << opt.spin_value
          << " min " << opt.min
          << " max " << opt.max;
      } else if (opt.type == option_type::check){
        SO << "check default "
          << (opt.check_value?"true":"false");
      }
      SO << NL;
    }
    SO << "uciok" << SE;
  }

  void newgame(){
    search_info.clear();
  }

  void apply_option(
    const std::string& name){
    if (name == "Hash"){
      search_info.hash.set_size(options["Hash"].spin_value);
    } else if (name == "Threads"){
      search_info.set_num_threads(options["Threads"].spin_value);
    } else if (name == "Contempt"){
      contempt = options["Contempt"].spin_value;
    } else if (name == "UseNNUE"){
      const bool old = use_nnue;
      use_nnue = options["UseNNUE"].check_value;
      if (old != use_nnue){
        search_info.clear();
      }
      SO << "info string UseNNUE set to "
        << (use_nnue?"true":"false")
        << NL;
    }
  }

  void setoption(
    std::istringstream& ss){
    std::string token, name, value;

    while (ss >> token){
      if (token == "name")
        ss >> name;
      else if (token == "value")
        std::getline(ss >> std::ws,value);
    }

    const auto it = options.find(name);
    if (it == options.end())
      return;

    if (uci_option& opt = it->second; opt.type == option_type::spin){
      int v = std::stoi(value);
      v = std::clamp(v,opt.min,opt.max);
      opt.spin_value = v;
    } else if (opt.type == option_type::check){
      opt.check_value = value == "true";
    }

    apply_option(name);
  }

  void loop(){
    bool running = true;
    std::string line, token;

    std::unordered_map<std::string,
      std::function<void(
        std::istringstream&)>> commands = {

      {
        "uci",[](
        std::istringstream&){
          info();
        }},
      {
        "ucinewgame",[](
        std::istringstream&){
          newgame();
        }},
      {
        "setoption",[](
        std::istringstream& ss){
          setoption(ss);
        }},
      {
        "isready",[](
        std::istringstream&){
          SO << "readyok" << NL;
        }},
      {
        "position",[](
        std::istringstream& ss){
          position(ss);
        }},
      {
        "go",[](
        std::istringstream& ss){
          go(ss);
        }},
      {
        "stop",[](
        std::istringstream&){
          stop();
        }},
      {
        "quit",[&running](
        std::istringstream&){
          stop();
          running = false;
        }},
      {
        "print",[](
        std::istringstream&){
          SO << pos << NL << pos.fen() << NL;
        }},
      {
        "eval",[](
        std::istringstream&){
          SO << "Static eval: "
            << eval::evaluate(pos)
            << NL;
        }},
      {
        "bench",[](
        std::istringstream& ss){
          bench(ss);
        }},
      {"perft",perft},
      {
        "selfplay",[](
        std::istringstream& ss){
          int games = 0;
          int movetime = 0;
          int depth = 0;
          int nodes = 0;
          ss >> games;
          std::string arg;
          while (ss >> arg){
            if (arg == "movetime")
              ss >> movetime;
            else if (arg == "depth")
              ss >> depth;
            else if (arg == "nodes")
              ss >> nodes;
          }

          if (nodes <= 0 && depth <= 0 && movetime <= 0){
            std::cout << "Error: specify nodes, depth, or movetime\n";
            return;
          }
          run_selfplay(games,movetime,depth,nodes);
        }
      },
    };

    while (running && std::getline(std::cin,line)){
      std::istringstream ss(line);
      ss >> token;

      if (auto it = commands.find(token); it != commands.end()){
        it->second(ss);
      }
    }
  }

  void position(
    std::istringstream& ss){
    std::string token, fen;
    ss >> token;
    if (token == "startpos"){
      fen = start_fen;
      ss >> token;
    } else if (token == "fen"){
      while (ss >> token && token != "moves") fen += token + " ";
    } else return;
    pos = board(fen);
    while (ss >> token){
      if (const u16 move = to_move(token,pos); move){
        pos.apply_move(move);
      } else{
        std::cerr << "Invalid move: " << token << NL;
        break;
      }
    }
  }

  void go(
    std::istringstream& ss){
    std::scoped_lock lock(search_mutex);
    stop();
    search_info.time = {};
    search_info.time.start();
    std::string token;
    while (ss >> token){
      if (token == "wtime") ss >> search_info.time.time[white];
      else if (token == "btime") ss >> search_info.time.time[black];
      else if (token == "winc") ss >> search_info.time.inc[white];
      else if (token == "binc") ss >> search_info.time.inc[black];
      else if (token == "nodes"){
        search_info.time.use_node_limit = true;
        ss >> search_info.time.node_limit;
      } else if (token == "depth"){
        search_info.time.use_depth_limit = true;
        ss >> search_info.time.depth_limit;
      } else if (token == "movetime"){
        search_info.time.use_move_limit = true;
        ss >> search_info.time.move_time_limit;
      }
    }
    search_info.time.init_time(pos.side_to_move);
    thread = std::jthread(get_bestmove);
  }

  void stop(){
    search_info.stop();
    if (thread.joinable())
      thread.join();
  }

  u16 to_move(
    const std::string& str,
    board& b){
    move_list moves;
    gen_moves(b,moves);

    for (const auto& [move, score] : moves){
      if (move::move_to_string(move) == str)
        return move;
    }

    return 0;
  }

  void get_bestmove(){
    const u16 move = search_info.best_move(pos);
    SO << "bestmove " << move::move_to_string(move) << SE;
  }

  void perft(
    std::istringstream& ss){
    i32 depth;
    ss >> depth;

    const auto begin = std::chrono::steady_clock::now();
    const auto node_cnt = perft(pos,depth);
    const auto end = std::chrono::steady_clock::now();

    const double seconds =
      std::chrono::duration<double>(end - begin).count();

    const double nps = seconds > 0.0
      ?static_cast<double>(node_cnt) / seconds
      :0.0;

    SO << "node " << node_cnt << NL;
    SO << "time " << seconds << NL;
    SO << "nps " << static_cast<u64>(nps) << NL;
  }

  void bench(
    std::istringstream& ss){
    verbose = false;
    int depth = 16;
    constexpr int num_pos = 32;
    ss >> depth;

    static const std::array<std::string, num_pos> fens = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -",
      "r1bn1rk1/ppp1qppp/3pp3/3P4/2P1n3/2B2NP1/PP2PPBP/2RQK2R w K -",
      "r2q1rk1/1bppbppp/p4n2/n2Np3/Pp2P3/1B1P1N2/1PP2PPP/R1BQ1RK1 w - -",
      "rnb2rk1/1pq1bppp/p3pn2/3p4/3NPP2/2N1B3/PPP1B1PP/R3QRK1 w - -",
      "2rq1rk1/p3bppp/bpn1pn2/2pp4/3P4/1P2PNP1/PBPN1PBP/R2QR1K1 w - -",
      "rn3rk1/1p2ppbp/1pp3p1/3n4/3P1Bb1/2N1PN2/PP3PPP/2R1KB1R w K -",
      "r1bq1rk1/3nbppp/p1p1pn2/1p4B1/3P4/2NBPN2/PP3PPP/2RQK2R w K -",
      "r3kbnr/1bpq2pp/p2p1p2/1p2p3/3PP2N/1PN5/1PP2PPP/R1BQ1RK1 w kq -",
      "r1b1k2r/pp1nqp1p/2p3p1/3p3n/3P4/2NBP3/PPQ2PPP/2KR2NR w kq -",
      "r2q1rk1/1b2ppbp/ppnp1np1/2p5/P3P3/2PP1NP1/1P1N1PBP/R1BQR1K1 w - -",
      "r2q1rk1/pp2ppbp/2n1bnp1/3p4/4PPP1/1NN1B3/PPP1B2P/R2QK2R w KQ -",
      "2q1r1k1/1ppb4/r2p1Pp1/p4n1p/2P1n3/5NPP/PP3Q1K/2BRRB2 w - -",
      "7r/1p2k3/2bpp3/p3np2/P1PR4/2N2PP1/1P4K1/3B4 b - -",
      "4k3/p1P3p1/2q1np1p/3N4/8/1Q3PP1/6KP/8 w - -",
      "2r1b1k1/R4pp1/4pb1p/1pBr4/1Pq2P2/3N4/2PQ2PP/5RK1 b - -",
      "6k1/p1qb1p1p/1p3np1/2b2p2/2B5/2P3N1/PP2QPPP/4N1K1 b - -",
      "1rr1nbk1/5ppp/3p4/1q1PpN2/np2P3/5Q1P/P1BB1PP1/2R1R1K1 w - -",
      "3q4/pp3pkp/5npN/2bpr1B1/4r3/2P2Q2/PP3PPP/R4RK1 w - -",
      "3rr1k1/pb3pp1/1p1q1b1p/1P2NQ2/3P4/P1NB4/3K1P1P/2R3R1 w - -",
      "r1b1r1k1/p1p3pp/2p2n2/2bp4/5P2/3BBQPq/PPPK3P/R4N1R b - -",
      "3r4/1b2k3/1pq1pp2/p3n1pr/2P5/5PPN/PP1N1QP1/R2R2K1 b - -",
      "2r4k/pB4bp/6p1/6q1/1P1n4/2N5/P4PPP/2R1Q1K1 b - -",
      "1N2k3/5p2/p2P2p1/3Pp3/pP3b2/5P1r/P7/1K4R1 b - -",
      "2k2R2/6r1/8/B2pp2p/1p6/3P4/PP2b3/2K5 b - -",
      "2k5/1pp5/2pb2p1/7p/6n1/P5N1/1PP3PP/2K1B3 b - -",
      "2n5/1k6/3pNn2/3ppp2/7p/4P2P/1P4P1/5NK1 w - -",
      "5nk1/B4p2/7p/6p1/3N3n/2r2PK1/5P1P/4R3 b - -",
      "8/1p3pkp/p1r3p1/3P3n/3p1P2/3P4/PP3KP1/R3N3 b - -",
      "8/2B2k2/p2p2pp/2pP1p2/2P2P2/2b1N1PP/P4K2/2n5 b - -",
      "8/4p1kp/1n1p2p1/nPp5/b5P1/P5KP/3N1P2/4NB2 w - -",
      "r1b3k1/2p4p/3p1p2/1p1P4/1P3P2/P5P1/5KNP/R7 b - -",
      "1k2b3/1pp5/4r3/R3N1pp/1P3P2/p5P1/2P4P/1K6 w - -",
    };

    search_info.set_num_threads(1);
    search_info.clear();
    u64 total_nodes = 0;
    const auto global_begin = std::chrono::steady_clock::now();

    for (size_t i = 0; i < fens.size(); ++i){
      const auto& fen = fens[i];

      std::cout << "Position " << (i + 1) << "/" << fens.size() << " ";
      std::cout << fen << "\n";
      pos = board(fen);

      std::cout.flush();

      search_info.time = {};
      search_info.time.use_depth_limit = true;
      search_info.time.depth_limit = depth;
      search_info.time.start();

      for (auto* td : search_info.thread_info)
        td->node_count = 0;

      search_info.best_move(pos);

      u64 pos_nodes = 0;
      for (const auto* td : search_info.thread_info)
        pos_nodes += td->node_count;

      total_nodes += pos_nodes;

      if (verbose)
        std::cout << "Nodes: " << pos_nodes << "\n";
    }

    const auto global_end = std::chrono::steady_clock::now();

    const double seconds =
      std::chrono::duration<double>(global_end - global_begin).count();

    const double nps = seconds > 0.0
      ?static_cast<double>(total_nodes) / seconds
      :0.0;

    std::cout << "Bench Results\n";
    std::cout << "Positions " << num_pos << "\n";
    std::cout << "Depth " << depth << "\n";
    std::cout << "Nodes " << total_nodes << "\n";
    std::cout << "Time " << seconds << " sec\n";
    std::cout << "NPS " << static_cast<u64>(nps) << "\n";
    std::cout.flush();
  }
}
