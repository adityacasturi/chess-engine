#include "engine_internal.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <sstream>
#include <utility>

namespace chess {

Board Board::starting_position() {
  Board board;
  board.load_fen(detail::kStartFen);
  return board;
}

bool Board::load_fen(const std::string& fen) {
  std::istringstream input(fen);
  std::string placement;
  std::string side;
  std::string castling;
  std::string ep;
  if (!(input >> placement >> side >> castling >> ep >> halfmove_clock_ >> fullmove_number_)) {
    return false;
  }

  std::fill(std::begin(squares_), std::end(squares_), '.');
  white_king_square_ = -1;
  black_king_square_ = -1;
  int rank = 7;
  int file = 0;
  for (char ch : placement) {
    if (ch == '/') {
      if (file != 8) return false;
      file = 0;
      rank--;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      file += ch - '0';
      if (file > 8) return false;
      continue;
    }
    if (std::string("pnbrqkPNBRQK").find(ch) == std::string::npos) {
      return false;
    }
    if (rank < 0 || file >= 8) return false;
    int square = rank * 8 + file;
    squares_[square] = ch;
    if (ch == 'K') white_king_square_ = square;
    if (ch == 'k') black_king_square_ = square;
    file++;
  }
  if (rank != 0 || file != 8) return false;

  white_to_move_ = side == "w";
  if (!white_to_move_ && side != "b") return false;

  castle_white_king_ = castling.find('K') != std::string::npos;
  castle_white_queen_ = castling.find('Q') != std::string::npos;
  castle_black_king_ = castling.find('k') != std::string::npos;
  castle_black_queen_ = castling.find('q') != std::string::npos;

  en_passant_ = -1;
  if (ep != "-") {
    en_passant_ = detail::parse_square(ep, 0);
    if (en_passant_ == -1) return false;
  }

  return white_king_square_ != -1 && black_king_square_ != -1;
}

std::string Board::fen() const {
  std::ostringstream out;
  for (int rank = 7; rank >= 0; --rank) {
    int empty = 0;
    for (int file = 0; file < 8; ++file) {
      char piece = squares_[rank * 8 + file];
      if (piece == '.') {
        empty++;
      } else {
        if (empty > 0) {
          out << empty;
          empty = 0;
        }
        out << piece;
      }
    }
    if (empty > 0) out << empty;
    if (rank > 0) out << '/';
  }

  out << (white_to_move_ ? " w " : " b ");
  std::string castling;
  if (castle_white_king_) castling += 'K';
  if (castle_white_queen_) castling += 'Q';
  if (castle_black_king_) castling += 'k';
  if (castle_black_queen_) castling += 'q';
  out << (castling.empty() ? "-" : castling) << ' ';
  out << (en_passant_ == -1 ? "-" : detail::square_name(en_passant_)) << ' ';
  out << halfmove_clock_ << ' ' << fullmove_number_;
  return out.str();
}

bool Board::white_to_move() const {
  return white_to_move_;
}

char Board::piece_at(int square) const {
  return detail::on_board(square) ? squares_[square] : '.';
}

int Board::en_passant_square() const {
  return en_passant_;
}

uint64_t Board::hash_key() const {
  uint64_t key = 0;
  for (int square = 0; square < 64; ++square) {
    int index = detail::piece_index(squares_[square]);
    if (index >= 0) {
      key ^= detail::splitmix64(static_cast<uint64_t>(index * 64 + square));
    }
  }
  if (white_to_move_) key ^= detail::splitmix64(768);
  if (castle_white_king_) key ^= detail::splitmix64(769);
  if (castle_white_queen_) key ^= detail::splitmix64(770);
  if (castle_black_king_) key ^= detail::splitmix64(771);
  if (castle_black_queen_) key ^= detail::splitmix64(772);
  if (en_passant_ != -1) key ^= detail::splitmix64(773 + detail::file_of(en_passant_));
  return key;
}

std::vector<Move> Board::pseudo_legal_moves() const {
  std::vector<Move> moves;
  moves.reserve(64);
  bool white = white_to_move_;

  for (int from = 0; from < 64; ++from) {
    char piece = squares_[from];
    if (!detail::same_color(piece, white)) continue;

    int file = detail::file_of(from);
    int rank = detail::rank_of(from);
    char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));

    if (kind == 'p') {
      int forward = white ? 8 : -8;
      int start_rank = white ? 1 : 6;
      int promotion_rank = white ? 6 : 1;
      int one = from + forward;
      if (detail::on_board(one) && squares_[one] == '.') {
        if (rank == promotion_rank) {
          detail::add_promotions(moves, from, one);
        } else {
          detail::add_move(moves, from, one);
          int two = from + 2 * forward;
          if (rank == start_rank && detail::on_board(two) && squares_[two] == '.') {
            detail::add_move(moves, from, two);
          }
        }
      }

      const std::array<int, 2> capture_offsets = white ? std::array<int, 2>{7, 9}
                                                       : std::array<int, 2>{-9, -7};
      for (int offset : capture_offsets) {
        int to = from + offset;
        if (!detail::on_board(to) || std::abs(detail::file_of(to) - file) != 1) continue;
        if (detail::enemy_color(squares_[to], white) &&
            std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
          if (rank == promotion_rank) {
            detail::add_promotions(moves, from, to);
          } else {
            detail::add_move(moves, from, to);
          }
        } else if (to == en_passant_) {
          detail::add_move(moves, from, to);
        }
      }
      continue;
    }

    if (kind == 'n') {
      const std::array<std::pair<int, int>, 8> jumps = {{{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                                        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
      for (auto [df, dr] : jumps) {
        int nf = file + df;
        int nr = rank + dr;
        if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
        int to = nr * 8 + nf;
        if (!detail::same_color(squares_[to], white) &&
            std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
          detail::add_move(moves, from, to);
        }
      }
      continue;
    }

    if (kind == 'b' || kind == 'r' || kind == 'q') {
      constexpr std::array<std::pair<int, int>, 4> bishop_dirs = {
          std::pair<int, int>{1, 1}, std::pair<int, int>{1, -1},
          std::pair<int, int>{-1, 1}, std::pair<int, int>{-1, -1}};
      constexpr std::array<std::pair<int, int>, 4> rook_dirs = {
          std::pair<int, int>{1, 0}, std::pair<int, int>{-1, 0},
          std::pair<int, int>{0, 1}, std::pair<int, int>{0, -1}};
      auto add_sliding = [&](const auto& dirs) {
        for (auto [df, dr] : dirs) {
          int nf = file + df;
          int nr = rank + dr;
          while (nf >= 0 && nf <= 7 && nr >= 0 && nr <= 7) {
            int to = nr * 8 + nf;
            if (detail::same_color(squares_[to], white)) break;
            if (std::tolower(static_cast<unsigned char>(squares_[to])) == 'k') break;
            detail::add_move(moves, from, to);
            if (detail::enemy_color(squares_[to], white)) break;
            nf += df;
            nr += dr;
          }
        }
      };
      if (kind == 'b' || kind == 'q') add_sliding(bishop_dirs);
      if (kind == 'r' || kind == 'q') add_sliding(rook_dirs);
      continue;
    }

    if (kind == 'k') {
      for (int df = -1; df <= 1; ++df) {
        for (int dr = -1; dr <= 1; ++dr) {
          if (df == 0 && dr == 0) continue;
          int nf = file + df;
          int nr = rank + dr;
          if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
          int to = nr * 8 + nf;
          if (!detail::same_color(squares_[to], white) &&
              std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
            detail::add_move(moves, from, to);
          }
        }
      }

      if (white && from == 4 && !in_check(true)) {
        if (castle_white_king_ && squares_[5] == '.' && squares_[6] == '.' && squares_[7] == 'R' &&
            !square_attacked(5, false) && !square_attacked(6, false)) {
          detail::add_move(moves, 4, 6);
        }
        if (castle_white_queen_ && squares_[3] == '.' && squares_[2] == '.' && squares_[1] == '.' &&
            squares_[0] == 'R' && !square_attacked(3, false) && !square_attacked(2, false)) {
          detail::add_move(moves, 4, 2);
        }
      }
      if (!white && from == 60 && !in_check(false)) {
        if (castle_black_king_ && squares_[61] == '.' && squares_[62] == '.' && squares_[63] == 'r' &&
            !square_attacked(61, true) && !square_attacked(62, true)) {
          detail::add_move(moves, 60, 62);
        }
        if (castle_black_queen_ && squares_[59] == '.' && squares_[58] == '.' && squares_[57] == '.' &&
            squares_[56] == 'r' && !square_attacked(59, true) && !square_attacked(58, true)) {
          detail::add_move(moves, 60, 58);
        }
      }
    }
  }

  return moves;
}

std::vector<Move> Board::pseudo_tactical_moves() const {
  std::vector<Move> moves;
  moves.reserve(32);
  bool white = white_to_move_;

  for (int from = 0; from < 64; ++from) {
    char piece = squares_[from];
    if (!detail::same_color(piece, white)) continue;

    int file = detail::file_of(from);
    int rank = detail::rank_of(from);
    char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));

    if (kind == 'p') {
      int forward = white ? 8 : -8;
      int promotion_rank = white ? 6 : 1;
      int one = from + forward;
      if (rank == promotion_rank && detail::on_board(one) && squares_[one] == '.') {
        detail::add_promotions(moves, from, one);
      }

      const std::array<int, 2> capture_offsets = white ? std::array<int, 2>{7, 9}
                                                       : std::array<int, 2>{-9, -7};
      for (int offset : capture_offsets) {
        int to = from + offset;
        if (!detail::on_board(to) || std::abs(detail::file_of(to) - file) != 1) continue;
        if (detail::enemy_color(squares_[to], white) &&
            std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
          if (rank == promotion_rank) {
            detail::add_promotions(moves, from, to);
          } else {
            detail::add_move(moves, from, to);
          }
        } else if (to == en_passant_) {
          detail::add_move(moves, from, to);
        }
      }
      continue;
    }

    if (kind == 'n') {
      const std::array<std::pair<int, int>, 8> jumps = {{{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                                        {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
      for (auto [df, dr] : jumps) {
        int nf = file + df;
        int nr = rank + dr;
        if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
        int to = nr * 8 + nf;
        if (detail::enemy_color(squares_[to], white) &&
            std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
          detail::add_move(moves, from, to);
        }
      }
      continue;
    }

    if (kind == 'b' || kind == 'r' || kind == 'q') {
      constexpr std::array<std::pair<int, int>, 4> bishop_dirs = {
          std::pair<int, int>{1, 1}, std::pair<int, int>{1, -1},
          std::pair<int, int>{-1, 1}, std::pair<int, int>{-1, -1}};
      constexpr std::array<std::pair<int, int>, 4> rook_dirs = {
          std::pair<int, int>{1, 0}, std::pair<int, int>{-1, 0},
          std::pair<int, int>{0, 1}, std::pair<int, int>{0, -1}};
      auto add_sliding_captures = [&](const auto& dirs) {
        for (auto [df, dr] : dirs) {
          int nf = file + df;
          int nr = rank + dr;
          while (nf >= 0 && nf <= 7 && nr >= 0 && nr <= 7) {
            int to = nr * 8 + nf;
            if (squares_[to] != '.') {
              if (detail::enemy_color(squares_[to], white) &&
                  std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
                detail::add_move(moves, from, to);
              }
              break;
            }
            nf += df;
            nr += dr;
          }
        }
      };
      if (kind == 'b' || kind == 'q') add_sliding_captures(bishop_dirs);
      if (kind == 'r' || kind == 'q') add_sliding_captures(rook_dirs);
      continue;
    }

    if (kind == 'k') {
      for (int df = -1; df <= 1; ++df) {
        for (int dr = -1; dr <= 1; ++dr) {
          if (df == 0 && dr == 0) continue;
          int nf = file + df;
          int nr = rank + dr;
          if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
          int to = nr * 8 + nf;
          if (detail::enemy_color(squares_[to], white) &&
              std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') {
            detail::add_move(moves, from, to);
          }
        }
      }
    }
  }

  return moves;
}

std::vector<Move> Board::legal_moves() const {
  Board copy = *this;
  return copy.legal_moves_fast();
}

std::vector<Move> Board::legal_moves_fast() {
  std::vector<Move> legal;
  legal.reserve(64);
  bool us = white_to_move_;
  for (const auto& move : pseudo_legal_moves()) {
    UndoState undo;
    bool made = make_move_internal(move, &undo);
    if (made && !in_check(us)) {
      legal.push_back(move);
    }
    if (made) {
      unmake_move(undo);
    }
  }
  return legal;
}

std::vector<Move> Board::tactical_moves_fast() {
  std::vector<Move> legal;
  legal.reserve(32);
  bool us = white_to_move_;
  for (const auto& move : pseudo_tactical_moves()) {
    UndoState undo;
    bool made = make_move_internal(move, &undo);
    if (made && !in_check(us)) {
      legal.push_back(move);
    }
    if (made) {
      unmake_move(undo);
    }
  }
  return legal;
}

bool Board::is_legal(const Move& move) const {
  auto moves = legal_moves();
  return std::find(moves.begin(), moves.end(), move) != moves.end();
}

bool Board::make_move(const Move& move) {
  return make_move_internal(move, nullptr);
}

bool Board::make_move_internal(const Move& move, UndoState* undo) {
  if (!detail::on_board(move.from) || !detail::on_board(move.to)) return false;
  char piece = squares_[move.from];
  if (piece == '.' || !detail::same_color(piece, white_to_move_)) return false;

  bool moving_white = white_to_move_;
  char captured = squares_[move.to];
  char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));
  bool pawn_move = kind == 'p';
  bool capture = captured != '.';

  UndoState local_undo;
  UndoState& state = undo ? *undo : local_undo;
  state.white_to_move = white_to_move_;
  state.castle_white_king = castle_white_king_;
  state.castle_white_queen = castle_white_queen_;
  state.castle_black_king = castle_black_king_;
  state.castle_black_queen = castle_black_queen_;
  state.en_passant = en_passant_;
  state.halfmove_clock = halfmove_clock_;
  state.fullmove_number = fullmove_number_;
  state.white_king_square = white_king_square_;
  state.black_king_square = black_king_square_;
  state.square_count = 0;
  detail::remember_square(state, move.from, squares_[move.from]);
  detail::remember_square(state, move.to, squares_[move.to]);

  squares_[move.from] = '.';

  if (pawn_move && move.to == en_passant_ && captured == '.' &&
      std::abs(detail::file_of(move.to) - detail::file_of(move.from)) == 1) {
    int captured_square = moving_white ? move.to - 8 : move.to + 8;
    detail::remember_square(state, captured_square, squares_[captured_square]);
    squares_[captured_square] = '.';
    capture = true;
  }

  if (kind == 'k' && std::abs(static_cast<int>(move.to) - static_cast<int>(move.from)) == 2) {
    if (move.to == 6) {
      detail::remember_square(state, 5, squares_[5]);
      detail::remember_square(state, 7, squares_[7]);
      squares_[5] = squares_[7];
      squares_[7] = '.';
    } else if (move.to == 2) {
      detail::remember_square(state, 3, squares_[3]);
      detail::remember_square(state, 0, squares_[0]);
      squares_[3] = squares_[0];
      squares_[0] = '.';
    } else if (move.to == 62) {
      detail::remember_square(state, 61, squares_[61]);
      detail::remember_square(state, 63, squares_[63]);
      squares_[61] = squares_[63];
      squares_[63] = '.';
    } else if (move.to == 58) {
      detail::remember_square(state, 59, squares_[59]);
      detail::remember_square(state, 56, squares_[56]);
      squares_[59] = squares_[56];
      squares_[56] = '.';
    }
  }

  char placed = piece;
  if (pawn_move && (detail::rank_of(move.to) == 7 || detail::rank_of(move.to) == 0)) {
    placed = detail::normalize_promotion(move.promotion, moving_white);
  }
  squares_[move.to] = placed;
  if (placed == 'K') white_king_square_ = move.to;
  if (placed == 'k') black_king_square_ = move.to;

  if (kind == 'k') {
    if (moving_white) {
      castle_white_king_ = false;
      castle_white_queen_ = false;
    } else {
      castle_black_king_ = false;
      castle_black_queen_ = false;
    }
  }
  if (kind == 'r') {
    if (move.from == 0) castle_white_queen_ = false;
    if (move.from == 7) castle_white_king_ = false;
    if (move.from == 56) castle_black_queen_ = false;
    if (move.from == 63) castle_black_king_ = false;
  }
  if (move.to == 0) castle_white_queen_ = false;
  if (move.to == 7) castle_white_king_ = false;
  if (move.to == 56) castle_black_queen_ = false;
  if (move.to == 63) castle_black_king_ = false;

  en_passant_ = -1;
  if (pawn_move && std::abs(static_cast<int>(move.to) - static_cast<int>(move.from)) == 16) {
    en_passant_ = (move.to + move.from) / 2;
  }

  halfmove_clock_ = (pawn_move || capture) ? 0 : halfmove_clock_ + 1;
  if (!moving_white) {
    fullmove_number_++;
  }
  white_to_move_ = !white_to_move_;
  return true;
}

void Board::unmake_move(const UndoState& undo) {
  white_to_move_ = undo.white_to_move;
  castle_white_king_ = undo.castle_white_king;
  castle_white_queen_ = undo.castle_white_queen;
  castle_black_king_ = undo.castle_black_king;
  castle_black_queen_ = undo.castle_black_queen;
  en_passant_ = undo.en_passant;
  halfmove_clock_ = undo.halfmove_clock;
  fullmove_number_ = undo.fullmove_number;
  white_king_square_ = undo.white_king_square;
  black_king_square_ = undo.black_king_square;
  for (int i = 0; i < undo.square_count; ++i) {
    squares_[undo.squares[i]] = undo.pieces[i];
  }
}

int Board::king_square(bool white) const {
  return white ? white_king_square_ : black_king_square_;
}

bool Board::square_attacked(int square, bool by_white) const {
  int file = detail::file_of(square);
  if (by_white) {
    if (file < 7 && detail::on_board(square - 7) && squares_[square - 7] == 'P') return true;
    if (file > 0 && detail::on_board(square - 9) && squares_[square - 9] == 'P') return true;
  } else {
    if (file > 0 && detail::on_board(square + 7) && squares_[square + 7] == 'p') return true;
    if (file < 7 && detail::on_board(square + 9) && squares_[square + 9] == 'p') return true;
  }

  const std::array<std::pair<int, int>, 8> knights = {{{1, 2}, {2, 1}, {2, -1}, {1, -2},
                                                      {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
  for (auto [df, dr] : knights) {
    int nf = file + df;
    int nr = detail::rank_of(square) + dr;
    if (nf < 0 || nf > 7 || nr < 0 || nr > 7) continue;
    char p = squares_[nr * 8 + nf];
    if (p == (by_white ? 'N' : 'n')) return true;
  }

  const std::array<std::pair<int, int>, 8> dirs = {{{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                                   {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}};
  for (auto [df, dr] : dirs) {
    int nf = file + df;
    int nr = detail::rank_of(square) + dr;
    int distance = 1;
    while (nf >= 0 && nf <= 7 && nr >= 0 && nr <= 7) {
      char p = squares_[nr * 8 + nf];
      if (p != '.') {
        if (detail::same_color(p, by_white)) {
          char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(p)));
          bool diagonal = df != 0 && dr != 0;
          bool straight = df == 0 || dr == 0;
          if (distance == 1 && kind == 'k') return true;
          if (diagonal && (kind == 'b' || kind == 'q')) return true;
          if (straight && (kind == 'r' || kind == 'q')) return true;
        }
        break;
      }
      nf += df;
      nr += dr;
      distance++;
    }
  }

  return false;
}

bool Board::in_check(bool white) const {
  int king = king_square(white);
  return king == -1 || square_attacked(king, !white);
}

std::string Board::status() const {
  auto moves = legal_moves();
  if (moves.empty()) {
    return in_check(white_to_move_) ? "checkmate" : "stalemate";
  }
  if (halfmove_clock_ >= 100) {
    return "draw";
  }
  return in_check(white_to_move_) ? "check" : "ongoing";
}

} // namespace chess
