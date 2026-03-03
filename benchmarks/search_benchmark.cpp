#include "src/core/BitboardMoves.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 12;
constexpr int kDefaultTimeMs = 2000;
constexpr int kDefaultThreads = 1;
constexpr int kDefaultRounds = 3;

int parseIntArg(const std::vector<std::string>& args, const std::string& key, int fallback) {
    const std::string prefix = key + "=";
    for (const std::string& arg : args) {
        if (arg.starts_with(prefix)) {
            const std::string value = arg.substr(prefix.size());
            char* end = nullptr;
            errno = 0;
            const long parsed = std::strtol(value.c_str(), &end, 10);
            if (errno == 0 && end != value.c_str() && *end == '\0' && parsed > 0 &&
                parsed <= std::numeric_limits<int>::max()) {
                return static_cast<int>(parsed);
            }
        }
    }
    return fallback;
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int depthLimit = parseIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = parseIntArg(args, "--time_ms", kDefaultTimeMs);
    const int threads = parseIntArg(args, "--threads", kDefaultThreads);
    const int rounds = parseIntArg(args, "--rounds", kDefaultRounds);

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
