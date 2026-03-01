#include "src/core/BitboardMoves.h"
#include "src/core/ChessBoard.h"
#include "src/search/AdvancedSearch.h"
#include "src/search/search.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultIterations = 2000000;

int parseIntArg(int argc, char** argv, const std::string& key, int fallback) {
    const std::string prefix = key + "=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg.rfind(prefix, 0) == 0) {
            return std::max(1, std::atoi(arg.substr(prefix.size()).c_str()));
        }
    }
    return fallback;
}

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
} // namespace

int main(int argc, char** argv) {
    const int iterations = parseIntArg(argc, argv, "--iterations", kDefaultIterations);

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    const std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    std::vector<Board> boards;
    boards.reserve(fens.size());
    for (const std::string& fen : fens) {
        Board board;
        board.InitializeFromFEN(fen);
        boards.push_back(board);
    }

    const long long nullMoveMs = benchNullMovePruning(boards, iterations);
    const long long gamePhaseMs = benchGamePhase(boards, iterations);
    const long long bookKeyMs = benchBookKeyPath(boards, iterations);

    std::cout << "Micro benchmark" << '\n';
    std::cout << "iterations=" << iterations << '\n';
    std::cout << "nullMovePruning_ms=" << nullMoveMs << '\n';
    std::cout << "getGamePhase_ms=" << gamePhaseMs << '\n';
    std::cout << "bookKeyPath_ms=" << bookKeyMs << '\n';
    std::cout << "total_ms=" << (nullMoveMs + gamePhaseMs + bookKeyMs) << '\n';

    return 0;
}
