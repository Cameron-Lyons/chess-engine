#include "core/BitboardMoves.h"
#include "core/CastlingConstants.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"
#include "test_helpers.h"

class CastlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        initEngineForTests();
    }
};

TEST_F(CastlingTest, WhiteKingsideCastleIsLegalInOpenPosition) {
    Board board;
    board.InitializeFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    EXPECT_TRUE(IsMoveLegal(board, 4, 6));
    EXPECT_TRUE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 4, 6));
}

TEST_F(CastlingTest, WhiteQueensideCastleIsLegalInOpenPosition) {
    Board board;
    board.InitializeFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    EXPECT_TRUE(IsMoveLegal(board, 4, 2));
    EXPECT_TRUE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 4, 2));
}

TEST_F(CastlingTest, CastleBlockedWhenKingIsInCheck) {
    Board board;
    board.InitializeFromFEN("8/8/8/8/8/8/4q3/R3K2R w KQ - 0 1");

    EXPECT_TRUE(IsKingInCheck(board, ChessPieceColor::WHITE));
    EXPECT_FALSE(IsMoveLegal(board, 4, 6));
    EXPECT_FALSE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 4, 6));
}

TEST_F(CastlingTest, CastleBlockedWhenPassingThroughCheck) {
    Board board;
    board.InitializeFromFEN("8/8/8/8/8/8/6q1/R3K2R w KQ - 0 1");

    EXPECT_FALSE(IsKingInCheck(board, ChessPieceColor::WHITE));
    EXPECT_FALSE(IsMoveLegal(board, 4, 6));
}

TEST_F(CastlingTest, CastleBlockedWhenDestinationIsAttacked) {
    Board board;
    board.InitializeFromFEN("8/8/8/8/8/8/5q1/R3K2R w KQ - 0 1");

    EXPECT_FALSE(IsMoveLegal(board, 4, 6));
}

TEST_F(CastlingTest, KingMoveRemovesCastlingRights) {
    Board board;
    board.InitializeFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    ASSERT_TRUE(board.movePiece(4, 5));
    EXPECT_FALSE(board.hasCastlingRight(CastlingConstants::kWhiteKingsideCastlingRight));
    EXPECT_FALSE(board.hasCastlingRight(CastlingConstants::kWhiteQueensideCastlingRight));
}

TEST_F(CastlingTest, RookMoveRemovesSideCastlingRights) {
    Board board;
    board.InitializeFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    ASSERT_TRUE(board.movePiece(7, 6));
    EXPECT_FALSE(board.hasCastlingRight(CastlingConstants::kWhiteKingsideCastlingRight));
    EXPECT_TRUE(board.hasCastlingRight(CastlingConstants::kWhiteQueensideCastlingRight));
    EXPECT_TRUE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 4, 2));
    EXPECT_FALSE(containsMove(GetAllMoves(board, ChessPieceColor::WHITE), 4, 6));
}

TEST_F(CastlingTest, CastlingMoveUpdatesRookAndKingSquares) {
    Board board;
    board.InitializeFromFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");

    ASSERT_TRUE(applySearchMove(board, 4, 6, false));
    EXPECT_EQ(board.squares[6].piece.PieceType, ChessPieceType::KING);
    EXPECT_EQ(board.squares[5].piece.PieceType, ChessPieceType::ROOK);
    EXPECT_EQ(board.squares[4].piece.PieceType, ChessPieceType::NONE);
    EXPECT_EQ(board.squares[7].piece.PieceType, ChessPieceType::NONE);
}

TEST_F(CastlingTest, PartialCastlingRightsArePreservedInFen) {
    expectFenRoundTrip("r3k2r/8/8/8/8/8/8/R3K2R w K - 0 1");
    expectFenRoundTrip("r3k2r/8/8/8/8/8/8/R3K2R w Qq - 0 1");
    expectFenRoundTrip("r3k2r/8/8/8/8/8/8/R3K2R w k - 0 1");
}
