# Chess Engine Source Code Organization

This directory contains the organized source code for the chess engine, structured into logical modules for better maintainability and understanding.

## Directory Structure

### `core/` — Core Chess Data Structures
Fundamental chess data structures and rule enforcement:
- `ChessBoard.h/cpp` — Board representation and state management
- `ChessEngine.h` — High-level game engine wrapper
- `ChessPiece.h`, `Move.h` — Piece and move definitions
- `Bitboard.h`, `BitboardMoves.h/cpp` — Bitboard representation and move generation
- `BitboardOnly.h/cpp` — Bitboard-only board representation
- `MagicBitboards.h/cpp` — Magic bitboard sliding-piece attacks
- `GameRules.h/cpp` — Check, checkmate, stalemate, draw detection
- `CastlingConstants.h`, `MaterialValues.h`, `SquareSentinel.h` — Shared constants

### `search/` — Search and Move Generation
Search algorithms, pruning, and supporting structures:
- `search.h/cpp`, `search_pvs.cpp`, `search_root.cpp`, `search_move_picker.cpp` — Alpha-beta search with iterative deepening
- `search_internal.h` — Shared search state, constants, and helpers
- `ValidMoves.h/cpp`, `ValidMoves_generation.cpp`, `ValidMoves_internal.h` — Legal move generation
- `AdvancedSearch.h/cpp` — Futility pruning, null move, extensions, aspiration windows
- `LMR.h/cpp` — Late move reductions
- `LazySMP.h/cpp` — Lazy SMP parallel search
- `TranspositionTableV2.h/cpp` — Transposition table with Zobrist hashing
- `BookUtils.h/cpp` — Opening book lookup
- `ZobristKeys.h`, `SearchTuning.h` — Hash keys and tunable search parameters

### `evaluation/` — Position Evaluation
Static evaluation and analysis:
- `Evaluation.h/cpp` — Traditional heuristic evaluation
- `HybridEvaluator.h/cpp` — Weighted blend of traditional and NNUE eval
- `EvaluationTuning.h/cpp`, `TunableParams` (via `utils/`) — Texel-style parameter tuning
- `NNUE.h/cpp`, `NNUEBitboard.h/cpp` — NNUE evaluation
- `PositionAnalysis.h/cpp` — Detailed position analysis for CLI `analyze` mode
- `GamePhaseConstants.h` — Opening/middlegame/endgame phase constants

### `protocol/` — Communication Protocols
- `uci.h/cpp` — UCI protocol (options, `position`, `go`, tablebase/NNUE integration)
- `uci_main.cpp` — UCI binary entry point
- `uci_output.h` — Structured UCI output helpers

### `ai/` — Neural Networks and Tablebases
- `NeuralNetwork.h/cpp` — Neural network eval, self-play training, and data generation
- `SyzygyTablebase.h/cpp` — Syzygy WDL/DTZ probing (used in search and UCI root moves)
- `EndgameTablebase.h/cpp` — Generic endgame tablebase wrapper and endgame knowledge heuristics

### `utils/` — Shared Utilities
- `engine_globals.h/cpp` — Global engine initialization and state
- `ChessFormat.h` — FEN, move, and display formatting
- `SearchThread.h` — Search thread helpers
- `TunableParams.h` — Runtime-tunable engine parameters exposed via UCI

### Entry Points (outside subdirectories)
- `main.cpp` — Interactive CLI, training/tuning subcommands
- `protocol/uci_main.cpp` — Dedicated UCI binary

## Implementation Status

### Completed
- Core chess rules, FEN parsing, and board representation
- Bitboard move generation with magic bitboards
- Alpha-beta search with iterative deepening and quiescence search
- Transposition table with Zobrist hashing
- Advanced pruning (futility, null move, LMR, aspiration windows)
- Move ordering (MVV-LVA, history heuristic, killer moves)
- Opening book support (`BookUtils`, UCI `OwnBook` option)
- Lazy SMP parallel search
- Traditional evaluation with tunable parameters (Texel tuning via CLI)
- NNUE evaluation and hybrid evaluator
- Neural network self-play training and data generation (CLI `train` / `generate`)
- Position analysis tooling (CLI `analyze`)
- UCI protocol with Hash, Threads, MultiPV, NNUE, and tablebase options
- Syzygy tablebase probing in search and at the root
- Adaptive time management in CLI and UCI modes

### In Progress
- EndgameTablebase generic wrapper (Syzygy path is the primary integration today)
- Neural network training pipeline polish (model versioning, stronger default nets)
- SIMD-optimized evaluation paths

### Planned
- Tournament / match mode
- Advanced neural network architectures
- Lock-free transposition table
- Broader Nalimov/custom tablebase support via `EndgameTablebase`

## Build System

Bazel with `MODULE.bazel` (bzlmod) for dependency management. C++26 (`-std=c++26`) with `-Werror`.

## Module Dependencies

```
core  ←  evaluation, search, ai, utils, protocol
evaluation  ←  search, protocol
search  ←  protocol, main
ai  ←  evaluation, search, protocol
utils  ←  search, protocol
protocol  ←  (entry point; depends on search, evaluation, ai)
```

Search and evaluation both depend on `core`. The UCI and CLI entry points sit at the top and wire modules together at runtime.

## Usage

Build binaries:
```bash
bazel build //:engine_cli //:engine_uci
```

Run tests:
```bash
bazel test //tests/...
```

- `engine_cli` — interactive play plus `analyze`, `train`, `test`, `generate`, and `tune` subcommands
- `engine_uci` — UCI-only binary for GUIs

See the [root README](../README.md) for benchmarks, CI checks, and IDE setup.
