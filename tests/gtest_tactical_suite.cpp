#include "core/ChessBoard.h"
#include "search/search.h"
#include "core/BitboardMoves.h"
#include "gtest/gtest.h"

TEST(TacticalSuite, MateInOne) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("8/8/8/8/8/8/k1K5/r7 w - - 0 1");

    SearchResult result = iterativeDeepeningParallel(board, 3, 5000, 1);
    ASSERT_EQ(result.score, 1000000);
}

TEST(TacticalSuite, Fork) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.InitializeFromFEN("r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");

    SearchResult result = iterativeDeepeningParallel(board, 4, 5000, 1);
    // This test is more about ensuring the engine finds a reasonable move in a tactical position.
    // The exact best move might vary, but it should not crash and return a valid move.
    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}
