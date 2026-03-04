# Chess Engine Source Code Organization

This directory contains the organized source code for the chess engine, structured into logical modules for better maintainability and understanding.

## Directory Structure

### `core/` - Core Chess Data Structures
Contains the fundamental chess data structures and basic functionality:
- `ChessBoard.h/cpp` - Main board representation and state management
- `ChessPiece.h` - Piece definitions and properties
- `Bitboard.h` - Bitboard data structure for efficient piece tracking
- `BitboardMoves.h/cpp` - Bitboard-based move generation
- `BitboardOnly.h/cpp` - Bitboard-only board representation
- `MagicBitboards.h/cpp` - Magic bitboard implementation for sliding pieces
- `MoveContent.h` - Move representation and utilities

### `search/` - Search Algorithms and Move Generation
Contains all search-related functionality:
- `search.h/cpp` - Main search algorithms (minimax, alpha-beta)
- `ValidMoves.h/cpp` - Legal move generation and validation
- `AdvancedSearch.h/cpp` - Advanced search techniques (futility pruning, null move, extensions)
- `LazySMP.h/cpp` - Lazy SMP (Symmetric Multi-Processing) parallel search
- `LMR.h/cpp` - Enhanced Late Move Reductions
- `TranspositionTableV2.h/cpp` - Advanced transposition table implementation

### `evaluation/` - Position Evaluation
Contains position evaluation functions:
- `Evaluation.h/cpp` - Basic position evaluation
- `HybridEvaluator.h/cpp` - Hybrid evaluation with advanced features
- `EvaluationTuning.h/cpp` - Tunable evaluation parameters
- `NNUE.h` - NNUE (Efficiently Updatable Neural Network) evaluation
- `NNUEBitboard.h/cpp` - Bitboard NNUE implementation
- `PositionAnalysis.h/cpp` - Position analysis utilities

### `protocol/` - Communication Protocols
Contains protocol implementations:
- `uci.h/cpp` - Universal Chess Interface (UCI) protocol implementation

### `ai/` - Advanced AI Features
Contains advanced AI and machine learning components:
- `NeuralNetwork.h/cpp` - Neural network evaluation and training
- `EndgameTablebase.h/cpp` - Endgame tablebase support
- `SyzygyTablebase.h/cpp` - Syzygy tablebase integration

### `utils/` - Utilities and Optimizations
Contains utility functions and performance optimizations:
- `engine_globals.h/cpp` - Global engine state and configuration
- `Profiler.h` - Performance profiling tools

## Implementation Status

### Completed Features ✅
- Core chess rules and board representation
- Bitboard move generation with magic bitboards
- Alpha-beta search with iterative deepening
- Transposition table with Zobrist hashing
- Advanced pruning techniques (futility, null move, LMR)
- Move ordering (MVV-LVA, history heuristic, killer moves)
- Neural network evaluation framework
- UCI protocol support
- Time management system
- Parallel search (Lazy SMP)

### In Progress 🔄
- Endgame tablebase integration
- Neural network training pipeline
- Advanced time management features

### Planned 📋
- Syzygy tablebase integration
- Advanced neural network architectures
- Tournament mode
- Position analysis tools

## Build System

Bazel with `MODULE.bazel` (bzlmod) for dependency management. C++23 standard with `-Werror`.

## Dependencies

- Core modules have minimal dependencies
- Search depends on core and evaluation
- Evaluation depends on core
- Protocol depends on search, evaluation, and AI
- AI depends on core
- Utils depend on core and search

## Usage

To build the project:
```bash
bazel build //:engine_cli //:engine_uci
```

To run tests:
```bash
bazel test //tests/...
```

The CLI executable is built as `engine_cli` and the UCI executable as `engine_uci`.

## Next Steps

1. **Endgame Tablebase Integration**
   - Complete Syzygy tablebase support
   - Add tablebase probing in search
   - Implement endgame-specific evaluation

2. **Neural Network Training**
   - Complete training data generation
   - Implement batch training pipeline
   - Add model versioning and management

3. **Performance Optimizations**
   - SIMD-optimized evaluation
   - Lock-free transposition table
   - Cache-optimized data structures
