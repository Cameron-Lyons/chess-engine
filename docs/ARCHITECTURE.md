# Chess Engine Architecture

## Overview

A C++23 chess engine built with Bazel, featuring bitboard-based move generation,
alpha-beta search with advanced pruning, multiple evaluation backends (classical
piece-square tables, tuned parameters, NNUE), and UCI protocol support.

## Directory Structure

```
src/
├── main.cpp                  # Entry point, console UI, game loop
├── core/                     # Board representation and move generation
├── search/                   # Search algorithms and move ordering
├── evaluation/               # Position evaluation (classical + NNUE)
├── ai/                       # Opening book, endgame tablebases, neural network
├── protocol/                 # UCI protocol implementation
└── utils/                    # Shared types, profiling, engine globals
tests/                        # Google Test suite (13 targets)
```

## Core Components

### Board Representation (`src/core/`)

Two parallel board representations coexist:

- **`ChessBoard.h/cpp`** — Mailbox array (`squares[64]`) plus per-piece-type
  bitboards (`whitePawns`, `whiteKnights`, ...). Used by the classical evaluation
  and main search.
- **`BitboardOnly.h/cpp`** — Pure bitboard representation (`BitboardPosition`)
  with `pieces[2][6]` and occupancy arrays. Used by the NNUE evaluator.
- **`Bitboard.h`** — Common bitboard utilities (`popcount`, `lsb`, `msb`,
  `set_bit`, `clear_bit`) using C++20 `<bit>`.
- **`BitboardMoves.h/cpp`** — Bitboard-based legal move generation with
  pre-computed attack tables.
- **`MagicBitboards.h/cpp`** — Magic bitboard tables for sliding piece attacks.
- **`ChessPiece.h`** — Piece type/color enums and material value lookups.

### Search (`src/search/`)

- **`search.h/cpp`** — Main alpha-beta search with iterative deepening,
  aspiration windows, transposition table, killer moves, history heuristic,
  and quiescence search.
- **`AdvancedSearch.h/cpp`** — Advanced pruning techniques: null-move pruning,
  futility pruning, late move reductions, singular extensions, multi-cut.
- **`LMREnhanced.h/cpp`** — Late move reduction tables and tuning.
- **`LazySMP.h/cpp`** — Lazy SMP parallel search across multiple threads.
- **`TranspositionTableV2.h`** — Cache-line-aligned transposition table with
  bucket replacement scheme.
- **`ValidMoves.h/cpp`** — Legal move generation and validation.
- **`MoveOrdering.h`** — Move scoring for search ordering (MVV-LVA, history, killers).
- **`SearchEnhancements.h`** — Search configuration and parameters.
- **`SearchImprovements.h`** — Parallel search coordinator.
- **`EnhancedSearch.h/cpp`** — Alternative search entry point.

### Evaluation (`src/evaluation/`)

Multiple evaluation backends, selected at runtime:

- **`Evaluation.h/cpp`** — Classical evaluation: piece-square tables (middlegame
  and endgame), pawn structure, mobility, king safety, bishop pair, rook on
  open files, passed pawns, hanging pieces, queen trap detection.
  Tapered evaluation interpolates between middlegame and endgame scores.
- **`EvaluationTuning.h/cpp`** — Tuned piece-square tables from Texel tuning.
  Includes the `TexelTuner` for parameter optimization from game data.
- **`EvaluationEnhanced.h/cpp`** — Hybrid evaluator combining classical and
  NNUE scores.
- **`NNUE.h/cpp`** — Basic NNUE (Efficiently Updatable Neural Network)
  evaluation with incremental accumulator updates.
- **`NNUEOptimized.h/cpp`** — SIMD-optimized NNUE using AVX2/AVX512/VNNI
  intrinsics with runtime CPU feature detection.
- **`PositionAnalysis.h/cpp`** — Position analysis and reporting utilities.

### AI (`src/ai/`)

- **`EndgameTablebase.h/cpp`** — Endgame knowledge and basic endgame
  evaluation rules.
- **`SyzygyTablebase.h/cpp`** — Syzygy tablebase probing for WDL/DTZ
  in endgame positions.
- **`NeuralNetwork.h/cpp`** — General neural network infrastructure
  with training support.

### Protocol (`src/protocol/`)

- **`uci.h/cpp`** — UCI (Universal Chess Interface) protocol implementation
  for GUI communication.

### Utilities (`src/utils/`)

- **`ModernChess.h`** — Common type aliases, error enum, `Move` and `Position`
  structs with `constexpr` operations.
- **`engine_globals.h/cpp`** — Global engine state and configuration.
- **`PerformanceOptimizations.h`** — SIMD helpers, thread pool, lock-free TT,
  cache-optimized board layout.
- **`Profiler.h`** — Scoped profiling with per-function timing and NPS stats.
- **`PieceMoves.h`** — Piece movement tables.
- **`PerftTest.h`** — Perft (performance test) move counting for validation.
- **`TunableParams.h`** — Runtime-tunable search/evaluation parameters.

## Key Algorithms

### Search Pipeline

1. **Iterative deepening** — Search depth 1, 2, 3, ... with time management.
2. **Aspiration windows** — Narrow alpha-beta window around previous score.
3. **Move ordering** — Hash move, captures (MVV-LVA + SEE), killers, history.
4. **Alpha-beta** with:
   - Transposition table cutoffs
   - Null-move pruning
   - Futility pruning
   - Late move reductions
   - Singular extensions
   - Multi-cut pruning
5. **Quiescence search** — Extends captures/checks to avoid horizon effect.

### Evaluation Pipeline

1. Check if NNUE is enabled and loaded → use NNUE score.
2. Otherwise, classical evaluation:
   - Material count
   - Tuned piece-square tables (middlegame + endgame)
   - Pawn structure (doubled, isolated, passed pawns)
   - Piece mobility
   - King safety (pawn shield, open files)
   - Tactical features (hanging pieces, queen traps)
3. Tapered eval: interpolate MG/EG scores by game phase.

## Build System

- **Bazel** with `MODULE.bazel` (bzlmod) for dependency management.
- C++23 standard with `-Wall -Wextra -Wpedantic`.
- `-Werror` on engine library and binary targets.
- Tests use Google Test 1.17.

## Testing

13 test targets covering:
- `gtest_bitboard_moves` — Bitboard move generation
- `gtest_comprehensive` — General engine behavior
- `gtest_crash` — Crash resistance
- `gtest_engine_improvements` — Engine feature tests
- `gtest_fen` — FEN parsing and generation
- `gtest_killer_moves` — Killer move heuristic
- `gtest_king_safety` — King safety evaluation
- `gtest_parallel` — Multi-threaded search
- `gtest_pawn` — Pawn evaluation and structure
- `gtest_perft` — Move generation correctness (perft counts)
- `gtest_quiescence` — Quiescence search
- `gtest_tactical_suite` — Tactical puzzle solving
- `gtest_wac` — Win At Chess test suite

## CI/CD

GitHub Actions workflow (`.github/workflows/ci.yml`):
- **Format check** — Verifies all C++ files pass `clang-format`.
- **Build & test** — Matrix of GCC 14 and Clang 18, runs full test suite.
