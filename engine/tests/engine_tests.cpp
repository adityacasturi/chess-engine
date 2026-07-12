#include "engine.hpp"

#include <cassert>
#include <iostream>

namespace {

chess::Board from_fen(const std::string& fen) {
  chess::Board board;
  assert(board.load_fen(fen));
  return board;
}

void test_fen_round_trip() {
  auto board = chess::Board::starting_position();
  assert(board.fen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void test_perft() {
  auto start = chess::Board::starting_position();
  assert(chess::perft(start, 1) == 20);
  assert(chess::perft(start, 2) == 400);

  auto kiwipete = from_fen(
      "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  assert(chess::perft(kiwipete, 1) == 48);
  assert(chess::perft(kiwipete, 2) == 2039);
}

void test_special_moves() {
  auto castles = from_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  assert(castles.is_legal(*chess::parse_uci_move("e1g1")));
  assert(castles.is_legal(*chess::parse_uci_move("e1c1")));

  auto ep = from_fen("8/8/8/3pP3/8/8/8/4K2k w - d6 0 1");
  assert(ep.is_legal(*chess::parse_uci_move("e5d6")));

  auto promo = from_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
  assert(promo.is_legal(*chess::parse_uci_move("a7a8q")));
  assert(promo.is_legal(*chess::parse_uci_move("a7a8n")));
}

void test_status_and_search() {
  auto mate = from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
  assert(mate.status() == "checkmate");

  auto stalemate = from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
  assert(stalemate.status() == "stalemate");

  auto board = chess::Board::starting_position();
  auto result = chess::search_best_move(board, 1);
  assert(chess::move_to_uci(result.move) != "0000");
  assert(board.is_legal(result.move));
}

void test_make_move_updates_fen() {
  auto board = chess::Board::starting_position();
  assert(board.make_move(*chess::parse_uci_move("e2e4")));
  assert(board.fen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
}

void test_make_unmake_restores_state() {
  auto board = from_fen("r3k2r/8/8/3pP3/8/8/8/R3K2R w KQkq d6 0 1");
  std::string start = board.fen();
  uint64_t start_hash = board.hash_key();

  for (const std::string& move_text : {"e5d6", "e1g1", "e1c1"}) {
    auto move = chess::parse_uci_move(move_text);
    chess::Board::UndoState undo;
    assert(move);
    assert(board.make_move_internal(*move, &undo));
    board.unmake_move(undo);
    assert(board.fen() == start);
    assert(board.hash_key() == start_hash);
  }
}

void test_opening_book() {
  auto london = from_fen("rnbqkbnr/ppp1pppp/8/3p4/3P1B2/8/PPP1PPPP/RN1QKBNR b KQkq - 1 2");
  assert(chess::move_to_uci(chess::search_best_move(london, 4).move) == "g8f6");

  auto queens_gambit =
      from_fen("rnbqkbnr/ppp1pppp/8/3p4/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2");
  assert(chess::move_to_uci(chess::search_best_move(queens_gambit, 4).move) == "e7e6");

  auto english = from_fen("rnbqkbnr/pppppppp/8/8/2P5/8/PP1PPPPP/RNBQKBNR b KQkq c3 0 1");
  assert(chess::move_to_uci(chess::search_best_move(english, 4).move) == "e7e5");
}

void test_positional_evaluation() {
  auto centered_knight = from_fen("4k3/8/8/8/3N4/8/8/4K3 w - - 0 1");
  auto rim_knight = from_fen("4k3/8/8/8/N7/8/8/4K3 w - - 0 1");
  assert(centered_knight.evaluate_white() > rim_knight.evaluate_white());

  auto bishop_pair = from_fen("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1");
  auto one_bishop = from_fen("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1");
  assert(bishop_pair.evaluate_white() - one_bishop.evaluate_white() > 300);
}

} // namespace

int main() {
  test_fen_round_trip();
  test_perft();
  test_special_moves();
  test_status_and_search();
  test_make_move_updates_fen();
  test_make_unmake_restores_state();
  test_opening_book();
  test_positional_evaluation();
  std::cout << "engine tests passed\n";
  return 0;
}
