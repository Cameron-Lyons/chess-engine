#include "src/core/ChessBoard.h"
#include "src/search/search.h"
#include "src/utils/engine_globals.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

namespace {
constexpr int kThreadFallback = 4;

int getWorkerCount(std::size_t taskCount, int perTaskThreads = 1) {
    if (taskCount == 0U) {
        return 1;
    }
    const unsigned int hardware = std::thread::hardware_concurrency();
    const int available = (hardware == 0U) ? kThreadFallback : static_cast<int>(hardware);
    const int boundedByCpu = std::max(1, available / std::max(1, perTaskThreads));
    return std::max(1, std::min(static_cast<int>(taskCount), boundedByCpu));
}
} // namespace

int main() {
    InitZobrist();

    std::vector<std::string> testPositions = {

        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",

        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",

        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;

    std::cout << "Lazy SMP Benchmark" << std::endl;
    std::cout << "==================" << std::endl;
    std::cout << "Threads available: " << numThreads << std::endl;
    std::cout << std::endl;

    struct TimedSearch {
        SearchResult result;
        long long timeMs = 0;
    };
    std::vector<TimedSearch> regularResults(testPositions.size());
    std::vector<TimedSearch> smpResults(testPositions.size());

    auto runRegularSearch = [&testPositions, &regularResults](std::size_t index) {
        Board board;
        board.InitializeFromFEN(testPositions[index]);
        auto start = std::chrono::high_resolution_clock::now();
        SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1);
        auto end = std::chrono::high_resolution_clock::now();
        regularResults[index] = {
            result,
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()};
    };

    auto runSMPSearch = [&testPositions, &smpResults, numThreads](std::size_t index) {
        Board board;
        board.InitializeFromFEN(testPositions[index]);
        auto start = std::chrono::high_resolution_clock::now();
        SearchResult result = iterativeDeepeningParallel(board, 8, 5000, numThreads);
        auto end = std::chrono::high_resolution_clock::now();
        smpResults[index] = {
            result,
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()};
    };

    const int regularWorkers = getWorkerCount(testPositions.size(), 1);
    const int smpWorkers = getWorkerCount(testPositions.size(), numThreads);

    auto runStage = [&testPositions](int workerCount, auto&& task) {
        std::atomic<std::size_t> nextIndex{0};
        std::vector<std::future<void>> workers;
        workers.reserve(static_cast<std::size_t>(workerCount));
        for (int worker = 0; worker < workerCount; ++worker) {
            workers.emplace_back(std::async(std::launch::async, [&nextIndex, &testPositions, &task] {
                while (true) {
                    const std::size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (index >= testPositions.size()) {
                        break;
                    }
                    task(index);
                }
            }));
        }
        for (auto& future : workers) {
            future.get();
        }
    };

    runStage(regularWorkers, runRegularSearch);
    runStage(smpWorkers, runSMPSearch);

    for (std::size_t i = 0; i < testPositions.size(); ++i) {
        const auto& regular = regularResults[i];
        const auto& smp = smpResults[i];
        std::cout << "Position: " << testPositions[i] << std::endl;
        std::cout << "Regular search (1 thread):" << std::endl;
        std::cout << "  Depth: " << regular.result.depth << std::endl;
        std::cout << "  Score: " << regular.result.score << std::endl;
        std::cout << "  Nodes: " << regular.result.nodes << std::endl;
        std::cout << "  Time: " << regular.timeMs << " ms" << std::endl;
        std::cout << "  NPS: " << (regular.result.nodes * 1000 / std::max(1LL, regular.timeMs))
                  << std::endl;
        std::cout << "Lazy SMP (" << numThreads << " threads):" << std::endl;
        std::cout << "  Depth: " << smp.result.depth << std::endl;
        std::cout << "  Score: " << smp.result.score << std::endl;
        std::cout << "  Nodes: " << smp.result.nodes << std::endl;
        std::cout << "  Time: " << smp.timeMs << " ms" << std::endl;
        std::cout << "  NPS: " << (smp.result.nodes * 1000 / std::max(1LL, smp.timeMs))
                  << std::endl;
        double speedup =
            static_cast<double>(regular.timeMs) / static_cast<double>(std::max(1LL, smp.timeMs));
        double efficiency = speedup / numThreads * 100.0;
        std::cout << "Speedup: " << speedup << "x" << std::endl;
        std::cout << "Efficiency: " << efficiency << "%" << std::endl;
        std::cout << std::endl;
    }

    return 0;
}
