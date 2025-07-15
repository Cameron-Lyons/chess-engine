#include "../src/ChessBoard.h"
#include "../src/search.h"
#include "../src/BitboardMoves.h"
#include "gtest/gtest.h"

TEST(KingSafety, StartingPosition) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board startBoard;
    startBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int whiteKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::WHITE);
    int blackKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::BLACK);

    ASSERT_GE(whiteKingSafety, 0);
    ASSERT_GE(blackKingSafety, 0);
}

TEST(KingSafety, DamagedPawnShield) {
    Board startBoard;
    startBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int whiteKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::WHITE);

    Board damagedPawnShield;
    damagedPawnShield.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int damagedWhiteKingSafety = evaluateKingSafety(damagedPawnShield, ChessPieceColor::WHITE);

    ASSERT_LT(damagedWhiteKingSafety, whiteKingSafety);
}

TEST(KingSafety, OpenFile) {
    Board startBoard;
    startBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int whiteKingSafety = evaluateKingSafety(startBoard, ChessPieceColor::WHITE);

    Board openFile;
    openFile.InitializeFromFEN("rnbqkbnr/ppp1pppp/8/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1");
    int openFileWhiteKingSafety = evaluateKingSafety(openFile, ChessPieceColor::WHITE);

    ASSERT_LT(openFileWhiteKingSafety, whiteKingSafety);
}
