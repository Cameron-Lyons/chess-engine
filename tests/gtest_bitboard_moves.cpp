#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"

TEST(BitboardMoves, InitialPosition) {
    initKnightAttacks();
    initKingAttacks();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    Bitboard whitePawns = board.whitePawns;
    Bitboard empty = ~board.allPieces;
    Bitboard blackPieces = board.blackPieces;

    Bitboard pawnPushesBB = pawnPushes(whitePawns, empty, ChessPieceColor::WHITE);
    Bitboard pawnCapturesBB = pawnCaptures(whitePawns, blackPieces, ChessPieceColor::WHITE);

    ASSERT_EQ(popcount(pawnPushesBB), 16);
    ASSERT_EQ(popcount(pawnCapturesBB), 0);

    Bitboard whiteKnights = board.whiteKnights;
    Bitboard whitePieces = board.whitePieces;

    Bitboard knightMovesBB = knightMoves(whiteKnights, whitePieces);
    ASSERT_EQ(popcount(knightMovesBB), 4);

    Bitboard whiteBishops = board.whiteBishops;
    Bitboard bishopMovesBB = bishopMoves(whiteBishops, whitePieces, board.allPieces);
    ASSERT_EQ(popcount(bishopMovesBB), 0);

    Bitboard whiteRooks = board.whiteRooks;
    Bitboard rookMovesBB = rookMoves(whiteRooks, whitePieces, board.allPieces);
    ASSERT_EQ(popcount(rookMovesBB), 0);

    Bitboard whiteQueens = board.whiteQueens;
    Bitboard queenMovesBB = queenMoves(whiteQueens, whitePieces, board.allPieces);
    ASSERT_EQ(popcount(queenMovesBB), 0);

    Bitboard whiteKings = board.whiteKings;
    Bitboard kingMovesBB = kingMoves(whiteKings, whitePieces);
    ASSERT_EQ(popcount(kingMovesBB), 0);
}

TEST(BitboardMoves, SyncAfterMove) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int e2 = 12;
    int e4 = 28;
    board.movePiece(e2, e4);

    Bitboard expectedWhitePawns = 0ULL;
    expectedWhitePawns |= (1ULL << 8) | (1ULL << 9) | (1ULL << 10) | (1ULL << 11) | (1ULL << 28) |
                          (1ULL << 13) | (1ULL << 14) | (1ULL << 15);
    ASSERT_EQ(board.whitePawns, expectedWhitePawns);

    int b1 = 1;
    int f3 = 21;
    board.movePiece(b1, f3);

    Bitboard expectedWhiteKnights = 0ULL;
    expectedWhiteKnights |= (1ULL << 6) | (1ULL << 21);
    ASSERT_EQ(board.whiteKnights, expectedWhiteKnights);
}
