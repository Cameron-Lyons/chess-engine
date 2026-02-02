#include "ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

TEST(PawnMoves, StartingPosition) {
    Board testBoard;
    testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    std::vector<std::pair<int, int>> allMoves = GetAllMoves(testBoard, ChessPieceColor::WHITE);

    int pawnMoveCount = 0;
    for (const auto& move : allMoves) {
        const Piece& piece = testBoard.squares[move.first].piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            pawnMoveCount++;
        }
    }

    ASSERT_EQ(pawnMoveCount, 16);
}

TEST(PawnMoves, Promotion) {
    Board testBoard;
    testBoard.InitializeFromFEN("rnbqkb1r/ppppp2p/5n2/5Pp1/8/8/PPPPPP1P/RNBQKBNR w KQkq g6 0 4");

    testBoard.squares[53].piece = Piece(ChessPieceColor::WHITE, ChessPieceType::PAWN);
    testBoard.squares[61].piece = Piece();
    testBoard.updateBitboards();

    std::vector<std::pair<int, int>> promotionMoves =
        GetAllMoves(testBoard, ChessPieceColor::WHITE);

    bool foundPromotionMove = false;
    for (const auto& move : promotionMoves) {
        if (move.first == 53 && move.second == 61) {
            foundPromotionMove = true;
            break;
        }
    }

    ASSERT_TRUE(foundPromotionMove);
}
