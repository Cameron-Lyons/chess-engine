#include "ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

TEST(Crash, Initialization) {
    Board testBoard;
    testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    ASSERT_EQ(testBoard.squares[0].piece.PieceType, ChessPieceType::ROOK);
    auto moves = GetAllMoves(testBoard, testBoard.turn);
    ASSERT_GE(moves.size(), 16u);
}
