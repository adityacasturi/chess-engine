#include "engine_internal.hpp"

#include <array>
#include <utility>

namespace chess {
namespace {

constexpr std::array<int, 64> kPawnTable = {
    0, 0, 0, 0, 0, 0, 0, 0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5, 5, 10, 28, 28, 10, 5, 5,
    0, 0, 0, 22, 22, 0, 0, 0,
    5, -5, -10, 0, 0, -10, -5, 5,
    5, 10, 10, -25, -25, 10, 10, 5,
    0, 0, 0, 0, 0, 0, 0, 0};

constexpr std::array<int, 64> kKnightTable = {
    -50, -40, -30, -30, -30, -30, -40, -50,
    -40, -20, 0, 5, 5, 0, -20, -40,
    -30, 5, 10, 15, 15, 10, 5, -30,
    -30, 0, 15, 20, 20, 15, 0, -30,
    -30, 5, 15, 20, 20, 15, 5, -30,
    -30, 0, 10, 15, 15, 10, 0, -30,
    -40, -20, 0, 0, 0, 0, -20, -40,
    -50, -40, -30, -30, -30, -30, -40, -50};

constexpr std::array<int, 64> kBishopTable = {
    -20, -10, -10, -10, -10, -10, -10, -20,
    -10, 5, 0, 0, 0, 0, 5, -10,
    -10, 10, 10, 10, 10, 10, 10, -10,
    -10, 0, 10, 10, 10, 10, 0, -10,
    -10, 5, 5, 10, 10, 5, 5, -10,
    -10, 0, 5, 10, 10, 5, 0, -10,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -20, -10, -10, -10, -10, -10, -10, -20};

constexpr std::array<int, 64> kRookTable = {
    0, 0, 5, 10, 10, 5, 0, 0,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    -5, 0, 0, 0, 0, 0, 0, -5,
    5, 10, 10, 10, 10, 10, 10, 5,
    0, 0, 0, 5, 5, 0, 0, 0};

constexpr std::array<int, 64> kQueenTable = {
    -20, -10, -10, -5, -5, -10, -10, -20,
    -10, 0, 5, 0, 0, 0, 0, -10,
    -10, 5, 5, 5, 5, 5, 0, -10,
    0, 0, 5, 5, 5, 5, 0, -5,
    -5, 0, 5, 5, 5, 5, 0, -5,
    -10, 0, 5, 5, 5, 5, 0, -10,
    -10, 0, 0, 0, 0, 0, 0, -10,
    -20, -10, -10, -5, -5, -10, -10, -20};

constexpr std::array<int, 64> kKingTable = {
    20, 30, 10, 0, 0, 10, 30, 20,
    20, 20, 0, 0, 0, 0, 20, 20,
    -10, -20, -20, -20, -20, -20, -20, -10,
    -20, -30, -30, -40, -40, -30, -30, -20,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30};

int piece_square_value(char piece, int square) {
  bool white = detail::is_white_piece(piece);
  int index = white ? square : (square ^ 56);
  switch (std::tolower(static_cast<unsigned char>(piece))) {
    case 'p': return kPawnTable[index];
    case 'n': return kKnightTable[index];
    case 'b': return kBishopTable[index];
    case 'r': return kRookTable[index];
    case 'q': return kQueenTable[index];
    case 'k': return kKingTable[index];
    default: return 0;
  }
}

} // namespace

int Board::pseudo_mobility(bool white) const {
  int mobility = 0;

  for (int from = 0; from < 64; ++from) {
    char piece = squares_[from];
    if (!detail::same_color(piece, white)) continue;

    int file = detail::file_of(from);
    int rank = detail::rank_of(from);
    char kind = static_cast<char>(std::tolower(static_cast<unsigned char>(piece)));

    if (kind == 'p') {
      int forward = white ? 8 : -8;
      int start_rank = white ? 1 : 6;
      int one = from + forward;
      if (detail::on_board(one) && squares_[one] == '.') {
        mobility++;
        int two = from + 2 * forward;
        if (rank == start_rank && detail::on_board(two) && squares_[two] == '.') {
          mobility++;
        }
      }

      const std::array<int, 2> capture_offsets = white ? std::array<int, 2>{7, 9}
                                                       : std::array<int, 2>{-9, -7};
      for (int offset : capture_offsets) {
        int to = from + offset;
        if (!detail::on_board(to) || std::abs(detail::file_of(to) - file) != 1) continue;
        if ((detail::enemy_color(squares_[to], white) &&
             std::tolower(static_cast<unsigned char>(squares_[to])) != 'k') ||
            to == en_passant_) {
          mobility++;
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
          mobility++;
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
      auto count_sliding = [&](const auto& dirs) {
        for (auto [df, dr] : dirs) {
          int nf = file + df;
          int nr = rank + dr;
          while (nf >= 0 && nf <= 7 && nr >= 0 && nr <= 7) {
            int to = nr * 8 + nf;
            if (detail::same_color(squares_[to], white)) break;
            if (std::tolower(static_cast<unsigned char>(squares_[to])) == 'k') break;
            mobility++;
            if (detail::enemy_color(squares_[to], white)) break;
            nf += df;
            nr += dr;
          }
        }
      };
      if (kind == 'b' || kind == 'q') count_sliding(bishop_dirs);
      if (kind == 'r' || kind == 'q') count_sliding(rook_dirs);
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
            mobility++;
          }
        }
      }
    }
  }

  return mobility;
}

int Board::evaluate_white() const {
  int score = 0;
  std::array<int, 8> white_pawns_by_file = {};
  std::array<int, 8> black_pawns_by_file = {};
  int white_bishops = 0;
  int black_bishops = 0;

  for (int square = 0; square < 64; ++square) {
    char piece = squares_[square];
    if (piece == '.') continue;
    int value = detail::piece_value(piece) + piece_square_value(piece, square);
    if (detail::is_white_piece(piece)) {
      score += value;
      if (piece == 'P') white_pawns_by_file[detail::file_of(square)]++;
      if (piece == 'B') white_bishops++;
    } else {
      score -= value;
      if (piece == 'p') black_pawns_by_file[detail::file_of(square)]++;
      if (piece == 'b') black_bishops++;
    }
  }

  if (white_bishops >= 2) score += 35;
  if (black_bishops >= 2) score -= 35;

  for (int file = 0; file < 8; ++file) {
    if (white_pawns_by_file[file] > 1) score -= 15 * (white_pawns_by_file[file] - 1);
    if (black_pawns_by_file[file] > 1) score += 15 * (black_pawns_by_file[file] - 1);

    bool white_isolated = white_pawns_by_file[file] > 0 &&
                          (file == 0 || white_pawns_by_file[file - 1] == 0) &&
                          (file == 7 || white_pawns_by_file[file + 1] == 0);
    bool black_isolated = black_pawns_by_file[file] > 0 &&
                          (file == 0 || black_pawns_by_file[file - 1] == 0) &&
                          (file == 7 || black_pawns_by_file[file + 1] == 0);
    if (white_isolated) score -= 12 * white_pawns_by_file[file];
    if (black_isolated) score += 12 * black_pawns_by_file[file];
  }

  for (int square = 0; square < 64; ++square) {
    char piece = squares_[square];
    if (piece != 'P' && piece != 'p') continue;
    bool white = piece == 'P';
    int file = detail::file_of(square);
    int rank = detail::rank_of(square);
    bool blocked_by_enemy_pawn = false;
    for (int df = -1; df <= 1; ++df) {
      int pawn_file = file + df;
      if (pawn_file < 0 || pawn_file > 7) continue;
      for (int pawn_rank = white ? rank + 1 : rank - 1;
           white ? pawn_rank <= 7 : pawn_rank >= 0;
           white ? ++pawn_rank : --pawn_rank) {
        char ahead = squares_[pawn_rank * 8 + pawn_file];
        if (white ? ahead == 'p' : ahead == 'P') {
          blocked_by_enemy_pawn = true;
        }
      }
    }
    if (!blocked_by_enemy_pawn) {
      int bonus = 12 + (white ? rank : 7 - rank) * 6;
      score += white ? bonus : -bonus;
    }
  }

  score += (pseudo_mobility(true) - pseudo_mobility(false)) * 2;

  int white_king = king_square(true);
  int black_king = king_square(false);
  if (white_king == 6 || white_king == 2) score += 25;
  if (black_king == 62 || black_king == 58) score -= 25;

  return score;
}

} // namespace chess
