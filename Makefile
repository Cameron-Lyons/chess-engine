CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread
TARGET = chess_engine
SOURCES = main.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp engine_globals.cpp
HEADERS = ChessPiece.h ChessBoard.h ChessEngine.h ValidMoves.h MoveContent.h search.h PieceMoves.h Evaluation.h PieceTables.h Bitboard.h BitboardMoves.h engine_globals.h

# Default target
all: $(TARGET)

# Build the chess engine
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Clean build files
clean:
	rm -f $(TARGET) test_parallel test_bitboard_moves test_comprehensive

# Run the chess engine
run: $(TARGET)
	./$(TARGET)

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build
release: CXXFLAGS += -DNDEBUG
release: $(TARGET)

# Install (copy to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Test targets
test_parallel: test_parallel.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp
	$(CXX) $(CXXFLAGS) -o test_parallel test_parallel.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp $(LDFLAGS)

test_bitboard_moves: test_bitboard_moves.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp
	$(CXX) $(CXXFLAGS) -o test_bitboard_moves test_bitboard_moves.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp $(LDFLAGS)

test_comprehensive: test_comprehensive.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp
	$(CXX) $(CXXFLAGS) -o test_comprehensive test_comprehensive.cpp engine_globals.cpp BitboardMoves.cpp ValidMoves.cpp ChessBoard.cpp search.cpp Evaluation.cpp $(LDFLAGS)

# Run all tests
test: test_parallel test_bitboard_moves test_comprehensive
	@echo "Running parallel search test..."
	./test_parallel
	@echo "\nRunning bitboard moves test..."
	./test_bitboard_moves
	@echo "\nRunning comprehensive test..."
	./test_comprehensive

.PHONY: all clean run debug release install uninstall test 