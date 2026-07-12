#include "engine_internal.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <unordered_map>
#include <utility>

namespace chess::detail {
namespace {

int captured_piece_value(const Board& board, const Move& move) {
  if (!is_valid_move_square(move.to)) return 0;
  char captured = board.piece_at(move.to);
  if (captured == '.' && move.to == board.en_passant_square()) {
    char mover = board.piece_at(move.from);
    if (std::tolower(static_cast<unsigned char>(mover)) == 'p') {
      captured = is_white_piece(mover) ? 'p' : 'P';
    }
  }
  return piece_value(captured);
}

int move_order_score(const Board& board, const Move& move, const Move* hash_move = nullptr) {
  if (hash_move && move == *hash_move) {
    return 2'000'000;
  }

  int score = 0;
  char mover = is_valid_move_square(move.from) ? board.piece_at(move.from) : '.';
  int captured = captured_piece_value(board, move);
  if (captured > 0) {
    score += 1'000'000 + captured * 10 - piece_value(mover);
  }
  if (move.promotion) {
    score += 800'000 + piece_value(move.promotion);
  }
  if (std::tolower(static_cast<unsigned char>(mover)) == 'k' &&
      std::abs(static_cast<int>(move.to) - static_cast<int>(move.from)) == 2) {
    score += 50'000;
  }
  int center_distance = std::abs(file_of(move.to) - 3) + std::abs(rank_of(move.to) - 3);
  score += 20 - center_distance;
  return score;
}

enum class TTFlag : uint8_t {
  Exact,
  LowerBound,
  UpperBound,
};

struct TTEntry {
  uint64_t key = 0;
  int depth = -1;
  int score = 0;
  TTFlag flag = TTFlag::Exact;
  Move best_move;
};

using TranspositionTable = std::unordered_map<uint64_t, TTEntry>;

int quiescence(Board& board, int ply, int alpha, int beta, SearchStats& stats) {
  stats.quiescence_nodes++;
  stats.max_quiescence_ply = std::max(stats.max_quiescence_ply, ply);
  stats.static_evaluations++;
  int stand_pat = board.white_to_move() ? board.evaluate_white() : -board.evaluate_white();
  if (stand_pat >= beta) return beta;
  if (stand_pat > alpha) alpha = stand_pat;
  if (ply >= kMaxQuiescencePly) return alpha;

  auto moves = board.tactical_moves_fast();
  stats.q_moves_generated += moves.size();
  order_moves(board, moves);

  for (const auto& move : moves) {
    stats.q_moves_searched++;
    Board::UndoState undo;
    board.make_move_internal(move, &undo);
    int score = -quiescence(board, ply + 1, -beta, -alpha, stats);
    board.unmake_move(undo);
    if (score >= beta) {
      stats.beta_cutoffs++;
      return beta;
    }
    if (score > alpha) alpha = score;
  }
  return alpha;
}

int negamax(Board& board,
            int depth,
            int ply,
            int alpha,
            int beta,
            SearchStats& stats,
            TranspositionTable& tt) {
  stats.search_nodes++;
  stats.max_ply = std::max(stats.max_ply, ply);
  int original_alpha = alpha;
  uint64_t key = board.hash_key();
  Move hash_move;
  bool has_hash_move = false;
  stats.tt_probes++;
  auto found = tt.find(key);
  if (found != tt.end() && found->second.key == key) {
    stats.tt_hits++;
    const auto& entry = found->second;
    if (entry.depth >= depth) {
      if (entry.flag == TTFlag::Exact) {
        stats.tt_cutoffs++;
        return entry.score;
      }
      if (entry.flag == TTFlag::LowerBound) alpha = std::max(alpha, entry.score);
      if (entry.flag == TTFlag::UpperBound) beta = std::min(beta, entry.score);
      if (alpha >= beta) {
        stats.tt_cutoffs++;
        return entry.score;
      }
    }
    hash_move = entry.best_move;
    has_hash_move = is_valid_move_square(hash_move.from);
  }

  if (depth == 0 && !board.in_check(board.white_to_move())) {
    return quiescence(board, ply, alpha, beta, stats);
  }

  auto moves = board.legal_moves_fast();
  stats.moves_generated += moves.size();
  if (moves.empty()) {
    return board.in_check(board.white_to_move()) ? -kMateScore + ply : 0;
  }
  if (depth == 0) {
    return quiescence(board, ply, alpha, beta, stats);
  }

  order_moves(board, moves, has_hash_move ? &hash_move : nullptr);
  int best = INT_MIN + 1;
  Move best_move = moves.front();
  for (const auto& move : moves) {
    stats.moves_searched++;
    Board::UndoState undo;
    board.make_move_internal(move, &undo);
    int score = -negamax(board, depth - 1, ply + 1, -beta, -alpha, stats, tt);
    board.unmake_move(undo);
    if (score > best) {
      best = score;
      best_move = move;
    }
    alpha = std::max(alpha, score);
    if (alpha >= beta) {
      stats.beta_cutoffs++;
      break;
    }
  }

  TTEntry entry;
  entry.key = key;
  entry.depth = depth;
  entry.score = best;
  entry.best_move = best_move;
  if (best <= original_alpha) {
    entry.flag = TTFlag::UpperBound;
  } else if (best >= beta) {
    entry.flag = TTFlag::LowerBound;
  } else {
    entry.flag = TTFlag::Exact;
  }
  tt[key] = entry;
  stats.tt_stores++;
  return best;
}

} // namespace

bool is_capture_or_promotion(const Board& board, const Move& move) {
  if (move.promotion) return true;
  if (!is_valid_move_square(move.from) || !is_valid_move_square(move.to)) return false;
  char piece = board.piece_at(move.from);
  char target = board.piece_at(move.to);
  if (target != '.') return true;
  return std::tolower(static_cast<unsigned char>(piece)) == 'p' && move.to == board.en_passant_square() &&
         std::abs(file_of(move.to) - file_of(move.from)) == 1;
}

void order_moves(const Board& board, std::vector<Move>& moves, const Move* hash_move) {
  std::vector<std::pair<int, Move>> scored;
  scored.reserve(moves.size());
  for (const auto& move : moves) {
    scored.emplace_back(move_order_score(board, move, hash_move), move);
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    return a.first > b.first;
  });
  for (size_t i = 0; i < scored.size(); ++i) {
    moves[i] = scored[i].second;
  }
}

uint64_t perft_mutable(Board& board, int depth) {
  if (depth == 0) {
    return 1;
  }
  auto moves = board.legal_moves_fast();
  uint64_t nodes = 0;
  for (const auto& move : moves) {
    Board::UndoState undo;
    board.make_move_internal(move, &undo);
    nodes += perft_mutable(board, depth - 1);
    board.unmake_move(undo);
  }
  return nodes;
}

} // namespace chess::detail

namespace chess {

SearchResult search_best_move(const Board& board, int depth) {
  SearchResult result;
  if (auto book_move = detail::opening_book_move(board)) {
    result.move = *book_move;
    result.stats.book_hit = true;
    result.stats.static_evaluations = 1;
    result.score = board.white_to_move() ? board.evaluate_white() : -board.evaluate_white();
    return result;
  }

  Board work = board;
  detail::TranspositionTable tt;
  int search_depth = std::max(1, depth);
  tt.reserve(static_cast<size_t>(1) << std::min(search_depth + 13, 20));
  auto moves = work.legal_moves_fast();
  if (moves.empty()) {
    result.move = Move{-1, -1, 0};
    result.score = work.in_check(work.white_to_move()) ? -detail::kMateScore : 0;
    return result;
  }

  detail::order_moves(work, moves);
  result.stats.moves_generated += moves.size();
  int best = INT_MIN + 1;
  int alpha = INT_MIN + 1;
  int beta = INT_MAX - 1;
  result.move = moves.front();
  for (const auto& move : moves) {
    result.stats.moves_searched++;
    Board::UndoState undo;
    work.make_move_internal(move, &undo);
    int score = -detail::negamax(work, search_depth - 1, 1, -beta, -alpha, result.stats, tt);
    work.unmake_move(undo);
    if (score > best) {
      best = score;
      result.move = move;
    }
    alpha = std::max(alpha, score);
  }
  result.score = best;
  result.nodes = result.stats.search_nodes + result.stats.quiescence_nodes;
  return result;
}

uint64_t perft(const Board& board, int depth) {
  Board work = board;
  return detail::perft_mutable(work, depth);
}

} // namespace chess
