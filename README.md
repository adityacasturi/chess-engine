# Custom Chess Engine

A playable chess app with a React frontend and a native C++ engine. The public site compiles that engine to WebAssembly and runs it in the browser, so you can play without a server.

Play here: https://adityacasturi.github.io/chess-engine/

Choose search depths 1–7 using the depth slider. Depths 6 and 7 search further and can take longer, especially in complex positions. The browser engine runs in a Web Worker so the page stays responsive during search.

Locally, the browser still uses `chess.js` for instant human move legality, move highlighting, and board updates. `npm run dev` can fall back to the Express bridge and native binary if the WASM build is missing.

## Setup

```sh
npm install
npm run build:engine
npm run dev
```

The client runs on `http://127.0.0.1:3000` and the API server runs on `http://127.0.0.1:3001`.

## Engine

The engine supports the minimal UCI commands:

```txt
uci
isready
position fen <fen>
go depth <n>
bestmove <move>
quit
```

The server uses `POST /api/engine-move` and a custom engine command:

```txt
best depth <n> fen <fen>
```

That command checks a small built-in opening book first, then searches the given FEN with alpha-beta negamax, applies the engine move, and returns JSON with the engine move, final FEN, status, score, node count, and search time.

The current book includes early replies for common human openings including the London System, Queen's Gambit, English Opening, Reti/Nf3 setups, and basic e4 open games. Outside the book, evaluation combines material, piece-square tables, mobility, bishop pair, pawn structure, passed pawns, and castled king safety.

Current search/performance features:

- make/unmake search instead of copying boards at every node
- move ordering for hash moves, captures, promotions, castling, and centralization
- transposition table keyed by a deterministic 64-bit board hash
- compact byte-sized move representation
- incremental king-square state for faster check detection
- quiescence search over captures/promotions at leaf nodes

## Benchmarks

Run engine throughput metrics with:

```sh
npm run bench:engine
```

This builds the C++ engine in Release mode and prints a full telemetry report:

- root position complexity: legal moves, captures, promotions, checking moves, and static eval
- perft correctness and move-generation throughput against known reference node counts
- alpha-beta search throughput across opening, middlegame, tactical, endgame, and black-to-move positions
- quiescence-search overhead, alpha-beta cutoff counts, transposition-table hit rate, effective branching factor, and depth reached
- a depth-6 stress sample with total positions searched, elapsed time, candidate moves generated, TT cutoffs/stores, and max ply
- opening-book probes, where `book hit: yes` and `nodes: 0` means the move came directly from book
- digestible headline metrics for resume bullets

Recent local baseline:

- perft correctness: all known reference cases passed
- move generation peak: roughly `21.3M legal positions/sec`
- move generation aggregate: roughly `20.1M legal positions/sec` across `9.9M` generated positions
- alpha-beta search aggregate: roughly `1.35M positions/sec` across `2.3M` searched positions
- depth-6 stress sample: roughly `1.07M positions searched in 0.74s`
- pruning telemetry: roughly `330K alpha-beta cutoffs`, `2.1%` transposition-table hit rate, and `60%` quiescence-node share
