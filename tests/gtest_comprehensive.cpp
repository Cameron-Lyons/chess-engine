#include "ChessBoard.h"
#include "BitboardMoves.h"
#include "gtest/gtest.h"

std::vector<std::pair<int, int>> generateBitboardMoves(Board& board, ChessPieceColor color);

TEST(Comprehensive, BasicMoveGeneration) {
    initKnightAttacks();
    initKingAttacks();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto whiteMoves = generateBitboardMoves(board, ChessPieceColor::WHITE);
    ASSERT_EQ(whiteMoves.size(), 20);
}

TEST(Comprehensive, CheckDetection) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    board.movePiece(1, 18); // Nb1c3
    board.movePiece(52, 36); // e5e4
    board.movePiece(18, 28); // Nc3e4
    board.movePiece(60, 52); // Ke8e7
    board.movePiece(28, 45); // Ne4d6 (check)

    ASSERT_TRUE(isKingInCheck(board, ChessPieceColor::BLACK));
}

TEST(Comprehensive, BitboardSync) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    board.movePiece(12, 28); // e2e4

    bool pawnOnE4 = board.squares[28].piece.PieceType == ChessPieceType::PAWN;
    bool pawnBitboardOnE4 = board.whitePawns & (1ULL << 28);
    bool noPawnOnE2 = board.squares[12].piece.PieceType == ChessPieceType::NONE;
    bool noPawnBitboardOnE2 = !(board.whitePawns & (1ULL << 12));

    ASSERT_TRUE(pawnOnE4);
    ASSERT_TRUE(pawnBitboardOnE4);
    ASSERT_TRUE(noPawnOnE2);
    ASSERT_TRUE(noPawnBitboardOnE2);
}
