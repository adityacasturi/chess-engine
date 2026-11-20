#pragma once

#include "engine.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace chess::detail {

constexpr const char* kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr int kMateScore = 100000;
constexpr int kMaxQuiescencePly = 8;

inline bool on_board(int square) {
  return square >= 0 && square < 64;
}

inline int file_of(int square) {
  return square % 8;
}

inline int rank_of(int square) {
  return square / 8;
}

inline bool is_white_piece(char piece) {
  return piece >= 'A' && piece <= 'Z';
}

inline bool is_black_piece(char piece) {
  return piece >= 'a' && piece <= 'z';
}

inline bool same_color(char piece, bool white) {
  return piece != '.' && (white ? is_white_piece(piece) : is_black_piece(piece));
}

inline bool enemy_color(char piece, bool white) {
  return piece != '.' && (white ? is_black_piece(piece) : is_white_piece(piece));
}

inline char normalize_promotion(char promotion, bool white) {
  char p = static_cast<char>(std::tolower(static_cast<unsigned char>(promotion)));
  if (p != 'q' && p != 'r' && p != 'b' && p != 'n') {
    p = 'q';
  }
  return white ? static_cast<char>(std::toupper(static_cast<unsigned char>(p))) : p;
}

inline int piece_value(char piece) {
  switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return 100;
    case 'n': return 320;
    case 'b': return 330;
    case 'r': return 500;
    case 'q': return 900;
    case 'k': return 0;
    default: return 0;
  }
}

inline std::string square_name(int square) {
  std::string out;
  out.push_back(static_cast<char>('a' + file_of(square)));
  out.push_back(static_cast<char>('1' + rank_of(square)));
  return out;
}

inline int parse_square(const std::string& text, size_t offset) {
  if (offset + 1 >= text.size()) {
    return -1;
  }
  char file = text[offset];
  char rank = text[offset + 1];
  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
    return -1;
  }
  return (rank - '1') * 8 + (file - 'a');
}

inline void add_move(MoveList& moves, int from, int to, char promotion = 0) {
  moves.push_back(Move{from, to, promotion});
}

inline void add_promotions(MoveList& moves, int from, int to) {
  add_move(moves, from, to, 'q');
  add_move(moves, from, to, 'r');
  add_move(moves, from, to, 'b');
  add_move(moves, from, to, 'n');
}

inline void remember_square(Board::UndoState& undo, int square, char piece) {
  for (int i = 0; i < undo.square_count; ++i) {
    if (undo.squares[i] == square) {
      return;
    }
  }
  undo.squares[undo.square_count] = square;
  undo.pieces[undo.square_count] = piece;
  undo.square_count++;
}

inline uint64_t splitmix64(uint64_t value) {
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

inline int piece_index(char piece) {
  switch (piece) {
    case 'P': return 0;
    case 'N': return 1;
    case 'B': return 2;
    case 'R': return 3;
    case 'Q': return 4;
    case 'K': return 5;
    case 'p': return 6;
    case 'n': return 7;
    case 'b': return 8;
    case 'r': return 9;
    case 'q': return 10;
    case 'k': return 11;
    default: return -1;
  }
}

inline bool is_valid_move_square(int square) {
  return square >= 0 && square < 64;
}

std::optional<Move> opening_book_move(const Board& board);
void order_moves(const Board& board, MoveList& moves, const Move* hash_move = nullptr);
bool is_capture_or_promotion(const Board& board, const Move& move);
uint64_t perft_mutable(Board& board, int depth);

} // namespace chess::detail
