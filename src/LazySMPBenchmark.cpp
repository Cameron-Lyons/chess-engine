#include "core/ChessBoard.h"
#include "search/search.h"
#include "utils/engine_globals.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    InitZobrist();

    std::vector<std::string> testPositions = {

        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",

        "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",

        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",

        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"};

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;

    std::cout << "Lazy SMP Benchmark" << std::endl;
    std::cout << "==================" << std::endl;
    std::cout << "Threads available: " << numThreads << std::endl;
    std::cout << std::endl;

    for (const auto& fen : testPositions) {
        Board board;
        board.InitializeFromFEN(fen);

        std::cout << "Position: " << fen << std::endl;

        auto start1 = std::chrono::high_resolution_clock::now();
        SearchResult result1 = iterativeDeepeningParallel(board, 8, 5000, 1);
        auto end1 = std::chrono::high_resolution_clock::now();
        auto time1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

        std::cout << "Regular search (1 thread):" << std::endl;
        std::cout << "  Depth: " << result1.depth << std::endl;
        std::cout << "  Score: " << result1.score << std::endl;
        std::cout << "  Nodes: " << result1.nodes << std::endl;
        std::cout << "  Time: " << time1 << " ms" << std::endl;
        std::cout << "  NPS: " << (result1.nodes * 1000 / std::max(1LL, time1)) << std::endl;

        auto start2 = std::chrono::high_resolution_clock::now();
        SearchResult result2 = lazySMPSearch(board, 8, 5000, numThreads);
        auto end2 = std::chrono::high_resolution_clock::now();
        auto time2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();

        std::cout << "Lazy SMP (" << numThreads << " threads):" << std::endl;
        std::cout << "  Depth: " << result2.depth << std::endl;
        std::cout << "  Score: " << result2.score << std::endl;
        std::cout << "  Nodes: " << result2.nodes << std::endl;
        std::cout << "  Time: " << time2 << " ms" << std::endl;
        std::cout << "  NPS: " << (result2.nodes * 1000 / std::max(1LL, time2)) << std::endl;

        double speedup = static_cast<double>(time1) / static_cast<double>(std::max(1LL, time2));
        double efficiency = speedup / numThreads * 100.0;

        std::cout << "Speedup: " << speedup << "x" << std::endl;
        std::cout << "Efficiency: " << efficiency << "%" << std::endl;
        std::cout << std::endl;
    }

    return 0;
}