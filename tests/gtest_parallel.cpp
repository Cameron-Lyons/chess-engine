#include "../src/core/ChessBoard.h"
#include "../src/search/search.h"
#include "../src/utils/engine_globals.h"
#include "gtest/gtest.h"
#include <thread>

TEST(ParallelSearch, Speedup) {
    InitZobrist();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    auto start1 = std::chrono::steady_clock::now();
    SearchResult result1 = iterativeDeepeningParallel(board, 4, 5000, 1);
    auto end1 = std::chrono::steady_clock::now();
    auto time1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    auto start2 = std::chrono::steady_clock::now();
    SearchResult result2 = iterativeDeepeningParallel(board, 4, 5000, numThreads);
    auto end2 = std::chrono::steady_clock::now();
    auto time2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);

    ASSERT_GT(time1.count(), time2.count());
}

TEST(ParallelSearch, ComplexPosition) {
    InitZobrist();

    Board complexBoard;
    complexBoard.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    SearchResult result = iterativeDeepeningParallel(complexBoard, 5, 3000, numThreads);

    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}
