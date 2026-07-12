#include "engine.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string json_escape(const std::string& value) {
  std::string out;
  for (char ch : value) {
    if (ch == '"' || ch == '\\') {
      out.push_back('\\');
    }
    out.push_back(ch);
  }
  return out;
}

void print_result(const chess::Board& board,
                  bool accepted,
                  const std::string& accepted_move,
                  const std::string& engine_move,
                  int score,
                  uint64_t nodes,
                  int time_ms,
                  const std::string& error = "") {
  std::cout << "result {"
            << "\"accepted\":" << (accepted ? "true" : "false") << ','
            << "\"fen\":\"" << json_escape(board.fen()) << "\","
            << "\"acceptedMove\":\"" << json_escape(accepted_move) << "\","
            << "\"engineMove\":\"" << json_escape(engine_move) << "\","
            << "\"status\":\"" << board.status() << "\","
            << "\"score\":" << score << ','
            << "\"nodes\":" << nodes << ','
            << "\"timeMs\":" << time_ms;
  if (!error.empty()) {
    std::cout << ",\"error\":\"" << json_escape(error) << "\"";
  }
  std::cout << "}" << std::endl;
}

} // namespace

int main() {
  chess::Board board = chess::Board::starting_position();
  std::string line;

  while (std::getline(std::cin, line)) {
    if (line == "uci") {
      std::cout << "id name CustomChessEngine" << std::endl;
      std::cout << "id author Codex" << std::endl;
      std::cout << "uciok" << std::endl;
      continue;
    }
    if (line == "isready") {
      std::cout << "readyok" << std::endl;
      continue;
    }
    if (line == "ucinewgame") {
      board = chess::Board::starting_position();
      continue;
    }
    if (line == "quit") {
      break;
    }
    if (line.rfind("position fen ", 0) == 0) {
      board.load_fen(line.substr(13));
      continue;
    }
    if (line.rfind("go depth ", 0) == 0) {
      int depth = std::max(1, std::stoi(line.substr(9)));
      auto result = chess::search_best_move(board, depth);
      std::cout << "bestmove " << chess::move_to_uci(result.move) << std::endl;
      continue;
    }
    if (line.rfind("best ", 0) == 0) {
      size_t depth_pos = line.find("depth ");
      size_t fen_pos = line.find(" fen ");
      if (depth_pos == std::string::npos || fen_pos == std::string::npos || depth_pos > fen_pos) {
        print_result(board, false, "", "", 0, 0, 0, "Malformed best command");
        continue;
      }

      int depth = std::max(1, std::stoi(line.substr(depth_pos + 6, fen_pos - (depth_pos + 6))));
      std::string fen = line.substr(fen_pos + 5);
      if (!board.load_fen(fen)) {
        print_result(board, false, "", "", 0, 0, 0, "Invalid FEN");
        continue;
      }

      std::string engine_move;
      int score = 0;
      uint64_t nodes = 0;
      int time_ms = 0;

      if (board.status() == "ongoing" || board.status() == "check") {
        bool root_white_to_move = board.white_to_move();
        auto start = std::chrono::steady_clock::now();
        auto result = chess::search_best_move(board, depth);
        auto end = std::chrono::steady_clock::now();
        time_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        score = root_white_to_move ? result.score : -result.score;
        nodes = result.nodes;
        engine_move = chess::move_to_uci(result.move);
        if (engine_move != "0000") {
          board.make_move(result.move);
        }
      }

      print_result(board, true, "", engine_move, score, nodes, time_ms);
      continue;
    }
  }

  return 0;
}
