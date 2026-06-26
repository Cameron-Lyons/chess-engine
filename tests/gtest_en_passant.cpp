#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"
#include "test_helpers.h"

class EnPassantTest : public ::testing::Test {
protected:
    void SetUp() override {
        initEngineForTests();
    }
};

TEST_F(EnPassantTest, CaptureIsGeneratedForValidTargetSquare) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");

    EXPECT_TRUE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 36, 43));
}

TEST_F(EnPassantTest, CaptureIsNotGeneratedWithoutEnPassantTarget) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1");

    EXPECT_FALSE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 36, 43));
}

TEST_F(EnPassantTest, CaptureRemovesCapturedPawnAndUpdatesFen) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");

    ASSERT_TRUE(applySearchMove(board, 36, 43, false));
    EXPECT_EQ(board.squares[43].piece.PieceType, ChessPieceType::PAWN);
    EXPECT_EQ(board.squares[43].piece.PieceColor, ChessPieceColor::WHITE);
    EXPECT_EQ(board.squares[35].piece.PieceType, ChessPieceType::NONE);
    EXPECT_FALSE(board.enPassantSquare.target().has_value());
}

TEST_F(EnPassantTest, DoublePushSetsEnPassantTargetSquare) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    ASSERT_TRUE(board.movePiece(12, 28));
    ASSERT_TRUE(board.enPassantSquare.target().has_value());
    EXPECT_EQ(*board.enPassantSquare.target(), 20);
}

TEST_F(EnPassantTest, EnPassantRoundTripPreservesFenAndHash) {
    expectFenRoundTrip("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
}
