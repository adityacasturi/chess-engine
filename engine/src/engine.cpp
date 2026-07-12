#include "engine_internal.hpp"

#include <cctype>

namespace chess {

Move::Move(int from_square, int to_square, char promotion_piece)
    : from(static_cast<uint8_t>(from_square)),
      to(static_cast<uint8_t>(to_square)),
      promotion(static_cast<uint8_t>(promotion_piece)) {}

bool Move::operator==(const Move& other) const {
  return from == other.from && to == other.to &&
         std::tolower(static_cast<unsigned char>(promotion)) ==
             std::tolower(static_cast<unsigned char>(other.promotion));
}

std::optional<Move> parse_uci_move(const std::string& text) {
  if (text.size() < 4) {
    return std::nullopt;
  }
  int from = detail::parse_square(text, 0);
  int to = detail::parse_square(text, 2);
  if (from == -1 || to == -1) {
    return std::nullopt;
  }
  char promotion = 0;
  if (text.size() >= 5) {
    promotion = static_cast<char>(std::tolower(static_cast<unsigned char>(text[4])));
    if (promotion != 'q' && promotion != 'r' && promotion != 'b' && promotion != 'n') {
      return std::nullopt;
    }
  }
  return Move{from, to, promotion};
}

std::string move_to_uci(const Move& move) {
  if (!detail::on_board(move.from) || !detail::on_board(move.to)) {
    return "0000";
  }
  std::string out = detail::square_name(move.from) + detail::square_name(move.to);
  if (move.promotion) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(move.promotion))));
  }
  return out;
}

} // namespace chess
