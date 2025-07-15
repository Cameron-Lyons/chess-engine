#include "ChessBoard.h"
#include "gtest/gtest.h"

TEST(Crash, Initialization) {
    ASSERT_NO_THROW({
        Board testBoard;
        testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        ASSERT_EQ(testBoard.squares[0].piece.PieceType, ChessPieceType::ROOK);
        std::vector<std::pair<int, int>> moves = GetAllMoves(testBoard, testBoard.turn);
        ASSERT_GE(moves.size(), 16);
    });
}
