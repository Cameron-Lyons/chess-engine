#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 8;
constexpr int kDefaultTimeMs = 5000;

struct LazySmpRow {
    std::string id;
    std::string positionName;
    long long regularNodes = 0;
    long long regularElapsedMs = 0;
    long long smpNodes = 0;
    long long smpElapsedMs = 0;
    long long regularNps = 0;
    long long smpNps = 0;
    double speedup = 0.0;
};

void printTextReport(const std::vector<LazySmpRow>& rows, int threads, int depthLimit, int timeMs) {
    std::cout << "Lazy SMP benchmark\n";
    std::cout << "threads=" << threads << " depth=" << depthLimit << " time_ms=" << timeMs << '\n';
    std::cout << "id\tregular_nps\tsmp_nps\tspeedup\n";
    for (const auto& row : rows) {
        std::cout << row.id << '\t' << row.regularNps << '\t' << row.smpNps << '\t' << row.speedup
                  << '\n';
    }
}

void printJsonReport(const std::vector<LazySmpRow>& rows, int threads, int depthLimit, int timeMs) {
    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"lazy_smp_benchmark\",\n";
    std::cout << "  \"config\": {\"threads\": " << threads << ", \"depth\": " << depthLimit
              << ", \"time_ms\": " << timeMs << "},\n";
    std::cout << "  \"results\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        std::cout << "    {\"id\": \"" << BenchmarkSuite::jsonEscape(row.id)
                  << "\", \"position_name\": \"" << BenchmarkSuite::jsonEscape(row.positionName)
                  << "\", \"regular_nps\": " << row.regularNps << ", \"smp_nps\": " << row.smpNps
                  << ", \"speedup\": " << row.speedup << "}";
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
    const int hardwareThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    const int threads =
        BenchmarkArgs::parsePositiveIntArg(args, "--threads", std::max(2, hardwareThreads));
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);
    const std::vector<BenchmarkSuite::PositionCase> positions =
        BenchmarkSuite::selectPositions(args);

    if (positions.empty()) {
        std::cerr << "No benchmark positions matched the requested filters.\n";
        return 1;
    }

    std::vector<LazySmpRow> rows;
    rows.reserve(positions.size());

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        SearchConfig regularConfig;
        regularConfig.maxDepth = depthLimit;
        regularConfig.timeLimitMs = timeMs;
        regularConfig.optimalTimeMs = timeMs;
        regularConfig.maxTimeMs = timeMs;

        SearchConfig smpConfig = regularConfig;

        SearchContext regularContext;
        regularContext.threads = 1;
        SearchContext smpContext;
        smpContext.threads = threads;

        for (const auto& position : positions) {
            LazySmpRow row;
            row.id = position.id;
            row.positionName = position.name;

            Board regularBoard;
            regularBoard.InitializeFromFEN(position.fen);
            auto start = std::chrono::steady_clock::now();
            const SearchResult regularResult =
                iterativeDeepeningParallel(regularBoard, regularConfig, regularContext);
            row.regularElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            row.regularNodes = regularResult.nodes;
            row.regularNps = (row.regularNodes * 1000LL) / std::max(1LL, row.regularElapsedMs);

            Board smpBoard;
            smpBoard.InitializeFromFEN(position.fen);
            start = std::chrono::steady_clock::now();
            const SearchResult smpResult =
                iterativeDeepeningParallel(smpBoard, smpConfig, smpContext);
            row.smpElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
            row.smpNodes = smpResult.nodes;
            row.smpNps = (row.smpNodes * 1000LL) / std::max(1LL, row.smpElapsedMs);
            row.speedup = static_cast<double>(row.regularElapsedMs) /
                          static_cast<double>(std::max(1LL, row.smpElapsedMs));
            rows.push_back(row);
        }
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        printJsonReport(rows, threads, depthLimit, timeMs);
    } else {
        printTextReport(rows, threads, depthLimit, timeMs);
    }

    return 0;
}
