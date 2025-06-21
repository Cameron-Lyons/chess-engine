# Chess Engine

A C++ chess engine with a console-based interface that allows you to play chess against a computer opponent.

## Features

- **Complete Chess Rules**: Implements all standard chess rules including:
  - All piece movements (pawn, knight, bishop, rook, queen, king)
  - Castling (kingside and queenside)
  - Pawn promotion
  - Check and checkmate detection
  - Stalemate detection

- **AI Engine**: 
  - Alpha-beta search algorithm
  - Configurable search depth
  - Basic position evaluation

- **User Interface**:
  - ASCII board display
  - Algebraic notation input (e.g., e2e4)
  - Move validation
  - Game state feedback

## Building the Engine

### Prerequisites
- C++11 compatible compiler (GCC 4.8+ or Clang 3.3+)
- Make

### Compilation
```bash
# Build the engine
make

# Or build with debug information
make debug

# Or build optimized release version
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
   - `e2e4` - Move from e2 to e4
   - `g1f3` - Move knight from g1 to f3
   - `e1g1` - Kingside castle (if legal)

3. **Game Flow**:
   - You enter your move
   - The engine validates the move
   - If valid, the move is executed and the board is updated
   - The computer calculates and makes its move
   - The process repeats until checkmate or stalemate

4. **Ending the Game**: Type `quit` or `exit` to end the game early.

## File Structure

- `main.cpp` - Main program with console interface
- `ChessPiece.h` - Piece definitions and properties
- `ChessBoard.h` - Board representation and game state
- `ChessEngine.h` - Core engine functions
- `ValidMoves.h` - Move generation and validation
- `MoveContent.h` - Move representation and PGN notation
- `search.h` - AI search algorithms
- `PieceMoves.h` - Pre-calculated move tables
- `Evaluation.h` - Position evaluation functions
- `PieceTables.h` - Piece-square tables (currently empty)
- `Makefile` - Build configuration

## Technical Details

### Move Generation
The engine uses a combination of:
- Pre-calculated move tables for efficiency
- Dynamic move validation for legality checking
- Attack board tracking for check detection

### Search Algorithm
- Alpha-beta pruning for efficient search
- Configurable search depth (default: 3 ply)
- Basic material and position evaluation

### Board Representation
- 8x8 array representation
- 0x88 coordinate system for move calculations
- FEN string support for position initialization

## Limitations and Future Improvements

### Current Limitations
- Basic evaluation function (material + position)
- Limited search depth
- No opening book
- No endgame tablebases
- No time management

### Potential Improvements
- Implement more sophisticated evaluation
- Add opening book support
- Implement transposition tables
- Add UCI protocol support
- Improve move ordering for better pruning
- Add endgame tablebase support
- Implement time management

## License

This project is open source. Feel free to modify and distribute.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.
