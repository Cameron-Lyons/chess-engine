#include "src/core/BitboardMoves.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"
#include "benchmark_args.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 12;
constexpr int kDefaultTimeMs = 2000;
constexpr int kDefaultThreads = 1;
constexpr int kDefaultRounds = 3;
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int threads = BenchmarkArgs::parsePositiveIntArg(args, "--threads", kDefaultThreads);
    const int rounds = BenchmarkArgs::parsePositiveIntArg(args, "--rounds", kDefaultRounds);

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    std::cout << "Search benchmark" << '\n';
    std::cout << "rounds=" << rounds << " depth=" << depthLimit << " time_ms=" << timeMs
              << " threads=" << threads << '\n';

    long long totalNodes = 0;
    long long totalElapsedMs = 0;

    for (int round = 1; round <= rounds; ++round) {
        std::cout << "round=" << round << '\n';
        for (std::size_t i = 0; i < fens.size(); ++i) {
            Board board;
            board.InitializeFromFEN(fens[i]);

            const auto start = std::chrono::steady_clock::now();
            const SearchResult result =
                iterativeDeepeningParallel(board, depthLimit, timeMs, threads);
            const auto end = std::chrono::steady_clock::now();
            const auto elapsedMs =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            const long long nps = (result.nodes * 1000LL) / std::max(1LL, elapsedMs);

            totalNodes += result.nodes;
            totalElapsedMs += elapsedMs;

            std::cout << "pos=" << (i + 1) << " depth_reached=" << result.depth
                      << " nodes=" << result.nodes << " elapsed_ms=" << elapsedMs << " nps=" << nps
                      << '\n';
        }
    }

    const long long totalNps = (totalNodes * 1000LL) / std::max(1LL, totalElapsedMs);
    std::cout << "TOTAL nodes=" << totalNodes << " elapsed_ms=" << totalElapsedMs
              << " nps=" << totalNps << '\n';

    return 0;
}
