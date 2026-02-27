#include "src/core/ChessBoard.h"
#include "src/search/search.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <string>
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

void testPosition(const std::string& fen, const std::string& description, int depth = 8) {
    std::cout << "\n=== Testing: " << description << " ===" << '\n';
    std::cout << "FEN: " << fen << '\n';
    Board board;
    board.InitializeFromFEN(fen);
    auto start = std::chrono::high_resolution_clock::now();
    SearchResult result = iterativeDeepeningParallel(board, depth, 5000, 4);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int fromSquare = result.bestMove.first;
    int toSquare = result.bestMove.second;
    char fromFile = 'a' + (fromSquare % 8);
    char fromRank = '1' + (fromSquare / 8);
    char toFile = 'a' + (toSquare % 8);
    char toRank = '1' + (toSquare / 8);
    std::cout << "Best move: " << fromFile << fromRank << toFile << toRank << '\n';
    std::cout << "Score: " << result.score << " (centipawns)" << '\n';
    std::cout << "Depth reached: " << result.depth << '\n';
    std::cout << "Nodes searched: " << result.nodes << '\n';
    std::cout << "Time taken: " << duration.count() << " ms" << '\n';
    std::cout << "NPS: " << (result.nodes * 1000 / std::max<long long>(1LL, duration.count()))
              << '\n';
}

int main() {
    std::cout << "Chess Engine Performance Test - Search Improvements\n";
    std::cout << "====================================================\n";

    testPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "Starting position",
                 10);

    testPosition("r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
                 "Italian Game position", 10);

    testPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                 "Tactical position (Perft position 2)", 10);

    testPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", "Endgame position", 12);

    testPosition("r2q1rk1/ppp2ppp/2n1bn2/2bpp3/3P4/2N1PN2/PPP1BPPP/R1BQK2R w KQ - 0 8",
                 "Complex middlegame", 10);

    testPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                 "Position with promotion threat", 10);

    std::cout << "\n=== Benchmark Summary ===" << '\n';
    std::vector<std::string> benchmarkFens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    long long totalNodes = 0;
    long long totalTime = 0;
    constexpr int benchmarkSearchThreads = 4;
    struct BenchmarkResult {
        long long nodes = 0;
        long long timeMs = 0;
    };
    std::vector<BenchmarkResult> benchmarkResults(benchmarkFens.size());

    const int workerCount = getWorkerCount(benchmarkFens.size(), benchmarkSearchThreads);
    std::atomic<std::size_t> nextIndex{0};
    std::vector<std::future<void>> workers;
    workers.reserve(static_cast<std::size_t>(workerCount));
    for (int worker = 0; worker < workerCount; ++worker) {
        workers.emplace_back(std::async(std::launch::async, [&benchmarkFens, &benchmarkResults, &nextIndex] {
            while (true) {
                const std::size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
                if (index >= benchmarkFens.size()) {
                    break;
                }
                Board board;
                board.InitializeFromFEN(benchmarkFens[index]);
                auto start = std::chrono::high_resolution_clock::now();
                SearchResult result = iterativeDeepeningParallel(board, 8, 2000, benchmarkSearchThreads);
                auto end = std::chrono::high_resolution_clock::now();
                benchmarkResults[index] = {
                    result.nodes,
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()};
            }
        }));
    }
    for (auto& future : workers) {
        future.get();
    }

    for (const auto& result : benchmarkResults) {
        totalNodes += result.nodes;
        totalTime += result.timeMs;
    }

    std::cout << "Total nodes searched: " << totalNodes << '\n';
    std::cout << "Total time: " << totalTime << " ms" << '\n';
    std::cout << "Average NPS: " << (totalNodes * 1000 / std::max<long long>(1LL, totalTime))
              << '\n';

    std::cout << "\n=== Search Improvements Summary ===" << '\n';
    std::cout << "✓ Improved transposition table replacement strategy" << '\n';
    std::cout << "✓ Singular extensions for critical moves" << '\n';
    std::cout << "✓ Futility pruning in non-PV nodes" << '\n';
    std::cout << "✓ Enhanced move ordering with history heuristic" << '\n';
    std::cout << "✓ Aspiration windows in iterative deepening" << '\n';
    return 0;
}
