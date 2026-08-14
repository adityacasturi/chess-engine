#include "engine.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

std::string json_escape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    if (ch == '"' || ch == '\\') {
      out.push_back('\\');
    }
    out.push_back(ch);
  }
  return out;
}

std::string result_json(const chess::Board& board,
                        bool accepted,
                        const std::string& engine_move,
                        int score,
                        uint64_t nodes,
                        int time_ms,
                        const std::string& error) {
  std::ostringstream out;
  out << '{'
      << "\"accepted\":" << (accepted ? "true" : "false") << ','
      << "\"fen\":\"" << json_escape(board.fen()) << "\","
      << "\"engineMove\":\"" << json_escape(engine_move) << "\","
      << "\"status\":\"" << json_escape(board.status()) << "\","
      << "\"score\":" << score << ','
      << "\"nodes\":" << nodes << ','
      << "\"timeMs\":" << time_ms;
  if (!error.empty()) {
    out << ",\"error\":\"" << json_escape(error) << '"';
  }
  out << '}';
  return out.str();
}

}  // namespace

extern "C" {

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
const char* engine_best_move(const char* fen, int depth) {
  static std::string out;

  if (fen == nullptr || fen[0] == '\0') {
    chess::Board board = chess::Board::starting_position();
    out = result_json(board, false, "", 0, 0, 0, "FEN is required");
    return out.c_str();
  }

  chess::Board board;
  if (!board.load_fen(fen)) {
    chess::Board fallback = chess::Board::starting_position();
    out = result_json(fallback, false, "", 0, 0, 0, "Invalid FEN");
    return out.c_str();
  }

  depth = std::max(1, std::min(5, depth));
  std::string engine_move;
  int score = 0;
  uint64_t nodes = 0;
  int time_ms = 0;

  const std::string status = board.status();
  if (status == "ongoing" || status == "check") {
    const bool root_white_to_move = board.white_to_move();
    const auto start = std::chrono::steady_clock::now();
    const auto result = chess::search_best_move(board, depth);
    const auto end = std::chrono::steady_clock::now();
    time_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    score = root_white_to_move ? result.score : -result.score;
    nodes = result.nodes;
    engine_move = chess::move_to_uci(result.move);
    if (engine_move != "0000") {
      board.make_move(result.move);
    }
  }

  out = result_json(board, true, engine_move, score, nodes, time_ms, "");
  return out.c_str();
}

}  // extern "C"
