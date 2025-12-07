#include "ChessBoard.h"
#include "gtest/gtest.h"

TEST(Fen, StartingPosition) {
    Board testBoard;
    testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    ASSERT_EQ(testBoard.squares[0].piece.PieceType, ChessPieceType::ROOK);
    ASSERT_EQ(testBoard.squares[0].piece.PieceColor, ChessPieceColor::WHITE);
    ASSERT_EQ(testBoard.squares[63].piece.PieceType, ChessPieceType::ROOK);
    ASSERT_EQ(testBoard.squares[63].piece.PieceColor, ChessPieceColor::BLACK);
    ASSERT_EQ(testBoard.turn, ChessPieceColor::WHITE);
}

TEST(Fen, ComplexPosition) {
    Board testBoard;
    testBoard.InitializeFromFEN(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    ASSERT_EQ(testBoard.squares[4].piece.PieceType, ChessPieceType::KING);
    ASSERT_EQ(testBoard.squares[4].piece.PieceColor, ChessPieceColor::WHITE);
    ASSERT_EQ(testBoard.squares[60].piece.PieceType, ChessPieceType::KING);
    ASSERT_EQ(testBoard.squares[60].piece.PieceColor, ChessPieceColor::BLACK);
    ASSERT_EQ(testBoard.turn, ChessPieceColor::WHITE);
    ASSERT_TRUE(testBoard.whiteKingsideCastle);
    ASSERT_TRUE(testBoard.whiteQueensideCastle);
    ASSERT_TRUE(testBoard.blackKingsideCastle);
    ASSERT_TRUE(testBoard.blackQueensideCastle);
}
