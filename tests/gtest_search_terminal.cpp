#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "core/ChessEngine.h"
#include "gtest/gtest.h"
#include "search/search.h"

#include <cstdlib>

class SearchTerminalTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(SearchTerminalTest, SearchReturnsNoMoveOnStalemate) {
    Board board;
    board.InitializeFromFEN("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    const std::string originalFen = board.toFEN();

    SearchConfig config;
    config.maxDepth = 4;
    config.timeLimitMs = 50;
    SearchContext context;

    const SearchResult result = iterativeDeepeningParallel(board, config, context);

    EXPECT_EQ(result.bestMove.first, -1);
    EXPECT_EQ(result.bestMove.second, -1);
    EXPECT_EQ(result.score, 0);
    EXPECT_EQ(board.toFEN(), originalFen);
}

TEST_F(SearchTerminalTest, SearchReturnsNoMoveOnCheckmate) {
    Board board;
    board.InitializeFromFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    const std::string originalFen = board.toFEN();

    SearchConfig config;
    config.maxDepth = 4;
    config.timeLimitMs = 50;
    SearchContext context;

    const SearchResult result = iterativeDeepeningParallel(board, config, context);

    EXPECT_EQ(result.bestMove.first, -1);
    EXPECT_EQ(result.bestMove.second, -1);
    EXPECT_GT(std::abs(result.score), 9000);
    EXPECT_EQ(board.toFEN(), originalFen);
}

TEST_F(SearchTerminalTest, SearchScoresFiftyMovePositionAsDrawButKeepsLegalMove) {
    Board board;
    board.InitializeFromFEN("4k2n/8/8/8/8/8/8/4K1N1 w - - 100 75");
    const std::string originalFen = board.toFEN();

    SearchConfig config;
    config.maxDepth = 4;
    config.timeLimitMs = 50;
    SearchContext context;

    const SearchResult result = iterativeDeepeningParallel(board, config, context);

    EXPECT_EQ(result.score, 0);
    EXPECT_GE(result.bestMove.first, 0);
    EXPECT_GE(result.bestMove.second, 0);
    EXPECT_EQ(board.toFEN(), originalFen);
}

TEST_F(SearchTerminalTest, SearchScoresRepeatedPositionAsDrawFromHistory) {
    Engine engine;
    const Move repetitionLine[] = {
        {6, 21}, {62, 45}, {21, 6}, {45, 62}, {6, 21}, {62, 45}, {21, 6}, {45, 62},
    };
    for (const Move& move : repetitionLine) {
        ASSERT_TRUE(engine.applyMove(move).has_value());
    }

    Board board = engine.board();
    const std::string originalFen = board.toFEN();

    SearchConfig config;
    config.maxDepth = 4;
    config.timeLimitMs = 50;
    config.previousPositionHashes = engine.previousPositionHashes();
    SearchContext context;

    const SearchResult result = iterativeDeepeningParallel(board, config, context);

    EXPECT_EQ(result.score, 0);
    EXPECT_GE(result.bestMove.first, 0);
    EXPECT_GE(result.bestMove.second, 0);
    EXPECT_EQ(board.toFEN(), originalFen);
}
