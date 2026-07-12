#include "engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Position {
  std::string name;
  std::string fen;
  std::string tag;
};

struct PerftCase {
  std::string name;
  std::string fen;
  int depth;
  uint64_t expected;
};

struct TimedSearch {
  Position position;
  int depth = 0;
  chess::SearchResult result;
  double seconds = 0.0;
};

struct BenchSummary {
  double best_perft_nps = 0.0;
  std::string best_perft_name;
  int best_perft_depth = 0;
  uint64_t best_perft_nodes = 0;
  uint64_t total_perft_nodes = 0;
  double total_perft_seconds = 0.0;
  bool perft_all_correct = true;

  double best_search_nps = 0.0;
  std::string best_search_name;
  int best_search_depth = 0;
  uint64_t best_search_nodes = 0;
  double best_search_seconds = 0.0;
  std::string best_search_move;

  uint64_t largest_search_nodes = 0;
  std::string largest_search_name;
  int largest_search_depth = 0;
  double largest_search_seconds = 0.0;
  std::string largest_search_move;

  uint64_t total_search_nodes = 0;
  double total_search_seconds = 0.0;
  uint64_t total_beta_cutoffs = 0;
  uint64_t total_tt_probes = 0;
  uint64_t total_tt_hits = 0;
  uint64_t total_q_nodes = 0;
  int max_depth_completed = 0;
};

double elapsed_seconds(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration<double>(end - start).count();
}

std::string compact_number(double value) {
  std::ostringstream out;
  if (value >= 1'000'000.0) {
    out << std::fixed << std::setprecision(2) << value / 1'000'000.0 << "M";
  } else if (value >= 1'000.0) {
    out << std::fixed << std::setprecision(1) << value / 1'000.0 << "K";
  } else {
    out << std::fixed << std::setprecision(0) << value;
  }
  return out.str();
}

std::string percent(double numerator, double denominator) {
  std::ostringstream out;
  if (denominator <= 0.0) {
    out << "0.0%";
  } else {
    out << std::fixed << std::setprecision(1) << (100.0 * numerator / denominator) << "%";
  }
  return out.str();
}

std::string decimal(double value, int precision = 2) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(precision) << value;
  return out.str();
}

chess::Board board_from_fen(const std::string& fen) {
  chess::Board board;
  if (!board.load_fen(fen)) {
    std::cerr << "Invalid FEN: " << fen << '\n';
    std::exit(1);
  }
  return board;
}

std::vector<Position> benchmark_positions() {
  return {
      {"post-book", "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "opening"},
      {"middlegame", "r2q1rk1/ppp2ppp/2n1bn2/3pp3/3PP3/2P1BN2/PP3PPP/R2QKB1R w KQ - 2 8", "strategy"},
      {"tactical", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", "tactics"},
      {"endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", "endgame"},
      {"black-to-move", "r1bq1rk1/ppp2ppp/2n1pn2/3p4/3P4/2PBPN2/PP3PPP/R1BQ1RK1 b - - 1 8", "black"},
  };
}

void print_header(const std::string& title) {
  std::cout << "\n== " << title << " ==\n";
}

bool is_capture_or_promotion(const chess::Board& board, const chess::Move& move) {
  if (move.promotion) return true;
  char mover = board.piece_at(move.from);
  char target = board.piece_at(move.to);
  if (target != '.') return true;
  return std::tolower(static_cast<unsigned char>(mover)) == 'p' && move.to == board.en_passant_square();
}

void run_position_profile() {
  print_header("Root position profile");
  std::cout << std::left << std::setw(14) << "position"
            << std::setw(10) << "type"
            << std::right << std::setw(8) << "legal"
            << std::setw(10) << "captures"
            << std::setw(11) << "promotions"
            << std::setw(9) << "checks"
            << std::setw(9) << "eval" << '\n';

  for (const auto& position : benchmark_positions()) {
    auto board = board_from_fen(position.fen);
    auto moves = board.legal_moves();
    int captures = 0;
    int promotions = 0;
    int checks = 0;

    for (const auto& move : moves) {
      if (is_capture_or_promotion(board, move)) captures++;
      if (move.promotion) promotions++;
      auto next = board;
      next.make_move(move);
      if (next.in_check(next.white_to_move())) checks++;
    }

    std::cout << std::left << std::setw(14) << position.name
              << std::setw(10) << position.tag
              << std::right << std::setw(8) << moves.size()
              << std::setw(10) << captures
              << std::setw(11) << promotions
              << std::setw(9) << checks
              << std::setw(9) << board.evaluate_white() << '\n';
  }
}

void run_perft_benchmarks(BenchSummary& summary) {
  const std::vector<PerftCase> cases = {
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20},
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400},
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902},
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
      {"endgame", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 0},
  };

  print_header("Move generation perft");
  std::cout << std::left << std::setw(12) << "position"
            << std::right << std::setw(7) << "depth"
            << std::setw(16) << "nodes"
            << std::setw(16) << "expected"
            << std::setw(10) << "status"
            << std::setw(12) << "seconds"
            << std::setw(16) << "nodes/sec" << '\n';

  for (const auto& test : cases) {
    auto board = board_from_fen(test.fen);
    auto start = Clock::now();
    uint64_t nodes = chess::perft(board, test.depth);
    auto end = Clock::now();
    double seconds = elapsed_seconds(start, end);
    double nps = seconds > 0.0 ? static_cast<double>(nodes) / seconds : 0.0;
    bool known_expected = test.expected > 0;
    bool correct = !known_expected || nodes == test.expected;
    summary.perft_all_correct = summary.perft_all_correct && correct;
    summary.total_perft_nodes += nodes;
    summary.total_perft_seconds += seconds;

    if (nps > summary.best_perft_nps) {
      summary.best_perft_nps = nps;
      summary.best_perft_name = test.name;
      summary.best_perft_depth = test.depth;
      summary.best_perft_nodes = nodes;
    }

    std::cout << std::left << std::setw(12) << test.name
              << std::right << std::setw(7) << test.depth
              << std::setw(16) << nodes
              << std::setw(16) << (known_expected ? std::to_string(test.expected) : "-")
              << std::setw(10) << (correct ? "ok" : "FAIL")
              << std::setw(12) << std::fixed << std::setprecision(3) << seconds
              << std::setw(16) << std::fixed << std::setprecision(0) << nps << '\n';
  }
}

void update_search_summary(BenchSummary& summary, const TimedSearch& run) {
  double nps = run.seconds > 0.0 ? static_cast<double>(run.result.nodes) / run.seconds : 0.0;
  summary.total_search_nodes += run.result.nodes;
  summary.total_search_seconds += run.seconds;
  summary.total_beta_cutoffs += run.result.stats.beta_cutoffs;
  summary.total_tt_probes += run.result.stats.tt_probes;
  summary.total_tt_hits += run.result.stats.tt_hits;
  summary.total_q_nodes += run.result.stats.quiescence_nodes;
  summary.max_depth_completed = std::max(summary.max_depth_completed, run.depth);

  if (nps > summary.best_search_nps) {
    summary.best_search_nps = nps;
    summary.best_search_name = run.position.name;
    summary.best_search_depth = run.depth;
    summary.best_search_nodes = run.result.nodes;
    summary.best_search_seconds = run.seconds;
    summary.best_search_move = chess::move_to_uci(run.result.move);
  }

  if (run.result.nodes > summary.largest_search_nodes) {
    summary.largest_search_nodes = run.result.nodes;
    summary.largest_search_name = run.position.name;
    summary.largest_search_depth = run.depth;
    summary.largest_search_seconds = run.seconds;
    summary.largest_search_move = chess::move_to_uci(run.result.move);
  }
}

TimedSearch run_search(const Position& position, int depth) {
  auto board = board_from_fen(position.fen);
  auto start = Clock::now();
  auto result = chess::search_best_move(board, depth);
  auto end = Clock::now();
  return {position, depth, result, elapsed_seconds(start, end)};
}

void run_search_benchmarks(BenchSummary& summary) {
  const std::vector<int> depths = {1, 2, 3, 4, 5};

  print_header("Alpha-beta search telemetry");
  std::cout << std::left << std::setw(14) << "position"
            << std::right << std::setw(6) << "depth"
            << std::setw(8) << "move"
            << std::setw(8) << "score"
            << std::setw(12) << "nodes"
            << std::setw(11) << "qnodes"
            << std::setw(10) << "sec"
            << std::setw(13) << "nodes/sec"
            << std::setw(10) << "cutoffs"
            << std::setw(9) << "tt hit"
            << std::setw(9) << "qshare"
            << std::setw(8) << "eff bf" << '\n';

  for (const auto& position : benchmark_positions()) {
    for (int depth : depths) {
      TimedSearch run = run_search(position, depth);
      update_search_summary(summary, run);

      double nps = run.seconds > 0.0 ? static_cast<double>(run.result.nodes) / run.seconds : 0.0;
      double qshare = run.result.nodes > 0
                          ? static_cast<double>(run.result.stats.quiescence_nodes) / run.result.nodes
                          : 0.0;
      double effective_branching = run.result.nodes > 0 && depth > 0
                                       ? std::pow(static_cast<double>(run.result.nodes), 1.0 / depth)
                                       : 0.0;

      std::cout << std::left << std::setw(14) << position.name
                << std::right << std::setw(6) << depth
                << std::setw(8) << chess::move_to_uci(run.result.move)
                << std::setw(8) << run.result.score
                << std::setw(12) << run.result.nodes
                << std::setw(11) << run.result.stats.quiescence_nodes
                << std::setw(10) << std::fixed << std::setprecision(3) << run.seconds
                << std::setw(13) << std::fixed << std::setprecision(0) << nps
                << std::setw(10) << run.result.stats.beta_cutoffs
                << std::setw(9) << percent(run.result.stats.tt_hits, run.result.stats.tt_probes)
                << std::setw(9) << percent(qshare, 1.0)
                << std::setw(8) << decimal(effective_branching, 1) << '\n';
    }
  }
}

void run_deep_dive(BenchSummary& summary) {
  print_header("Depth-6 stress sample");
  Position position{"post-book", "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "opening"};
  TimedSearch run = run_search(position, 6);
  update_search_summary(summary, run);

  double nps = run.seconds > 0.0 ? static_cast<double>(run.result.nodes) / run.seconds : 0.0;
  std::cout << "position: " << position.name << '\n';
  std::cout << "best move: " << chess::move_to_uci(run.result.move) << ", score: " << run.result.score << '\n';
  std::cout << "positions searched: " << run.result.nodes << '\n';
  std::cout << "elapsed: " << decimal(run.seconds, 3) << "s, throughput: " << compact_number(nps)
            << " positions/sec\n";
  std::cout << "search nodes: " << run.result.stats.search_nodes
            << ", quiescence nodes: " << run.result.stats.quiescence_nodes
            << ", static evals: " << run.result.stats.static_evaluations << '\n';
  std::cout << "moves generated: " << run.result.stats.moves_generated
            << ", moves searched: " << run.result.stats.moves_searched
            << ", beta cutoffs: " << run.result.stats.beta_cutoffs << '\n';
  std::cout << "tt probes: " << run.result.stats.tt_probes
            << ", tt hits: " << run.result.stats.tt_hits
            << ", tt cutoffs: " << run.result.stats.tt_cutoffs
            << ", tt stores: " << run.result.stats.tt_stores << '\n';
  std::cout << "max search ply: " << run.result.stats.max_ply
            << ", max quiescence ply: " << run.result.stats.max_quiescence_ply << '\n';
}

void run_opening_book_probe() {
  const std::vector<Position> book_positions = {
      {"london", "rnbqkbnr/ppp1pppp/8/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR b KQkq - 1 2", "book"},
      {"queens-gambit", "rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2", "book"},
      {"english", "rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1", "book"},
      {"reti", "rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq - 1 1", "book"},
      {"open-game", "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", "book"},
  };

  print_header("Opening book probe");
  std::cout << std::left << std::setw(16) << "position"
            << std::right << std::setw(10) << "move"
            << std::setw(12) << "book hit"
            << std::setw(16) << "nodes" << '\n';

  for (const auto& position : book_positions) {
    auto board = board_from_fen(position.fen);
    auto result = chess::search_best_move(board, 5);
    std::cout << std::left << std::setw(16) << position.name
              << std::right << std::setw(10) << chess::move_to_uci(result.move)
              << std::setw(12) << (result.stats.book_hit ? "yes" : "no")
              << std::setw(16) << result.nodes << '\n';
  }
}

void run_digestible_metrics(const BenchSummary& summary) {
  print_header("Digestible metrics");
  double perft_nps = summary.total_perft_seconds > 0.0
                         ? static_cast<double>(summary.total_perft_nodes) / summary.total_perft_seconds
                         : 0.0;
  double search_nps = summary.total_search_seconds > 0.0
                          ? static_cast<double>(summary.total_search_nodes) / summary.total_search_seconds
                          : 0.0;

  std::cout << "Perft correctness: " << (summary.perft_all_correct ? "all known cases passed" : "failure detected")
            << '\n';
  std::cout << "Move generation peak: " << compact_number(summary.best_perft_nps)
            << " legal positions/sec"
            << " (" << summary.best_perft_name << ", depth " << summary.best_perft_depth
            << ", " << compact_number(static_cast<double>(summary.best_perft_nodes))
            << " positions counted)\n";
  std::cout << "Move generation aggregate: " << compact_number(perft_nps)
            << " legal positions/sec across " << compact_number(static_cast<double>(summary.total_perft_nodes))
            << " generated positions\n";

  std::cout << "Search peak: " << compact_number(summary.best_search_nps)
            << " positions/sec"
            << " (" << summary.best_search_name << ", depth " << summary.best_search_depth
            << ", " << compact_number(static_cast<double>(summary.best_search_nodes))
            << " positions searched in " << decimal(summary.best_search_seconds, 3)
            << "s"
            << ", best move " << summary.best_search_move << ")\n";
  std::cout << "Largest search: "
            << compact_number(static_cast<double>(summary.largest_search_nodes))
            << " positions searched"
            << " (" << summary.largest_search_name << ", depth " << summary.largest_search_depth
            << ", " << decimal(summary.largest_search_seconds, 3)
            << "s"
            << ", " << compact_number(summary.largest_search_nodes / summary.largest_search_seconds)
            << " positions/sec"
            << ", best move " << summary.largest_search_move << ")\n";
  std::cout << "Search aggregate: " << compact_number(search_nps)
            << " positions/sec across " << compact_number(static_cast<double>(summary.total_search_nodes))
            << " searched positions\n";
  std::cout << "Pruning telemetry: " << compact_number(static_cast<double>(summary.total_beta_cutoffs))
            << " alpha-beta cutoffs, " << percent(summary.total_tt_hits, summary.total_tt_probes)
            << " transposition-table hit rate, " << percent(summary.total_q_nodes, summary.total_search_nodes)
            << " quiescence-node share\n";
  std::cout << "Max completed benchmark depth: " << summary.max_depth_completed << '\n';
}

} // namespace

int main() {
  BenchSummary summary;
  std::cout << "Custom chess engine benchmark\n";
  run_position_profile();
  run_perft_benchmarks(summary);
  run_search_benchmarks(summary);
  run_deep_dive(summary);
  run_opening_book_probe();
  run_digestible_metrics(summary);
  return 0;
}
