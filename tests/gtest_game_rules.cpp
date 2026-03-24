#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "core/GameRules.h"
#include "gtest/gtest.h"
#include "search/search.h"

class GameRulesTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(GameRulesTest, DetectsStalemate) {
    Board board;
    board.InitializeFromFEN("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

    EXPECT_EQ(checkGameState(board), GameState::STALEMATE);
}

TEST_F(GameRulesTest, DetectsCheckmateAgainstBlack) {
    Board board;
    board.InitializeFromFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

    EXPECT_EQ(checkGameState(board), GameState::CHECKMATE_WHITE_WINS);
}

TEST_F(GameRulesTest, DetectsBareKingsAsDraw) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    EXPECT_EQ(checkGameState(board), GameState::DRAW_INSUFFICIENT_MATERIAL);
}

TEST_F(GameRulesTest, DetectsBishopVersusKingAsDraw) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/3B4/4K3 w - - 0 1");

    EXPECT_EQ(checkGameState(board), GameState::DRAW_INSUFFICIENT_MATERIAL);
}

TEST_F(GameRulesTest, DetectsOngoingPositionWithLegalMoves) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    EXPECT_EQ(checkGameState(board), GameState::ONGOING);
    EXPECT_TRUE(hasAnyLegalMoves(board, ChessPieceColor::WHITE));
}
