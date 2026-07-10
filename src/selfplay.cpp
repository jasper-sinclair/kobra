#include "selfplay.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <vector>
#include "movegen.h"
#include "uci.h"

namespace uci {
  namespace {
    std::mutex file_mutex;
    std::atomic games_completed=0;
    std::atomic<long long> total_positions=0;
    std::atomic<long long> total_plies=0;
    std::atomic white_wins=0;
    std::atomic black_wins=0;
    std::atomic draws=0;

    struct training_pos {
      std::string fen;
      bool white_to_move;
      int eval;
      int ply;
    };

    int piece_count(
      const board& position) {
      int count=-2;

      for (u8 sq=0; sq < 64; sq++)
        if (position.piece_on(sq) != no_piece)
          count++;

      return count;
    }

    bool has_legal_moves(
      board& position) {
      move_list moves;
      gen_moves(position, moves);

      for (size_t i=0; i < moves.size(); ++i) {
        if (const u16 m=moves.move(i); position.is_legal(m))
          return true;
      }
      return false;
    }

    float game_result(
      board& position) {
      if (position.is_draw())
        return 0.5f;

      if (!has_legal_moves(position)) {
        if (position.is_in_check()) {
          return position.side_to_move == white ? 0.0f : 1.0f;
        }

        return 0.5f;
      }

      return 0.5f;
    }

    float sigmoid_eval(
      const int eval) {
      constexpr float inv_scale=1.0f / 400.0f;
      return 1.0f / (1.0f + std::exp(-static_cast<float>(eval) * inv_scale));
    }

    void selfplay_worker(
      int games,
      int movetime,
      int depth,
      int nodes) {
      search engine;

      engine.selfplay_mode=true;
      engine.set_hash_size(16);
      engine.set_num_threads(1);

      thread_local std::ofstream out("training.txt", std::ios::app);

      thread_local std::mt19937 rng(std::random_device{}());

      for (int g=0; g < games; ++g) {
        engine.clear();

        board position(start_fen);

        std::uniform_int_distribution plies_dist(0, 5);
        int random_plies=6 + plies_dist(rng);

        for (int i=0; i < random_plies; i++) {
          move_list moves;
          gen_moves(position, moves);

          std::vector<u16> legal;

          for (size_t j=0; j < moves.size(); j++) {
            if (u16 m=moves.move(j); position.is_legal(m))
              legal.push_back(m);
          }

          if (legal.empty())
            break;

          std::uniform_int_distribution<size_t> dist(0, legal.size() - 1);
          position.apply_move(legal[dist(rng)]);
          if (!has_legal_moves(position))
            break;
        }

        int ply=0;

        std::vector<training_pos> game_positions;

        int eval=0;

        while (true) {
          if (position.is_draw() && ply > 60)
            break;

          if (!has_legal_moves(position))
            break;

          if (ply > 120)
            break;

          engine.time={};

          if (nodes > 0) {
            engine.time.use_node_limit=true;
            engine.time.node_limit=nodes;
          }
          else if (depth > 0) {
            engine.time.use_depth_limit=true;
            engine.time.depth_limit=depth;
          }
          else {
            engine.time.use_move_limit=true;
            engine.time.move_time_limit=movetime;
            engine.time.start();
          }

          u16 best=engine.best_move(position);

          eval=engine.get_last_score();

          if (!best)
            break;

          ply++;

          // sample positions only every 8 plies to reduce correlation
          if (ply > 16 && ply % 8 == 0) {
            game_positions.push_back({
              .fen=position.fen(),
              .white_to_move=position.side_to_move == white,
              .eval=eval,
              .ply=ply
              });
          }

          position.apply_move(best);
        }

        float result=game_result(position);

        if (result > 0.99f)
          ++white_wins;
        else if (result < 0.01f)
          ++black_wins;
        else
          ++draws;

        float white_result=result;
        float training_result=white_result;

        std::ostringstream buffer;
        int written_positions=0;

        for (const auto& p : game_positions) {
          if (p.ply < 16)
            continue;

          if (p.ply > ply - 8)
            continue;

          float label=
            p.white_to_move ? training_result : 1.0f - training_result;

          label=std::clamp(label, 0.0f, 1.0f);

          buffer << p.fen << " | " << label << "\n";
          written_positions++;
        }

        {
          std::scoped_lock lock(file_mutex);

          out << buffer.str();

          if ((g + 1) % 50 == 0)
            out.flush();
        }

        ++games_completed;
        total_positions+=written_positions;
        total_plies+=ply;
      }
    }
  }

  void run_selfplay(
    const int games,
    int movetime,
    int depth,
    int nodes,
    int threads) {
    games_completed=0;
    total_positions=0;
    total_plies=0;

    white_wins=0;
    black_wins=0;
    draws=0;

    const bool old_verbose=verbose;

    verbose=false;

    if (threads <= 0) {
      threads=static_cast<int>(std::thread::hardware_concurrency());
      if (threads <= 0)
        threads=1;
    }

    threads=std::min(threads, games);

    std::cout << "hardware_concurrency = "
      << std::thread::hardware_concurrency()
      << "\n";

    std::cout << "threads = " << threads << "\n";
    if (threads > 0)
      std::cout << "games per thread ~ " << (games / threads) << "\n";

    std::vector<std::thread> workers;

    const int games_per_thread=games / threads;
    const int remainder=games % threads;

    for (int i=0; i < threads; i++) {
      int count=
        games_per_thread +
        (i < remainder ? 1 : 0);

      workers.emplace_back(
        selfplay_worker,
        count,
        movetime,
        depth,
        nodes);
    }

    using namespace std::chrono_literals;

    const auto start=
      std::chrono::steady_clock::now();

    double smoothed_games_per_sec=0.0;
    int last_done=0;
    auto last_time=start;

    while (games_completed < games) {
      std::this_thread::sleep_for(1s);

      const int done=games_completed.load();

      const long long positions=
        total_positions.load();

      const long long plies=
        total_plies.load();

      auto now=
        std::chrono::steady_clock::now();

      const double seconds=
        std::chrono::duration<double>(now - start).count();

      const double percent=
        100.0 * done / games;

      auto dt=
        std::chrono::duration<double>(now - last_time).count();

      double instant_gps=0.0;

      if (dt > 0.0)
        instant_gps=(done - last_done) / dt;

      if (smoothed_games_per_sec == 0.0)
        smoothed_games_per_sec=instant_gps;
      else
        smoothed_games_per_sec=
        0.85 * smoothed_games_per_sec +
        0.15 * instant_gps;

      const double games_per_sec=smoothed_games_per_sec;

      last_done=done;
      last_time=now;

      const double pos_per_sec=
        static_cast<double>(positions) / seconds;

      const double plies_per_game=
        done > 0 ? static_cast<double>(plies) / done : 0.0;

      const double remaining_games=
        games - done;

      double eta_seconds=0.0;

      if (seconds > 5.0 && games_per_sec > 0.0)
        eta_seconds=remaining_games / games_per_sec;

      const int eta_min=
        static_cast<int>(eta_seconds / 60);

      const int eta_sec=
        static_cast<int>(eta_seconds) % 60;

      const int elapsed_hours=
        static_cast<int>(seconds / 3600);

      const int elapsed_minutes=
        static_cast<int>(seconds) % 3600 / 60;

      const int elapsed_seconds=
        static_cast<int>(seconds) % 60;

      const int w=white_wins.load();
      const int b=black_wins.load();
      const int d=draws.load();

      const int total=w + b + d;

      const double w_pct=
        total ? 100.0 * w / total : 0.0;

      const double b_pct=
        total ? 100.0 * b / total : 0.0;

      const double draw_pct=
        total ? 100.0 * d / total : 0.0;

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

      std::string out=line.str();

      if (out.length() < 140)
        out+=std::string(140 - out.length(), ' ');

      std::cout << out << std::flush;
    }

    for (auto& t : workers)
      t.join();

    const auto end=
      std::chrono::steady_clock::now();

    const double total_seconds=
      std::chrono::duration<double>(end - start).count();

    std::cout << "\nSelfplay finished in "
      << static_cast<int>(total_seconds / 3600) << "h "
      << static_cast<int>(total_seconds) % 3600 / 60 << "m "
      << static_cast<int>(total_seconds) % 60 << "s\n";

    verbose=old_verbose;
  }
}