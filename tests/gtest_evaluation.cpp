#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "evaluation/Evaluation.h"
#include "gtest/gtest.h"
#include "search/search.h"
#include "test_helpers.h"

class EvaluationTest : public ::testing::Test {
protected:
    void SetUp() override {
        initEngineForTests();
        setNNUEEnabled(false);
    }
};

TEST_F(EvaluationTest, StartingPositionIsRoughlyBalanced) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    const int whiteScore = evaluatePosition(board);
    board.turn = ChessPieceColor::BLACK;
    const int blackScore = evaluatePosition(board);

    EXPECT_NEAR(whiteScore, 0, 150);
    EXPECT_NEAR(blackScore, 0, 150);
}

TEST_F(EvaluationTest, MaterialAdvantageIncreasesScoreForSideToMove) {
    Board bareKing;
    bareKing.InitializeFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    Board queenUp;
    queenUp.InitializeFromFEN("4k3/8/8/8/8/8/8/Q3K3 w - - 0 1");

    EXPECT_GT(evaluatePosition(queenUp), evaluatePosition(bareKing));
}

TEST_F(EvaluationTest, KingSafetyPenalizesExposedKing) {
    Board safeKing;
    safeKing.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    Board exposedKing;
    exposedKing.InitializeFromFEN("rnbqkbnr/ppp1pppp/8/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1");

    EXPECT_LT(evaluateKingSafety(exposedKing, ChessPieceColor::WHITE),
              evaluateKingSafety(safeKing, ChessPieceColor::WHITE));
}

TEST_F(EvaluationTest, EvaluationChangesWhenSideToMoveChanges) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/8/Q3K3 w - - 0 1");
    const int whiteToMove = evaluatePosition(board);

    board.turn = ChessPieceColor::BLACK;
    const int blackToMove = evaluatePosition(board);

    EXPECT_NE(whiteToMove, blackToMove);
}

TEST_F(EvaluationTest, CheckmatePositionScoresExtremelyHigh) {
    Board board;
    board.InitializeFromFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

    SearchConfig config;
    config.maxDepth = 4;
    config.timeLimitMs = 50;
    SearchContext context;
    const SearchResult result = iterativeDeepeningParallel(board, config, context);

    EXPECT_GT(std::abs(result.score), 9000);
}
