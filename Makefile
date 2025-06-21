CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2
TARGET = chess_engine
SOURCES = main.cpp
HEADERS = ChessPiece.h ChessBoard.h ChessEngine.h ValidMoves.h MoveContent.h search.h PieceMoves.h Evaluation.h PieceTables.h

# Default target
all: $(TARGET)

# Build the chess engine
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# Clean build files
clean:
	rm -f $(TARGET)

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

.PHONY: all clean run debug release install uninstall 