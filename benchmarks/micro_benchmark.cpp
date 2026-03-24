#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/search/AdvancedSearch.h"
#include "src/search/search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultIterations = 2000000;

struct MicroRow {
    std::string id;
    long long elapsedMs = 0;
};

long long benchNullMovePruning(const std::vector<Board>& boards, int iterations) {
    volatile int sink = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const Board& b = boards[static_cast<std::size_t>(i) % boards.size()];
        sink += AdvancedSearch::nullMovePruning(b, 5, -32000, 32000) ? 1 : 0;
    }
    const auto end = std::chrono::steady_clock::now();
    if (sink == -1) {
        std::cerr << "unreachable\n";
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

long long benchGamePhase(const std::vector<Board>& boards, int iterations) {
    volatile int sink = 0;
    TimeManager::TimeControl tc{300000, 0, 40, false};
    TimeManager manager(tc);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const Board& b = boards[static_cast<std::size_t>(i) % boards.size()];
        sink += static_cast<int>(manager.getGamePhase(b));
    }
    const auto end = std::chrono::steady_clock::now();
    if (sink == -1) {
        std::cerr << "unreachable\n";
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

long long benchBookKeyPath(const std::vector<Board>& boards, int iterations) {
    volatile int sink = 0;
    EnhancedOpeningBook openingBook("/tmp/nonexistent_opening_book.bin");
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) {
        const Board& b = boards[static_cast<std::size_t>(i) % boards.size()];
        sink += openingBook.isInBook(b) ? 1 : 0;
    }
    const auto end = std::chrono::steady_clock::now();
    if (sink == -1) {
        std::cerr << "unreachable\n";
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

void printTextReport(const std::vector<MicroRow>& rows, int iterations, std::size_t corpusSize) {
    long long totalMs = 0;
    std::cout << "Micro benchmark\n";
    std::cout << "iterations=" << iterations << " corpus_size=" << corpusSize << '\n';
    std::cout << "id\telapsed_ms\n";
    for (const auto& row : rows) {
        totalMs += row.elapsedMs;
        std::cout << row.id << '\t' << row.elapsedMs << '\n';
    }
    std::cout << "TOTAL\ttotal_ms=" << totalMs << '\n';
}

void printJsonReport(const std::vector<MicroRow>& rows, int iterations, std::size_t corpusSize) {
    long long totalMs = 0;
    for (const auto& row : rows) {
        totalMs += row.elapsedMs;
    }
    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"micro_benchmark\",\n";
    std::cout << "  \"config\": {\"iterations\": " << iterations
              << ", \"corpus_size\": " << corpusSize << "},\n";
    std::cout << "  \"results\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::cout << "    {\"id\": \"" << BenchmarkSuite::jsonEscape(rows[i].id)
                  << "\", \"elapsed_ms\": " << rows[i].elapsedMs << "}";
        if (i + 1 < rows.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "  ],\n";
    std::cout << "  \"summary\": {\"total_ms\": " << totalMs << "}\n";
    std::cout << "}\n";
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int iterations =
        BenchmarkArgs::parsePositiveIntArg(args, "--iterations", kDefaultIterations);
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);
    const std::vector<BenchmarkSuite::PositionCase> positions =
        BenchmarkSuite::selectPositions(args);

    if (positions.empty()) {
        std::cerr << "No benchmark positions matched the requested filters.\n";
        return 1;
    }

    std::vector<Board> boards;
    std::vector<MicroRow> rows;

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        boards.reserve(positions.size());
        for (const auto& position : positions) {
            Board board;
            board.InitializeFromFEN(position.fen);
            boards.push_back(board);
        }

        rows = {
            {"nullMovePruning", benchNullMovePruning(boards, iterations)},
            {"getGamePhase", benchGamePhase(boards, iterations)},
            {"bookKeyPath", benchBookKeyPath(boards, iterations)},
        };
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        printJsonReport(rows, iterations, positions.size());
    } else {
        printTextReport(rows, iterations, positions.size());
    }

    return 0;
}
