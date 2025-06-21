#include "ChessBoard.h"
#include "search.h"
#include "engine_globals.h"
#include <iostream>
#include <chrono>
#include <thread>

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    std::cout << "  ---------------\n";
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << " ";
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece piece = board.squares[pos].Piece;
            char symbol = '.';
            if (piece.PieceType != ChessPieceType::NONE) {
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p'; break;
                    case ChessPieceType::KNIGHT: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n'; break;
                    case ChessPieceType::BISHOP: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b'; break;
                    case ChessPieceType::ROOK: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r'; break;
                    case ChessPieceType::QUEEN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q'; break;
                    case ChessPieceType::KING: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k'; break;
                    default: symbol = '.'; break;
                }
            }
            std::cout << symbol << " ";
        }
        std::cout << row + 1 << "\n";
    }
    std::cout << "  ---------------\n";
    std::cout << "  a b c d e f g h\n";
}

int main() {
    std::cout << "Testing Parallel Chess Engine Search\n";
    std::cout << "=====================================\n\n";
    
    // Initialize Zobrist hashing
    InitZobrist();
    
    // Test position
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Starting position:\n";
    printBoard(board);
    
    // Test single-threaded search
    std::cout << "\n=== Single-threaded Search ===\n";
    auto start1 = std::chrono::steady_clock::now();
    SearchResult result1 = iterativeDeepeningParallel(board, 4, 5000, 1);
    auto end1 = std::chrono::steady_clock::now();
    auto time1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
    
    std::cout << "Single-threaded result:\n";
    std::cout << "  Best move: " << result1.bestMove.first << " -> " << result1.bestMove.second << "\n";
    std::cout << "  Score: " << result1.score << "\n";
    std::cout << "  Depth: " << result1.depth << "\n";
    std::cout << "  Nodes: " << result1.nodes << "\n";
    std::cout << "  Time: " << time1.count() << "ms\n";
    
    // Test multi-threaded search
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    std::cout << "\n=== Multi-threaded Search (" << numThreads << " threads) ===\n";
    auto start2 = std::chrono::steady_clock::now();
    SearchResult result2 = iterativeDeepeningParallel(board, 4, 5000, numThreads);
    auto end2 = std::chrono::steady_clock::now();
    auto time2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
    
    std::cout << "Multi-threaded result:\n";
    std::cout << "  Best move: " << result2.bestMove.first << " -> " << result2.bestMove.second << "\n";
    std::cout << "  Score: " << result2.score << "\n";
    std::cout << "  Depth: " << result2.depth << "\n";
    std::cout << "  Nodes: " << result2.nodes << "\n";
    std::cout << "  Time: " << time2.count() << "ms\n";
    
    // Calculate speedup
    if (time1.count() > 0) {
        double speedup = static_cast<double>(time1.count()) / time2.count();
        std::cout << "\nSpeedup: " << speedup << "x\n";
        std::cout << "Efficiency: " << (speedup / numThreads) * 100 << "%\n";
    }
    
    // Test a more complex position
    std::cout << "\n=== Testing Complex Position ===\n";
    Board complexBoard;
    complexBoard.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3");
    
    std::cout << "Complex position:\n";
    printBoard(complexBoard);
    
    auto start3 = std::chrono::steady_clock::now();
    SearchResult result3 = iterativeDeepeningParallel(complexBoard, 5, 3000, numThreads);
    auto end3 = std::chrono::steady_clock::now();
    auto time3 = std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3);
    
    std::cout << "Complex position result:\n";
    std::cout << "  Best move: " << result3.bestMove.first << " -> " << result3.bestMove.second << "\n";
    std::cout << "  Score: " << result3.score << "\n";
    std::cout << "  Depth: " << result3.depth << "\n";
    std::cout << "  Nodes: " << result3.nodes << "\n";
    std::cout << "  Time: " << time3.count() << "ms\n";
    
    std::cout << "\nParallel search test completed successfully!\n";
    return 0;
} 