#include "../src/ChessBoard.h"
#include "../src/search.h"
#include "../src/BitboardMoves.h"
#include "gtest/gtest.h"

TEST(QuiescenceSearch, Simple) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int staticScore = evaluatePosition(board);

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 5000;

    GenValidMoves(board);

    int qScore = QuiescenceSearch(board, -10000, 10000, true, historyTable, context);

    ASSERT_EQ(qScore, staticScore);
}
