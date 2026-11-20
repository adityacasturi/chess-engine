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

// Fixed-capacity move list that lives on the stack, so move generation never
// touches the heap. 256 exceeds the maximum number of moves in any legal
// chess position (218).
class MoveList {
public:
  static constexpr int kCapacity = 256;

  void push_back(const Move& move) { moves_[size_++] = move; }
  void clear() { size_ = 0; }
  void resize(int size) { size_ = size; }
  int size() const { return size_; }
  bool empty() const { return size_ == 0; }

  Move& operator[](int index) { return moves_[index]; }
  const Move& operator[](int index) const { return moves_[index]; }
  const Move& front() const { return moves_[0]; }

  Move* begin() { return moves_.data(); }
  Move* end() { return moves_.data() + size_; }
  const Move* begin() const { return moves_.data(); }
  const Move* end() const { return moves_.data() + size_; }

private:
  std::array<Move, kCapacity> moves_;
  int size_ = 0;
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
  MoveList legal_moves_fast();
  MoveList tactical_moves_fast();
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

  void pseudo_legal_moves(MoveList& moves) const;
  void pseudo_tactical_moves(MoveList& moves) const;
  void keep_legal(MoveList& moves);
  int pseudo_mobility(bool white) const;
  bool square_attacked(int square, bool by_white) const;
  int king_square(bool white) const;
};

std::optional<Move> parse_uci_move(const std::string& text);
std::string move_to_uci(const Move& move);
SearchResult search_best_move(const Board& board, int depth);
uint64_t perft(const Board& board, int depth);

} // namespace chess
