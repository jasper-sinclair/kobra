#include "selfplay.h"
#include <atomic>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include "movegen.h"
#include "uci.h"

namespace uci{
  namespace{
    std::mutex file_mutex;
    std::atomic games_completed = 0;
    std::atomic<long long> total_positions = 0;
    std::atomic<long long> total_plies = 0;
    std::atomic white_wins = 0;
    std::atomic black_wins = 0;
    std::atomic draws = 0;

    bool has_legal_moves(
      board& position){
      move_list moves;
      gen_moves(position,moves);

      for (size_t i = 0; i < moves.size(); ++i){
        if (const u16 m = moves.move(i); ! position.is_legal(m))
          continue;
        return true;
      }
      return false;
    }

    float game_result(
      board& position){
      if (position.is_draw())
        return 0.5f;

      if (! has_legal_moves(position)){
        if (position.is_in_check()){
          return position.side_to_move == white?0.0f:1.0f;
        }
        return 0.5f;
      }
      return 0.5f;
    }

    void selfplay_worker(
      int games,
      int movetime,
      int depth){
      search engine;
      engine.selfplay_mode = true;
      engine.set_hash_size(16);
      engine.set_num_threads(1);
      static std::ofstream out("training.txt",std::ios::app);

      thread_local std::mt19937 rng(std::random_device{}());
      std::uniform_int_distribution parity_dist(0,1);

      for (int g = 0; g < games; ++g){
        engine.clear();
        board position(start_fen);
        std::uniform_int_distribution plies_dist(0,5);
        int random_plies = 6 + plies_dist(rng);

        for (int i = 0; i < random_plies; ++i){
          move_list moves;
          gen_moves(position,moves);

          std::vector<u16> legal;

          for (size_t j = 0; j < moves.size(); ++j){
            if (u16 m = moves.move(j); position.is_legal(m))
              legal.push_back(m);
          }

          if (legal.empty())
            break;

          std::uniform_int_distribution<size_t> dist(0,legal.size() - 1);
          u16 m = legal[dist(rng)];

          position.apply_move(m);
        }
        int ply = 0;
        int parity = parity_dist(rng);

        std::vector<std::string> game_positions;
        int eval = 0;
        while (true){
          if (position.is_draw() && ply > 60)
            break;

          if (! has_legal_moves(position))
            break;

          if (ply > 150)
            break;

          if (ply % 2 == parity)
            game_positions.push_back(position.fen());

          engine.time = {};

          if (depth > 0){
            engine.time.use_depth_limit = true;
            engine.time.depth_limit = depth;
          } else{
            engine.time.use_move_limit = true;
            engine.time.move_time_limit = movetime;
            engine.time.start();
          }

          u16 best = engine.best_move(position);
          eval = engine.get_last_score();

          if (! best)
            break;

          position.apply_move(best);
          ply++;
        }

        float result = 0.0;

        if (ply > 80){
          int white_eval = position.side_to_move == white
            ?eval
            :-eval;

          if (white_eval > 200)
            result = 1.0f;
          else if (white_eval < -200)
            result = 0.0f;
          else
            result = game_result(position);
        } else{
          result = game_result(position);
        }

        float white_result;

        if (position.side_to_move == white)
          white_result = result;
        else
          white_result = 1.0f - result;

        if (white_result == 1.0f)
          ++white_wins;
        else if (white_result == 0.0f)
          ++black_wins;
        else
          ++draws;

        std::ostringstream buffer;

        for (const auto& fen : game_positions)
          buffer << fen << " | " << result << "\n";

        {
          std::scoped_lock lock(file_mutex);
          out << buffer.str();

          if ((g + 1) % 50 == 0)
            out.flush();
        }
        ++games_completed;
        total_positions += static_cast<long long>(game_positions.size());
        total_plies += ply;
      }
    }
  }

  void run_selfplay(
    const int games,
    int movetime,
    int depth){
    games_completed = 0;
    total_positions = 0;
    total_plies = 0;
    white_wins = 0;
    black_wins = 0;
    draws = 0;

    const bool old_verbose = verbose;
    verbose = false;

    int threads = std::min(games,static_cast<int>(std::thread::hardware_concurrency()));
    if (threads == 0) threads = 4;
    std::cout << "hardware_concurrency = "
      << std::thread::hardware_concurrency() << "\n";
    std::cout << "threads = " << threads << "\n";
    std::vector<std::thread> workers;

    const int games_per_thread = games / threads;
    const int remainder = games % threads;

    for (int i = 0; i < threads; ++i){
      int count = games_per_thread + (i < remainder?1:0);
      workers.emplace_back(selfplay_worker,count,movetime,depth);
    }
    using namespace std::chrono_literals;

    const auto start = std::chrono::steady_clock::now();

    while (games_completed < games){
      std::this_thread::sleep_for(1s);

      const int done = games_completed.load();
      const long long positions = total_positions.load();
      const long long plies = total_plies.load();

      auto now = std::chrono::steady_clock::now();
      const double seconds =
        std::chrono::duration<double>(now - start).count();

      const double percent = 100.0 * done / games;
      const double games_per_sec = done / seconds;
      const double pos_per_sec = static_cast<double>(positions) / seconds;
      const double plies_per_game = done > 0?static_cast<double>(plies) / done:0.0;
      const double remaining_games = games - done;
      const double eta_seconds = games_per_sec > 0?remaining_games / games_per_sec:0.0;
      const int eta_min = static_cast<int>(eta_seconds / 60);
      const int eta_sec = static_cast<int>(eta_seconds) % 60;
      const int elapsed_hours = static_cast<int>(seconds / 3600);
      const int elapsed_minutes = static_cast<int>(seconds) % 3600 / 60;
      const int elapsed_seconds = static_cast<int>(seconds) % 60;
      const int w = white_wins.load();
      const int b = black_wins.load();
      const int d = draws.load();
      const int total = w + b + d;
      const double w_pct = total?100.0 * w / total:0.0;
      const double b_pct = total?100.0 * b / total:0.0;
      const double draw_pct = total?100.0 * d / total:0.0;

      std::cout << std::fixed << std::setprecision(2);
      std::ostringstream line;

      line << std::fixed << std::setprecision(2);
      line << "\nprogress: "
        << done << "/" << games
        << " (" << percent << "%)"
        << " | " << games_per_sec << " g/s"
        << " | " << pos_per_sec << " pos/s"
        << " | plies/g " << plies_per_game
        << " | W " << w_pct << "%"
        << " | B " << b_pct << "%"
        << " | D " << draw_pct << "%"
        << " | elapsed "
        << elapsed_hours << "h "
        << elapsed_minutes << "m "
        << elapsed_seconds << "s"
        << " | ETA "
        << eta_min << "m "
        << eta_sec << "s";

      std::string out = line.str();
      if (out.length() < 140)
        out += std::string(140 - out.length(),' ');

      std::cout << out << std::flush;
    }

    for (auto& t : workers)
      t.join();

    const auto end = std::chrono::steady_clock::now();
    const double total_seconds =
      std::chrono::duration<double>(end - start).count();

    const int total_hours = static_cast<int>(total_seconds / 3600);
    const int total_minutes = static_cast<int>(total_seconds) % 3600 / 60;
    const int total_secs = static_cast<int>(total_seconds) % 60;

    std::cout << "\nSelfplay finished in "
      << total_hours << "h "
      << total_minutes << "m "
      << total_secs << "s\n";
    verbose = old_verbose;
  }
}
