#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 10;
constexpr int kDefaultTimeMs = 1200;
constexpr int kDefaultRounds = 2;
constexpr int kDefaultMultiPV = 3;

struct BenchRow {
    std::string id;
    int threads = 0;
    long long nodes = 0;
    long long elapsedMs = 0;
    long long nps = 0;
    double speedupVsOne = 0.0;
};

void printTextReport(const std::vector<BenchRow>& rows, int rounds, int depthLimit, int timeMs,
                     int multiPV) {
    std::cout << "MultiPV threaded benchmark\n";
    std::cout << "rounds=" << rounds << " depth=" << depthLimit << " time_ms=" << timeMs
              << " multipv=" << multiPV << '\n';
    std::cout << "id\tthreads\tnodes\telapsed_ms\tnps\tspeedup_vs_1\n";
    for (const auto& row : rows) {
        std::cout << row.id << '\t' << row.threads << '\t' << row.nodes << '\t' << row.elapsedMs
                  << '\t' << row.nps << '\t' << row.speedupVsOne << '\n';
    }
}

void printJsonReport(const std::vector<BenchRow>& rows, int rounds, int depthLimit, int timeMs,
                     int multiPV) {
    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"multipv_benchmark\",\n";
    std::cout << "  \"config\": {\"rounds\": " << rounds << ", \"depth\": " << depthLimit
              << ", \"time_ms\": " << timeMs << ", \"multipv\": " << multiPV << "},\n";
    std::cout << "  \"results\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        std::cout << "    {\"id\": \"" << BenchmarkSuite::jsonEscape(row.id)
                  << "\", \"threads\": " << row.threads << ", \"nodes\": " << row.nodes
                  << ", \"elapsed_ms\": " << row.elapsedMs << ", \"nps\": " << row.nps
                  << ", \"speedup_vs_1\": " << row.speedupVsOne << "}";
        if (i + 1 < rows.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int rounds = BenchmarkArgs::parsePositiveIntArg(args, "--rounds", kDefaultRounds);
    const int multiPV =
        std::max(2, BenchmarkArgs::parsePositiveIntArg(args, "--multipv", kDefaultMultiPV));
    std::vector<int> threadCandidates =
        BenchmarkArgs::parsePositiveIntListArg(args, "--thread_candidates");
    if (threadCandidates.empty()) {
        threadCandidates = {1, 2, 4, 8};
    }
    const int hardwareThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);
    const std::vector<BenchmarkSuite::PositionCase> positions =
        BenchmarkSuite::selectPositions(args);

    if (positions.empty()) {
        std::cerr << "No benchmark positions matched the requested filters.\n";
        return 1;
    }

    std::vector<BenchRow> rows;
    rows.reserve(threadCandidates.size());

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        for (int candidateThreads : threadCandidates) {
            if (candidateThreads > hardwareThreads) {
                continue;
            }

            SearchConfig config;
            config.maxDepth = depthLimit;
            config.timeLimitMs = timeMs;
            config.multiPV = multiPV;
            config.optimalTimeMs = timeMs;
            config.maxTimeMs = timeMs;

            SearchContext searchContext;
            searchContext.threads = candidateThreads;

            BenchRow row;
            row.id = "threads:" + std::to_string(candidateThreads);
            row.threads = candidateThreads;
            for (int round = 0; round < rounds; ++round) {
                for (const auto& position : positions) {
                    Board board;
                    board.InitializeFromFEN(position.fen);

                    const auto start = std::chrono::steady_clock::now();
                    const SearchResult result =
                        iterativeDeepeningParallel(board, config, searchContext);
                    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - start)
                                               .count();
                    row.nodes += result.nodes;
                    row.elapsedMs += elapsedMs;
                }
            }
            row.nps = (row.nodes * 1000LL) / std::max(1LL, row.elapsedMs);
            rows.push_back(row);
        }
    }

    const auto baseIt = std::find_if(rows.begin(), rows.end(),
                                     [](const BenchRow& row) { return row.threads == 1; });
    const long long baseNps = (baseIt != rows.end()) ? baseIt->nps : 0LL;
    for (auto& row : rows) {
        row.speedupVsOne =
            (baseNps > 0) ? static_cast<double>(row.nps) / static_cast<double>(baseNps) : 0.0;
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        printJsonReport(rows, rounds, depthLimit, timeMs, multiPV);
    } else {
        printTextReport(rows, rounds, depthLimit, timeMs, multiPV);
    }

    return 0;
}
