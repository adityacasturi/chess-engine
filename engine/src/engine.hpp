#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace chess {

struct Move {
  uint8_t from = 255;
  uint8_t to = 255;
  uint8_t promotion = 0;

  Move() = default;
  Move(int from_square, int to_square, char promotion_piece = 0);

  bool operator==(const Move& other) const;
};

struct SearchStats {
  uint64_t search_nodes = 0;
  uint64_t quiescence_nodes = 0;
  uint64_t static_evaluations = 0;
  uint64_t moves_generated = 0;
  uint64_t moves_searched = 0;
  uint64_t q_moves_generated = 0;
  uint64_t q_moves_searched = 0;
  uint64_t tt_probes = 0;
  uint64_t tt_hits = 0;
  uint64_t tt_cutoffs = 0;
  uint64_t tt_stores = 0;
  uint64_t beta_cutoffs = 0;
  int max_ply = 0;
  int max_quiescence_ply = 0;
  bool book_hit = false;
};

struct SearchResult {
  Move move;
  int score = 0;
  uint64_t nodes = 0;
  SearchStats stats;
};

class Board {
public:
  static Board starting_position();

  bool load_fen(const std::string& fen);
  std::string fen() const;

  bool white_to_move() const;
  std::vector<Move> legal_moves() const;
  bool make_move(const Move& move);
  bool is_legal(const Move& move) const;
  bool in_check(bool white) const;
  std::string status() const;
  int evaluate_white() const;
  char piece_at(int square) const;
  int en_passant_square() const;
  uint64_t hash_key() const;

  struct UndoState {
    bool white_to_move = true;
    bool castle_white_king = false;
    bool castle_white_queen = false;
    bool castle_black_king = false;
    bool castle_black_queen = false;
    int en_passant = -1;
    int halfmove_clock = 0;
    int fullmove_number = 1;
    int white_king_square = -1;
    int black_king_square = -1;
    std::array<int, 6> squares = {};
    std::array<char, 6> pieces = {};
    int square_count = 0;
  };
  std::vector<Move> legal_moves_fast();
  std::vector<Move> tactical_moves_fast();
  bool make_move_internal(const Move& move, UndoState* undo);
  void unmake_move(const UndoState& undo);

private:
  friend SearchResult search_best_move(const Board& board, int depth);
  friend uint64_t perft(const Board& board, int depth);

  char squares_[64] = {};
  bool white_to_move_ = true;
  bool castle_white_king_ = false;
  bool castle_white_queen_ = false;
  bool castle_black_king_ = false;
  bool castle_black_queen_ = false;
  int en_passant_ = -1;
  int halfmove_clock_ = 0;
  int fullmove_number_ = 1;
  int white_king_square_ = -1;
  int black_king_square_ = -1;

  std::vector<Move> pseudo_legal_moves() const;
  std::vector<Move> pseudo_tactical_moves() const;
  int pseudo_mobility(bool white) const;
  bool square_attacked(int square, bool by_white) const;
  int king_square(bool white) const;
};

std::optional<Move> parse_uci_move(const std::string& text);
std::string move_to_uci(const Move& move);
SearchResult search_best_move(const Board& board, int depth);
uint64_t perft(const Board& board, int depth);

} // namespace chess
