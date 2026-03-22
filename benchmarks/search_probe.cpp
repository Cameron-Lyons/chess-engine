#include "benchmark_args.h"
#include "src/core/BitboardMoves.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 8;
constexpr int kDefaultTimeMs = 100;
constexpr int kDefaultThreads = 1;
constexpr int kBoardDimension = 8;
constexpr int kInvalidSquare = -1;
constexpr char kFileBase = 'a';
constexpr char kRankBase = '1';

std::string parseStringArg(const std::vector<std::string>& args, const std::string& key,
                           const std::string& fallback) {
    const std::string prefix = key + "=";
    for (const std::string& arg : args) {
        if (arg.compare(0, prefix.size(), prefix) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return fallback;
}

bool moveMatches(const Move& lhs, const Move& rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

std::string moveToUci(const Move& move) {
    if (move.first == kInvalidSquare || move.second == kInvalidSquare) {
        return "0000";
    }

    std::string uci;
    uci += static_cast<char>(kFileBase + (move.first % kBoardDimension));
    uci += static_cast<char>(kRankBase + (move.first / kBoardDimension));
    uci += static_cast<char>(kFileBase + (move.second % kBoardDimension));
    uci += static_cast<char>(kRankBase + (move.second / kBoardDimension));
    return uci;
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::string defaultFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::string fen = parseStringArg(args, "--fen", defaultFen);
    const int depthLimit = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const int timeMs = BenchmarkArgs::parsePositiveIntArg(args, "--time_ms", kDefaultTimeMs);
    const int threads = BenchmarkArgs::parsePositiveIntArg(args, "--threads", kDefaultThreads);

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN(fen);

    SearchConfig config;
    config.maxDepth = depthLimit;
    config.timeLimitMs = timeMs;
    config.optimalTimeMs = timeMs;
    config.maxTimeMs = timeMs;

    SearchContext searchContext;
    searchContext.threads = threads;

    const auto start = std::chrono::steady_clock::now();
    const SearchResult result = iterativeDeepeningParallel(board, config, searchContext);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();

    std::vector<Move> legalMoves = GetAllMoves(board, board.turn);
    const bool terminal =
        legalMoves.empty() && result.bestMove.first < 0 && result.bestMove.second < 0;
    const bool legal =
        terminal || (result.bestMove.first >= 0 && result.bestMove.second >= 0 &&
                     std::any_of(legalMoves.begin(), legalMoves.end(), [&](const Move& legalMove) {
                         return moveMatches(legalMove, result.bestMove);
                     }));

    std::cout << "fen=" << fen << '\n';
    std::cout << "bestmove=" << moveToUci(result.bestMove) << '\n';
    std::cout << "score=" << result.score << '\n';
    std::cout << "depth=" << result.depth << '\n';
    std::cout << "nodes=" << result.nodes << '\n';
    std::cout << "time_ms=" << elapsedMs << '\n';
    std::cout << "legal=" << (legal ? "true" : "false") << '\n';
    std::cout << "terminal=" << (terminal ? "true" : "false") << '\n';

    if (!legal) {
        return 2;
    }

    if (terminal) {
        return 0;
    }

    Board nextBoard = board;
    if (!applySearchMove(nextBoard, result.bestMove.first, result.bestMove.second)) {
        std::cout << "next_fen=INVALID" << '\n';
        return 3;
    }
    nextBoard.turn = (nextBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
    std::cout << "next_fen=" << nextBoard.toFEN() << '\n';

    return 0;
}
