CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -pthread -DUSE_ENHANCED_EVALUATION
LDFLAGS = -pthread
TARGET = bin/chess_engine
SRCDIR = src
TESTDIR = tests
BINDIR = bin

SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/EvaluationTuning.cpp $(SRCDIR)/EvaluationEnhanced.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/uci.cpp $(SRCDIR)/NeuralNetwork.cpp $(SRCDIR)/EndgameTablebase.cpp $(SRCDIR)/AdvancedSearch.cpp
HEADERS = $(SRCDIR)/ChessPiece.h $(SRCDIR)/ChessBoard.h $(SRCDIR)/ChessEngine.h $(SRCDIR)/ValidMoves.h $(SRCDIR)/MoveContent.h $(SRCDIR)/search.h $(SRCDIR)/PieceMoves.h $(SRCDIR)/Evaluation.h $(SRCDIR)/EvaluationTuning.h $(SRCDIR)/PieceTables.h $(SRCDIR)/Bitboard.h $(SRCDIR)/BitboardMoves.h $(SRCDIR)/engine_globals.h $(SRCDIR)/uci.h $(SRCDIR)/NeuralNetwork.h $(SRCDIR)/EndgameTablebase.h $(SRCDIR)/AdvancedSearch.h $(SRCDIR)/EvaluationEnhanced.h $(SRCDIR)/PerformanceOptimizations.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(BINDIR)/chess_engine $(BINDIR)/test_parallel $(BINDIR)/test_bitboard_moves $(BINDIR)/test_comprehensive $(BINDIR)/test_fen $(BINDIR)/test_crash $(BINDIR)/test_pawn $(BINDIR)/test_quiescence $(BINDIR)/test_king_safety $(BINDIR)/test_killer_moves $(BINDIR)/test_engine_improvements $(BINDIR)/test_tactical_suite

run: $(TARGET)
	./$(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

release: CXXFLAGS += -DNDEBUG
release: $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/chess_engine

uninstall:
	sudo rm -f /usr/local/bin/chess_engine

$(BINDIR)/test_parallel: $(TESTDIR)/test_parallel.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/EvaluationTuning.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_parallel $(TESTDIR)/test_parallel.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/EvaluationTuning.cpp $(SRCDIR)/uci.cpp $(LDFLAGS)

$(BINDIR)/test_bitboard_moves: $(TESTDIR)/test_bitboard_moves.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_bitboard_moves $(TESTDIR)/test_bitboard_moves.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp $(LDFLAGS)

$(BINDIR)/test_comprehensive: $(TESTDIR)/test_comprehensive.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_comprehensive $(TESTDIR)/test_comprehensive.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_fen: $(TESTDIR)/test_fen.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_fen $(TESTDIR)/test_fen.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_crash: $(TESTDIR)/test_crash.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_crash $(TESTDIR)/test_crash.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_pawn: $(TESTDIR)/test_pawn.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_pawn $(TESTDIR)/test_pawn.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_quiescence: $(TESTDIR)/test_quiescence.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_quiescence $(TESTDIR)/test_quiescence.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_king_safety: $(TESTDIR)/test_king_safety.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_king_safety $(TESTDIR)/test_king_safety.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_killer_moves: $(TESTDIR)/test_killer_moves.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_killer_moves $(TESTDIR)/test_killer_moves.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_engine_improvements: $(TESTDIR)/test_engine_improvements.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_engine_improvements $(TESTDIR)/test_engine_improvements.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_tactical_suite: $(TESTDIR)/test_tactical_suite.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_tactical_suite $(TESTDIR)/test_tactical_suite.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_queen_blunder: $(TESTDIR)/test_queen_blunder.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_queen_blunder $(TESTDIR)/test_queen_blunder.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

$(BINDIR)/test_simple_queen_save: $(TESTDIR)/test_simple_queen_save.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(SRCDIR)/uci.cpp
	@mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -o $(BINDIR)/test_simple_queen_save $(TESTDIR)/test_simple_queen_save.cpp $(SRCDIR)/engine_globals.cpp $(SRCDIR)/BitboardMoves.cpp $(SRCDIR)/ValidMoves.cpp $(SRCDIR)/ChessBoard.cpp $(SRCDIR)/search.cpp $(SRCDIR)/Evaluation.cpp $(LDFLAGS)

tests: $(BINDIR)/test_parallel $(BINDIR)/test_bitboard_moves $(BINDIR)/test_comprehensive $(BINDIR)/test_fen $(BINDIR)/test_crash $(BINDIR)/test_pawn $(BINDIR)/test_quiescence $(BINDIR)/test_king_safety $(BINDIR)/test_killer_moves $(BINDIR)/test_engine_improvements $(BINDIR)/test_tactical_suite

test: tests
	@echo "Running all tests..."
	@echo "\nRunning parallel test..."
	./$(BINDIR)/test_parallel
	@echo "\nRunning bitboard moves test..."
	./$(BINDIR)/test_bitboard_moves
	@echo "\nRunning comprehensive test..."
	./$(BINDIR)/test_comprehensive
	@echo "\nRunning FEN test..."
	./$(BINDIR)/test_fen
	@echo "\nRunning crash test..."
	./$(BINDIR)/test_crash
	@echo "\nRunning pawn test..."
	./$(BINDIR)/test_pawn
	@echo "\nRunning quiescence test..."
	./$(BINDIR)/test_quiescence
	@echo "\nRunning king safety test..."
	./$(BINDIR)/test_king_safety
	@echo "\nRunning killer moves test..."
	./$(BINDIR)/test_killer_moves
	@echo "\nRunning combined improvements test..."
	./$(BINDIR)/test_engine_improvements

performance_tests: test_quiescence test_king_safety test_killer_moves test_engine_improvements
	@echo "All performance tests compiled successfully!"

.PHONY: all clean run debug release install uninstall test tests performance_tests 