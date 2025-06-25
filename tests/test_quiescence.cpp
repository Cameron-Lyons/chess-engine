#include "../src/ChessBoard.h"
#include "../src/search.h"
#include "../src/BitboardMoves.h"
#include <iostream>
#include <chrono>

void initKnightAttacks();
void initKingAttacks();

void testSimpleQuiescence() {
    std::cout << "=== Simple Quiescence Search Test ===\n\n";
    
    std::cout << "Initializing engine components...\n";
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
    std::cout << "Initialization complete.\n\n";
    
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Board initialized successfully.\n";
    
    int staticScore = evaluatePosition(board);
    std::cout << "Static evaluation: " << staticScore << std::endl;
    
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 5000;
    
    std::cout << "Calling GenValidMoves...\n";
    GenValidMoves(board);
    std::cout << "GenValidMoves completed.\n";
    
    std::cout << "Testing quiescence search...\n";
    int qScore = QuiescenceSearch(board, -10000, 10000, true, historyTable, context);
    std::cout << "Quiescence score: " << qScore << std::endl;
    
    std::cout << "Difference: " << (qScore - staticScore) << std::endl;
    
    std::cout << "\n=== Test completed successfully ===\n";
}

int main() {
    try {
        testSimpleQuiescence();
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
    return 0;
} 