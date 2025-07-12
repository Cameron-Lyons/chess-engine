# Chess Engine Source Code Organization

This directory contains the organized source code for the chess engine, structured into logical modules for better maintainability and understanding.

## Directory Structure

### `core/` - Core Chess Data Structures
Contains the fundamental chess data structures and basic functionality:
- `ChessBoard.h/cpp` - Main board representation and state management
- `ChessPiece.h` - Piece definitions and properties
- `Bitboard.h` - Bitboard data structure for efficient piece tracking
- `BitboardMoves.h/cpp` - Bitboard-based move generation
- `MoveContent.h` - Move representation and utilities

### `search/` - Search Algorithms and Move Generation
Contains all search-related functionality:
- `search.h/cpp` - Main search algorithms (minimax, alpha-beta)
- `ValidMoves.h/cpp` - Legal move generation and validation
- `AdvancedSearch.h/cpp` - Advanced search techniques (futility pruning, null move, etc.)

### `evaluation/` - Position Evaluation
Contains position evaluation functions:
- `Evaluation.h/cpp` - Basic position evaluation
- `EvaluationEnhanced.h/cpp` - Enhanced evaluation with advanced features
- `EvaluationTuning.h/cpp` - Tunable evaluation parameters

### `protocol/` - Communication Protocols
Contains protocol implementations:
- `uci.h/cpp` - Universal Chess Interface (UCI) protocol implementation

### `ai/` - Advanced AI Features
Contains advanced AI and machine learning components:
- `NeuralNetwork.h/cpp` - Neural network evaluation
- `EndgameTablebase.h/cpp` - Endgame tablebase support

### `utils/` - Utilities and Optimizations
Contains utility functions and performance optimizations:
- `engine_globals.h/cpp` - Global engine state and configuration
- `PerformanceOptimizations.h` - Performance optimization techniques
- `PieceMoves.h` - Piece-specific move utilities
- `PieceTables.h` - Piece-square tables
- `ModernChess.h` - Modern chess concepts and utilities

## Build System

The CMakeLists.txt file has been updated to reflect the new directory structure. All include paths have been updated to use relative paths from the appropriate directories.

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
mkdir build
cd build
cmake ..
make
```

The main executable will be built as `chess_engine` and can be run directly or through UCI-compatible chess GUIs. 