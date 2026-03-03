#include "src/core/BitboardMoves.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"
#include "benchmark_args.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 10;
constexpr int kDefaultTimeMs = 1200;
constexpr int kDefaultRounds = 2;
constexpr int kDefaultMultiPV = 3;
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int rounds = BenchmarkArgs::parsePositiveIntArg(args, "--rounds", kDefaultRounds);
    const int multiPV =
        std::max(2, BenchmarkArgs::parsePositiveIntArg(args, "--multipv", kDefaultMultiPV));
    const int hardwareThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    const std::array<int, 4> threadCandidates = {1, 2, 4, 8};

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    std::cout << "MultiPV threaded benchmark" << '\n';
    std::cout << "rounds=" << rounds << " depth=" << depthLimit << " time_ms=" << timeMs
              << " multipv=" << multiPV << " hardware_threads=" << hardwareThreads << '\n';

    struct BenchRow {
        int threads = 0;
        long long nodes = 0;
        long long elapsedMs = 0;
    };

    std::vector<BenchRow> rows;
    rows.reserve(threadCandidates.size());

    for (int candidateThreads : threadCandidates) {
        if (candidateThreads > hardwareThreads) {
            std::cout << "threads=" << candidateThreads << " skipped (hardware limit)" << '\n';
            continue;
        }

        BenchRow row;
        row.threads = candidateThreads;
        for (int round = 0; round < rounds; ++round) {
            for (const auto& fen : fens) {
                Board board;
                board.InitializeFromFEN(fen);

                const auto start = std::chrono::steady_clock::now();
                const SearchResult result =
                    iterativeDeepeningParallel(board, depthLimit, timeMs, candidateThreads, 0, multiPV,
                                               timeMs, timeMs);
                const auto end = std::chrono::steady_clock::now();
                const auto elapsedMs =
                    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

                row.nodes += result.nodes;
                row.elapsedMs += elapsedMs;
            }
        }
        rows.push_back(row);
    }

    if (rows.empty()) {
        std::cout << "No benchmark rows executed." << '\n';
        return 0;
    }

    const auto baseIt = std::find_if(rows.begin(), rows.end(),
                                     [](const BenchRow& row) { return row.threads == 1; });
    const long long baseNps =
        (baseIt != rows.end()) ? (baseIt->nodes * 1000LL) / std::max(1LL, baseIt->elapsedMs) : 0LL;

    std::cout << "threads\tnodes\telapsed_ms\tnps\tspeedup_vs_1" << '\n';
    for (const auto& row : rows) {
        const long long nps = (row.nodes * 1000LL) / std::max(1LL, row.elapsedMs);
        const double speedup = (baseNps > 0) ? static_cast<double>(nps) / static_cast<double>(baseNps)
                                             : 0.0;
        std::cout << row.threads << '\t' << row.nodes << '\t' << row.elapsedMs << '\t' << nps
                  << '\t' << speedup << '\n';
    }

    return 0;
}
