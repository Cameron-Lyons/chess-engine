#include "src/core/ChessBoard.h"
#include "src/search/search.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

void testPosition(const std::string& fen, const std::string& description, int depth = 8) {
    std::cout << "\n=== Testing: " << description << " ===" << '\n';
    std::cout << "FEN: " << fen << '\n';

    Board board;
    board.InitializeFromFEN(fen);

    auto start = std::chrono::high_resolution_clock::now();
    SearchResult result = iterativeDeepeningParallel(board, depth, 5000, 4);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    int fromSquare = result.bestMove.first;
    int toSquare = result.bestMove.second;
    char fromFile = 'a' + (fromSquare % 8);
    char fromRank = '1' + (fromSquare / 8);
    char toFile = 'a' + (toSquare % 8);
    char toRank = '1' + (toSquare / 8);

    std::cout << "Best move: " << fromFile << fromRank << toFile << toRank << '\n';
    std::cout << "Score: " << result.score << " (centipawns)" << '\n';
    std::cout << "Depth reached: " << result.depth << '\n';
    std::cout << "Nodes searched: " << result.nodes << '\n';
    std::cout << "Time taken: " << duration.count() << " ms" << '\n';
    std::cout << "NPS: " << (result.nodes * 1000 / std::max<long long>(1LL, duration.count()))
              << '\n';
}

int main() {
    std::cout << "Chess Engine Performance Test - Search Improvements\n";
    std::cout << "====================================================\n";

    testPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "Starting position",
                 10);

    testPosition("r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
                 "Italian Game position", 10);

    testPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                 "Tactical position (Perft position 2)", 10);

    testPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", "Endgame position", 12);

    testPosition("r2q1rk1/ppp2ppp/2n1bn2/2bpp3/3P4/2N1PN2/PPP1BPPP/R1BQK2R w KQ - 0 8",
                 "Complex middlegame", 10);

    testPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
                 "Position with promotion threat", 10);

    std::cout << "\n=== Benchmark Summary ===" << '\n';
    std::vector<std::string> benchmarkFens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    long long totalNodes = 0;
    long long totalTime = 0;

    for (const auto& fen : benchmarkFens) {
        Board board;
        board.InitializeFromFEN(fen);

        auto start = std::chrono::high_resolution_clock::now();
        SearchResult result = iterativeDeepeningParallel(board, 8, 2000, 4);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        totalNodes += result.nodes;
        totalTime += duration.count();
    }

    std::cout << "Total nodes searched: " << totalNodes << '\n';
    std::cout << "Total time: " << totalTime << " ms" << '\n';
    std::cout << "Average NPS: " << (totalNodes * 1000 / std::max<long long>(1LL, totalTime))
              << '\n';

    std::cout << "\n=== Search Improvements Summary ===" << '\n';
    std::cout << "✓ Improved transposition table replacement strategy" << '\n';
    std::cout << "✓ Singular extensions for critical moves" << '\n';
    std::cout << "✓ Futility pruning in non-PV nodes" << '\n';
    std::cout << "✓ Enhanced move ordering with history heuristic" << '\n';
    std::cout << "✓ Aspiration windows in iterative deepening" << '\n';

    return 0;
}