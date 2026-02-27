#include "ChessBoard.h"
#include "core/BitboardMoves.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"

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
    board.InitializeFromFEN("rnbqkb1r/pppppppp/5N2/8/8/8/PPPPPPPP/R1BQKBNR b KQkq - 0 1");
    ASSERT_TRUE(IsKingInCheck(board, ChessPieceColor::BLACK));
}

TEST(Comprehensive, BitboardSync) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.movePiece(12, 28);
    bool pawnOnE4 = board.squares[28].piece.PieceType == ChessPieceType::PAWN;
    bool pawnBitboardOnE4 = board.whitePawns & (1ULL << 28);
    bool noPawnOnE2 = board.squares[12].piece.PieceType == ChessPieceType::NONE;
    bool noPawnBitboardOnE2 = !(board.whitePawns & (1ULL << 12));
    ASSERT_TRUE(pawnOnE4);
    ASSERT_TRUE(pawnBitboardOnE4);
    ASSERT_TRUE(noPawnOnE2);
    ASSERT_TRUE(noPawnBitboardOnE2);
}

TEST(Comprehensive, KingsideCastleLegalInOpenPosition) {
    initKnightAttacks();
    initKingAttacks();

    Board board;
    board.InitializeFromFEN("r1b1k2r/pppp1ppp/2nbpn2/8/1q1PP3/1BN2N2/PPP2PPP/R1BQK2R w KQkq - 0 1");

    EXPECT_TRUE(IsMoveLegal(board, 4, 6));
}
