#include "engine_internal.hpp"

#include <sstream>
#include <utility>

namespace chess::detail {
namespace {

std::string position_key(const Board& board) {
  std::istringstream fen(board.fen());
  std::string placement;
  std::string side;
  std::string castling;
  fen >> placement >> side >> castling;
  return placement + " " + side + " " + castling;
}

std::optional<Move> parse_book_move(const std::string& text) {
  if (text.size() < 4) return std::nullopt;
  int from = parse_square(text, 0);
  int to = parse_square(text, 2);
  if (from == -1 || to == -1) return std::nullopt;
  char promotion = text.size() >= 5 ? text[4] : 0;
  return Move{from, to, promotion};
}

} // namespace

std::optional<Move> opening_book_move(const Board& board) {
  static const std::vector<std::pair<std::string, std::string>> book = {
      {"rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq", "d7d5"},
      {"rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq", "e7e5"},
      {"rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R b KQkq", "d7d5"},
      {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq", "e7e5"},
      {"rnbqkbnr/ppp1pppp/8/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR b KQkq", "g8f6"},
      {"rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq", "e7e6"},
      {"rnbqkbnr/ppp1pppp/8/3p4/3P4/5N2/PPP1PPPP/RNBQKB1R b KQkq", "g8f6"},
      {"rnbqkbnr/ppp1pppp/8/3p4/3P4/4P3/PPP2PPP/RNBQKBNR b KQkq", "g8f6"},
      {"rnbqkbnr/pppp1ppp/8/4p3/2P5/2N5/PP1PPPPP/R1BQKBNR b KQkq", "g8f6"},
      {"rnbqkbnr/pppp1ppp/8/4p3/2P5/6P1/PP1PPP1P/RNBQKBNR b KQkq", "g8f6"},
      {"rnbqkbnr/pppp1ppp/8/4p3/2P5/5N2/PP1PPPPP/RNBQKB1R b KQkq", "b8c6"},
      {"rnbqkbnr/ppp1pppp/8/3p4/8/5NP1/PPPPPP1P/RNBQKB1R b KQkq", "g8f6"},
      {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq", "b8c6"},
      {"rnbqkbnr/pppp1ppp/8/4p3/2B1P3/8/PPPP1PPP/RNBQK1NR b KQkq", "g8f6"},
  };

  std::string key = position_key(board);
  for (const auto& [book_key, move_text] : book) {
    if (book_key == key) {
      auto move = parse_book_move(move_text);
      if (move && board.is_legal(*move)) {
        return move;
      }
    }
  }
  return std::nullopt;
}

} // namespace chess::detail
