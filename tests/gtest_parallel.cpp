#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"
#include "utils/engine_globals.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace {
int getThreadCount() {
    const auto hardwareThreads = std::thread::hardware_concurrency();
    return (hardwareThreads == 0U) ? 4 : static_cast<int>(hardwareThreads);
}

SearchResult runSearch(Board& board, int depth, int timeMs, int threads = 1, int contempt = 0,
                       int multiPV = 1, int optimalTime = 0, int maxTime = 0) {
    SearchConfig config;
    config.maxDepth = depth;
    config.timeLimitMs = timeMs;
    config.contempt = contempt;
    config.multiPV = multiPV;
    config.optimalTimeMs = optimalTime;
    config.maxTimeMs = maxTime;
    SearchContext context;
    context.threads = threads;
    return iterativeDeepeningParallel(board, config, context);
}

bool isSearchMoveLegal(const Board& board, const Move& move) {
    if (move.first < 0 || move.first >= 64 || move.second < 0 || move.second >= 64 ||
        move.first == move.second) {
        return false;
    }
    Board testBoard = board;
    if (!applySearchMove(testBoard, move.first, move.second)) {
        return false;
    }
    return !isInCheck(testBoard, board.turn);
}

std::vector<int> getThreadOptionsForHardware() {
    const int hardwareThreads = getThreadCount();
    std::vector<int> options = {1, std::min(2, hardwareThreads), std::min(4, hardwareThreads)};
    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());
    return options;
}
} // namespace

TEST(ParallelSearch, Speedup) {
    InitZobrist();
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    SearchResult result1 = runSearch(board, 4, 5000, 1);
    int numThreads = getThreadCount();
    SearchResult result2 = runSearch(board, 4, 5000, numThreads);
    ASSERT_GE(result2.nodes, result1.nodes);
}

TEST(ParallelSearch, ComplexPosition) {
    InitZobrist();
    Board complexBoard;
    complexBoard.InitializeFromFEN(
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");

    int numThreads = getThreadCount();
    SearchResult result = runSearch(complexBoard, 5, 3000, numThreads);
    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}

TEST(ParallelSearch, MultiPVThreadedSearchReturnsMove) {
    InitZobrist();
    Board board;
    board.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");

    int numThreads = std::max(2, getThreadCount());
    SearchResult result = runSearch(board, 5, 3000, numThreads, 0, 3, 3000, 3000);
    ASSERT_GE(result.bestMove.first, 0);
    ASSERT_GE(result.bestMove.second, 0);
    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}

TEST(ParallelSearch, RegressionAcrossThreadAndMultiPVMatrix) {
    InitZobrist();
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    const std::vector<int> threadOptions = getThreadOptionsForHardware();
    const std::vector<int> multiPvOptions = {1, 2, 3};

    for (const int threads : threadOptions) {
        for (const int multiPv : multiPvOptions) {
            Board searchBoard = board;
            SearchResult result = runSearch(searchBoard, 3, 1500, threads, 0, multiPv, 1500, 1500);
            ASSERT_TRUE(isSearchMoveLegal(board, result.bestMove))
                << "illegal move for threads=" << threads << " multipv=" << multiPv;
        }
    }
}

TEST(ParallelSearch, DeterministicAcrossRepeatedRuns) {
    InitZobrist();
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    const std::vector<int> threadOptions = getThreadOptionsForHardware();
    const std::vector<int> multiPvOptions = {1, 3};

    for (const int threads : threadOptions) {
        for (const int multiPv : multiPvOptions) {
            Board runOne = board;
            Board runTwo = board;
            SearchResult first = runSearch(runOne, 3, 1800, threads, 0, multiPv, 1800, 1800);
            SearchResult second = runSearch(runTwo, 3, 1800, threads, 0, multiPv, 1800, 1800);
            ASSERT_EQ(first.bestMove, second.bestMove)
                << "non-deterministic best move for threads=" << threads << " multipv=" << multiPv;
        }
    }
}
