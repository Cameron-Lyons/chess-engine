#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/core/Move.h"
#include "src/search/search.h"

#include "src/utils/ChessFormat.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 12;
constexpr int kDefaultTimeMs = 2000;
constexpr int kDefaultThreads = 1;
constexpr int kDefaultRounds = 3;

struct SearchRow {
    std::string id;
    std::string positionName;
    std::string fen;
    std::vector<std::string> tags;
    int round = 0;
    int depthReached = 0;
    int score = 0;
    int nodes = 0;
    long long elapsedMs = 0;
    long long nps = 0;
    Move bestMove{SearchConstants::kInvalidSquare, SearchConstants::kInvalidSquare};
};

void printTextReport(const std::vector<SearchRow>& rows, int rounds, int depthLimit, int timeMs,
                     int threads) {
    long long totalNodes = 0;
    long long totalElapsedMs = 0;

    std::cout << "Search benchmark\n";
    std::cout << "rounds=" << rounds << " depth=" << depthLimit << " time_ms=" << timeMs
              << " threads=" << threads << "\n";
    std::cout << "id\tdepth\tscore\tnodes\telapsed_ms\tnps\tbestmove\ttags\n";

    for (const auto& row : rows) {
        totalNodes += row.nodes;
        totalElapsedMs += row.elapsedMs;
        std::cout << row.id << '\t' << row.depthReached << '\t' << row.score << '\t' << row.nodes
                  << '\t' << row.elapsedMs << '\t' << row.nps << '\t'
                  << chess::format::moveToUci(row.bestMove) << '\t'
                  << BenchmarkSuite::joinTags(row.tags, ",") << '\n';
    }

    const long long totalNps = (totalNodes * 1000LL) / std::max(1LL, totalElapsedMs);
    std::cout << "TOTAL\tnodes=" << totalNodes << "\telapsed_ms=" << totalElapsedMs
              << "\tnps=" << totalNps << '\n';
}

void printJsonReport(const std::vector<SearchRow>& rows, int rounds, int depthLimit, int timeMs,
                     int threads) {
    long long totalNodes = 0;
    long long totalElapsedMs = 0;
    for (const auto& row : rows) {
        totalNodes += row.nodes;
        totalElapsedMs += row.elapsedMs;
    }
    const long long totalNps = (totalNodes * 1000LL) / std::max(1LL, totalElapsedMs);

    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"search_benchmark\",\n";
    std::cout << "  \"config\": {\"rounds\": " << rounds << ", \"depth\": " << depthLimit
              << ", \"time_ms\": " << timeMs << ", \"threads\": " << threads << "},\n";
    std::cout << "  \"results\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        std::cout << "    {\"id\": \"" << BenchmarkSuite::jsonEscape(row.id)
                  << "\", \"position_name\": \"" << BenchmarkSuite::jsonEscape(row.positionName)
                  << "\", \"round\": " << row.round << ", \"depth_reached\": " << row.depthReached
                  << ", \"score\": " << row.score << ", \"nodes\": " << row.nodes
                  << ", \"elapsed_ms\": " << row.elapsedMs << ", \"nps\": " << row.nps
                  << ", \"bestmove\": \""
                  << BenchmarkSuite::jsonEscape(chess::format::moveToUci(row.bestMove))
                  << "\", \"fen\": \"" << BenchmarkSuite::jsonEscape(row.fen) << "\", \"tags\": [";
        for (std::size_t tagIndex = 0; tagIndex < row.tags.size(); ++tagIndex) {
            if (tagIndex > 0) {
                std::cout << ", ";
            }
            std::cout << "\"" << BenchmarkSuite::jsonEscape(row.tags[tagIndex]) << "\"";
        }
        std::cout << "]}";
        if (i + 1 < rows.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "  ],\n";
    std::cout << "  \"summary\": {\"nodes\": " << totalNodes
              << ", \"elapsed_ms\": " << totalElapsedMs << ", \"nps\": " << totalNps << "}\n";
    std::cout << "}\n";
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int threads = BenchmarkArgs::parsePositiveIntArg(args, "--threads", kDefaultThreads);
    const int rounds = BenchmarkArgs::parsePositiveIntArg(args, "--rounds", kDefaultRounds);
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);
    const std::vector<BenchmarkSuite::PositionCase> positions =
        BenchmarkSuite::selectPositions(args);

    if (positions.empty()) {
        std::cerr << "No benchmark positions matched the requested filters.\n";
        return 1;
    }

    std::vector<SearchRow> rows;
    rows.reserve(static_cast<std::size_t>(rounds) * positions.size());

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        SearchConfig config;
        config.maxDepth = depthLimit;
        config.timeLimitMs = timeMs;
        config.optimalTimeMs = timeMs;
        config.maxTimeMs = timeMs;

        SearchContext searchContext;
        searchContext.threads = threads;

        for (int round = 1; round <= rounds; ++round) {
            for (const auto& position : positions) {
                Board board;
                board.InitializeFromFEN(position.fen);

                const auto start = std::chrono::steady_clock::now();
                const SearchResult result =
                    iterativeDeepeningParallel(board, config, searchContext);
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - start)
                                           .count();

                SearchRow row;
                row.id = std::format("round{}:{}", round, position.id);
                row.positionName = position.name;
                row.fen = position.fen;
                row.tags = position.tags;
                row.round = round;
                row.depthReached = result.depth;
                row.score = result.score;
                row.nodes = result.nodes;
                row.elapsedMs = elapsedMs;
                row.nps = (result.nodes * 1000LL) / std::max(1LL, elapsedMs);
                row.bestMove = result.bestMove;
                rows.push_back(row);
            }
        }
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        printJsonReport(rows, rounds, depthLimit, timeMs, threads);
    } else {
        printTextReport(rows, rounds, depthLimit, timeMs, threads);
    }

    return 0;
}
