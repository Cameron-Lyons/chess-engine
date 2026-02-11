#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"
#include "utils/engine_globals.h"

#include <thread>

TEST(ParallelSearch, Speedup) {
    InitZobrist();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    SearchResult result1 = iterativeDeepeningParallel(board, 4, 5000, 1);

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;

    SearchResult result2 = iterativeDeepeningParallel(board, 4, 5000, numThreads);

    ASSERT_GE(result2.nodes, result1.nodes);
}

TEST(ParallelSearch, ComplexPosition) {
    InitZobrist();

    Board complexBoard;
    complexBoard.InitializeFromFEN(
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;

    SearchResult result = iterativeDeepeningParallel(complexBoard, 5, 3000, numThreads);

    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}
