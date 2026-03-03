#include "src/core/ChessBoard.h"
#include "src/search/ValidMoves.h"
#include "src/search/search.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace std::chrono;

struct PerftResult {
    uint64_t nodes;
    duration<double> time;
    double nps;
};

class PerftBenchmark {
public:
    static PerftResult runPerft(Board& board, int depth) {
        auto start = high_resolution_clock::now();
        uint64_t nodes = perft(board, depth);
        auto end = high_resolution_clock::now();
        duration<double> time = end - start;
        double nps = static_cast<double>(nodes) / time.count();
        return {nodes, time, nps};
    }

    static uint64_t perft(Board& board, int depth) {
        if (depth == 0) {
            return 1;
        }

        uint64_t nodes = 0;
        GenValidMoves(board);
        std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

        for (const auto& move : moves) {
            Board backup = board;
            if (board.movePiece(move.first, move.second)) {
                nodes += perft(board, depth - 1);
            }
            board = backup;
        }

        return nodes;
    }
};

class SearchBenchmark {
public:
    struct SearchResult {
        int bestMove;
        int score;
        uint64_t nodes;
        duration<double> time;
    };

    static SearchResult runSearchBenchmark(Board& board, int depth, int timeMs) {
        auto start = high_resolution_clock::now();
        auto result = iterativeDeepeningParallel(board, depth, timeMs, 1);
        auto end = high_resolution_clock::now();
        duration<double> time = end - start;

        return {(result.bestMove.first * 8) + result.bestMove.second, result.score,
                static_cast<uint64_t>(result.nodes), time};
    }
};

void runBenchmarkSuite() {
    std::cout << "🚀 Chess Engine Performance Benchmark Suite\n";
    std::cout << "=============================================\n\n";

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    std::vector<std::pair<std::string, std::string>> testPositions = {
        {"Starting Position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
        {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"},
        {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"}};

    struct PerftTaskResult {
        std::size_t positionIndex;
        int depth;
        PerftResult result;
    };
    struct SearchTaskResult {
        std::size_t positionIndex;
        SearchBenchmark::SearchResult result;
    };

    std::cout << "📊 PERFT (Move Generation) Tests:\n";
    std::cout << "Position                 | Depth | Nodes      | Time(s) | NPS        \n";
    std::cout << "-------------------------|-------|------------|---------|------------\n";

    std::vector<std::future<PerftTaskResult>> perftFutures;
    perftFutures.reserve(testPositions.size() * 4U);
    for (std::size_t i = 0; i < testPositions.size(); ++i) {
        for (int depth = 1; depth <= 4; depth++) {
            perftFutures.emplace_back(std::async(std::launch::async, [i, depth, &testPositions]() {
                Board board;
                board.InitializeFromFEN(testPositions[i].second);
                return PerftTaskResult{i, depth, PerftBenchmark::runPerft(board, depth)};
            }));
        }
    }

    std::vector<PerftTaskResult> perftResults;
    perftResults.reserve(perftFutures.size());
    for (auto& future : perftFutures) {
        perftResults.push_back(future.get());
    }
    std::sort(perftResults.begin(), perftResults.end(),
              [](const PerftTaskResult& lhs, const PerftTaskResult& rhs) {
                  return std::tie(lhs.positionIndex, lhs.depth) <
                         std::tie(rhs.positionIndex, rhs.depth);
              });

    for (const auto& row : perftResults) {
        const auto& name = testPositions[row.positionIndex].first;
        std::cout << std::left << std::setw(24) << name.substr(0, 23) << "| " << std::setw(5)
                  << row.depth << "| " << std::setw(10) << row.result.nodes << "| " << std::setw(7)
                  << std::fixed << std::setprecision(3) << row.result.time.count() << "| "
                  << std::setw(10) << std::scientific << std::setprecision(2) << row.result.nps
                  << "\n";
        if (row.depth == 4) {
            std::cout << "-------------------------|-------|------------|---------|------------\n";
        }
    }

    std::cout << "\n🧠 Search Performance Tests:\n";
    std::cout << "Position           | Depth | Score | Nodes    | Time(s) | Best Move\n";
    std::cout << "-------------------|-------|-------|----------|---------|----------\n";

    std::vector<std::future<SearchTaskResult>> searchFutures;
    searchFutures.reserve(testPositions.size());
    for (std::size_t i = 0; i < testPositions.size(); ++i) {
        searchFutures.emplace_back(std::async(std::launch::async, [i, &testPositions]() {
            Board board;
            board.InitializeFromFEN(testPositions[i].second);
            return SearchTaskResult{i, SearchBenchmark::runSearchBenchmark(board, 6, 5000)};
        }));
    }

    std::vector<SearchTaskResult> searchResults;
    searchResults.reserve(searchFutures.size());
    for (auto& future : searchFutures) {
        searchResults.push_back(future.get());
    }
    std::sort(searchResults.begin(), searchResults.end(),
              [](const SearchTaskResult& lhs, const SearchTaskResult& rhs) {
                  return lhs.positionIndex < rhs.positionIndex;
              });

    for (const auto& row : searchResults) {
        const auto& name = testPositions[row.positionIndex].first;
        std::cout << std::left << std::setw(18) << name.substr(0, 17) << "| " << std::setw(5) << 6
                  << "| " << std::setw(5) << row.result.score << "| " << std::setw(8)
                  << row.result.nodes << "| " << std::setw(7) << std::fixed << std::setprecision(3)
                  << row.result.time.count() << "| " << std::setw(8) << row.result.bestMove << "\n";
    }

    std::cout << "\n✅ Benchmark suite completed!\n";
}

int main() {
    try {
        runBenchmarkSuite();
    } catch (const std::exception& e) {
        std::cerr << "❌ Benchmark failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
