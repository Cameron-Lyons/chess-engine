#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/core/ChessPiece.h"
#include "src/core/Move.h"
#include "src/search/search.h"

#include "src/utils/ChessFormat.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 8;
constexpr int kDefaultTimeMs = 100;
constexpr int kDefaultThreads = 1;

bool moveMatches(const Move& lhs, const Move& rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::string defaultFen = BenchmarkSuite::defaultCorpus().front().fen;
    const std::string fen = BenchmarkArgs::parseStringArg(args, "--fen", defaultFen);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int threads = BenchmarkArgs::parsePositiveIntArg(args, "--threads", kDefaultThreads);
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);

    Board board;
    SearchResult result{};
    long long elapsedMs = 0;

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        board.InitializeFromFEN(fen);

        SearchConfig config;
        config.maxDepth = depthLimit;
        config.timeLimitMs = timeMs;
        config.optimalTimeMs = timeMs;
        config.maxTimeMs = timeMs;

        SearchContext searchContext;
        searchContext.threads = threads;

        const auto start = std::chrono::steady_clock::now();
        result = iterativeDeepeningParallel(board, config, searchContext);
        elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
    }

    const std::vector<Move> legalMoves = GetAllMoves(board, board.turn);
    const bool terminal =
        legalMoves.empty() && result.bestMove.first < 0 && result.bestMove.second < 0;
    const bool legal =
        terminal || (result.bestMove.first >= 0 && result.bestMove.second >= 0 &&
                     std::any_of(legalMoves.begin(), legalMoves.end(), [&](const Move& legalMove) {
                         return moveMatches(legalMove, result.bestMove);
                     }));
    std::string nextFen;
    if (legal && !terminal) {
        Board nextBoard = board;
        if (applySearchMove(nextBoard, result.bestMove.first, result.bestMove.second)) {
            nextBoard.turn = (nextBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                        : ChessPieceColor::WHITE;
            nextFen = nextBoard.toFEN();
        } else {
            nextFen = "INVALID";
        }
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        std::cout << "{\n";
        std::cout << "  \"benchmark\": \"search_probe\",\n";
        std::cout << "  \"config\": {\"depth\": " << depthLimit << ", \"time_ms\": " << timeMs
                  << ", \"threads\": " << threads << "},\n";
        std::cout << "  \"result\": {\"fen\": \"" << BenchmarkSuite::jsonEscape(fen)
                  << "\", \"bestmove\": \"" << chess::format::moveToUci(result.bestMove)
                  << "\", \"score\": " << result.score << ", \"depth\": " << result.depth
                  << ", \"nodes\": " << result.nodes << ", \"time_ms\": " << elapsedMs
                  << ", \"legal\": " << (legal ? "true" : "false")
                  << ", \"terminal\": " << (terminal ? "true" : "false") << ", \"next_fen\": \""
                  << BenchmarkSuite::jsonEscape(nextFen) << "\"}\n";
        std::cout << "}\n";
    } else {
        std::cout << "fen=" << fen << '\n';
        std::cout << "bestmove=" << chess::format::moveToUci(result.bestMove) << '\n';
        std::cout << "score=" << result.score << '\n';
        std::cout << "depth=" << result.depth << '\n';
        std::cout << "nodes=" << result.nodes << '\n';
        std::cout << "time_ms=" << elapsedMs << '\n';
        std::cout << "legal=" << (legal ? "true" : "false") << '\n';
        std::cout << "terminal=" << (terminal ? "true" : "false") << '\n';
        if (!terminal) {
            std::cout << "next_fen=" << nextFen << '\n';
        }
    }

    if (!legal) {
        return 2;
    }

    return 0;
}
