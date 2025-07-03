# Chess Engine

A modern C++23 chess engine with a console-based interface that allows you to play chess against a computer opponent. Features a full bitboard implementation for efficient move generation and modern C++23 language features.

## Features

- **Complete Chess Rules**: Implements all standard chess rules including:
  - All piece movements (pawn, knight, bishop, rook, queen, king)
  - Castling (kingside and queenside)
  - Pawn promotion
  - Check and checkmate detection
  - Stalemate detection

- **Modern Bitboard Implementation**:
  - Full bitboard move generation for all piece types
  - Efficient bitboard operations using modern C++23
  - Bitboard-based move legality checking
  - Bitboard synchronization with array representation
  - Pre-calculated attack tables for knights and kings

- **AI Engine**: 
  - Alpha-beta search algorithm with modern optimizations
  - Iterative deepening
  - Transposition table for fast re-search
  - Move ordering (MVV-LVA, history heuristic)
  - Null Move Pruning (faster search, detects zugzwang)
  - Late Move Reductions (LMR) for efficient deep search
  - Configurable search depth
  - Position evaluation with center control and material counting
  - Opening book support (FEN-based)

- **Modern C++23 Features**:
  - Written in C++23 standard
  - Uses `std::string_view` for efficient string handling
  - `std::chrono` for precise timing
  - Modern algorithms and containers
  - Template metaprogramming for bitboard operations
  - Lambda expressions and functional programming patterns
  - `std::unordered_map` and other STL features

- **User Interface**:
  - ASCII board display with modern formatting
  - Algebraic notation input (e.g., e4, Nf3, exd5)
  - Move validation with detailed error messages
  - Game state feedback and timing information
  - Support for simple pawn moves (e4, d5) and captures (exd5)

## Building the Engine

### Prerequisites
- C++23 compatible compiler (GCC 13+, Clang 16+, or recent MSVC)
- Make

### Compilation
```bash
# Build the engine
make

# Clean build
make clean

# Build with debug information
make debug

# Build optimized release version
make release
```

## Running the Engine

```bash
# Run the chess engine
./chess_engine

# Or use make
make run
```

## How to Play

1. **Starting the Game**: The engine starts with the standard chess position. You play as White, the computer plays as Black.

2. **Making Moves**: Enter moves in algebraic notation:
   - `e4` - Simple pawn move to e4
   - `d5` - Simple pawn move to d5
   - `exd5` - Pawn capture from e-file to d5
   - `Nf3` - Knight move to f3
   - `O-O` - Kingside castle (if legal)
   - `O-O-O` - Queenside castle (if legal)

3. **Game Flow**:
   - You enter your move
   - The engine validates the move
   - If valid, the move is executed and the board is updated
   - The computer calculates and makes its move using evaluation-based move selection
   - The process repeats until checkmate or stalemate

4. **Ending the Game**: Type `quit` or `exit` to end the game early.

## File Structure

- `main.cpp` - Main program with console interface and modern C++23 features
- `ChessBoard.h/cpp` - Board representation with bitboard integration
- `ChessPiece.h` - Piece definitions and properties
- `ChessEngine.h` - Core engine functions
- `ValidMoves.h/cpp` - Move generation and validation
- `MoveContent.h` - Move representation and PGN notation
- `search.h` - AI search algorithms (alpha-beta, null move pruning, LMR, transposition table)
- `PieceMoves.h` - Pre-calculated move tables
- `Evaluation.h` - Position evaluation functions
- `PieceTables.h` - Piece-square tables
- `Bitboard.h` - Bitboard type definitions and utilities
- `BitboardMoves.h/cpp` - Bitboard-based move generation
- `Makefile` - Build configuration

## Technical Details

### Bitboard Implementation
The engine uses a full bitboard representation for efficient move generation:
- 64-bit integers representing piece positions
- Pre-calculated attack tables for knights and kings
- Bitboard-based move generation for all piece types
- Efficient bitboard operations using modern C++23 features
- Synchronization between bitboard and array representations

### Move Generation
The engine uses a hybrid approach:
- Bitboard-based move generation for efficiency
- Array-based validation for move legality
- Pre-calculated move tables for complex pieces
- Dynamic move validation for check detection

### Search Algorithm
- Alpha-beta pruning for efficient search
- Iterative deepening for best-move search
- Transposition table for caching positions
- Null Move Pruning for faster search
- Late Move Reductions (LMR) for deep search efficiency
- Move ordering (MVV-LVA, history heuristic)
- Configurable search depth (default: 3 ply)
- Evaluation-based move selection with center control and material counting

### Board Representation
- 8x8 array representation with bitboard integration
- FEN string support for position initialization
- Modern C++23 timing and string handling

## Testing

The engine includes several test programs:
- `test_bitboard_moves` - Tests bitboard move generation
- `test_comprehensive` - Comprehensive move generation tests
- `test_fen` - FEN parsing and board initialization tests
- `test_pawn` - Pawn move generation tests

```bash
# Run tests
make test_bitboard_moves
make test_comprehensive
make test_fen
make test_pawn
```

## Engine Strengths and Features

### Current Strengths
- Modern C++23 implementation with efficient bitboard operations
- Alpha-beta search with advanced pruning techniques
- Transposition table with Zobrist hashing
- Parallel search capabilities
- Comprehensive move generation and validation
- Configurable evaluation parameters

### Advanced Features (Version 2.0)
- **Neural Network Evaluation**: Machine learning-based position evaluation
- **Endgame Tablebases**: Perfect play in endgame positions (Syzygy format)
- **Advanced Search Techniques**: Futility pruning, static null move pruning, singular extensions
- **UCI Protocol Support**: Full GUI integration support
- **Enhanced Evaluation**: Sophisticated positional understanding
- **Performance Optimizations**: SIMD, cache optimizations, lock-free data structures
- **Time Management**: Intelligent time allocation for tournament play
- **Work-Stealing Thread Pool**: Efficient parallel processing

## Implementation Roadmap

### Phase 1: Core Improvements (Completed)
- ✅ Enhanced evaluation function with positional understanding
- ✅ Advanced search techniques (futility pruning, static null move pruning)
- ✅ UCI protocol implementation
- ✅ Performance optimizations framework

### Phase 2: Machine Learning Integration (In Progress)
- 🔄 Neural network evaluation implementation
- 🔄 Training data collection and model training
- 🔄 Hybrid evaluation (NN + traditional)
- 🔄 Model optimization and tuning

### Phase 3: Endgame Perfection (Planned)
- 📋 Endgame tablebase integration (Syzygy)
- 📋 Endgame knowledge implementation
- 📋 Perfect play in 7-piece endgames
- 📋 Endgame-specific evaluation

### Phase 4: Advanced Features (Planned)
- 📋 Opening book with statistical analysis
- 📋 Advanced time management
- 📋 Tournament mode with adaptive play
- 📋 Position analysis tools
- 📋 Self-play training framework

### Phase 5: Performance Excellence (Planned)
- 📋 SIMD-optimized evaluation
- 📋 Lock-free transposition table
- 📋 Work-stealing thread pool
- 📋 Cache-optimized data structures
- 📋 Branch prediction optimization

## Expected Strength Improvements

With these enhancements, the engine is expected to achieve:

- **Search Depth**: 8-12 plies in typical positions (vs current 6-8)
- **Evaluation Quality**: 30-50% improvement through neural network
- **Endgame Play**: Perfect play in 7-piece endgames
- **Tactical Strength**: 20-30% improvement through advanced pruning
- **Positional Understanding**: Significant improvement through enhanced evaluation
- **Overall Rating**: Estimated 200-300 Elo points improvement

## Performance Targets

- **Nodes per Second**: 1-2 million on modern hardware
- **Memory Usage**: < 1GB for typical games
- **Search Efficiency**: 90%+ transposition table hit rate
- **Parallel Scaling**: Near-linear scaling up to 16 threads

## License

This project is open source. Feel free to modify and distribute.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
