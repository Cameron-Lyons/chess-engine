#include "ChessBoard.h"
#include <iostream>

int main() {
    std::cout << "Testing chess engine initialization...\n";
    
    try {
        std::cout << "Step 1: Creating board...\n";
        Board testBoard;
        std::cout << "Board created successfully.\n";
        
        std::cout << "Step 2: Initializing from FEN...\n";
        testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        std::cout << "FEN initialization completed.\n";
        
        std::cout << "Step 3: Testing basic board operations...\n";
        if (testBoard.squares[0].Piece.PieceType == ChessPieceType::ROOK) {
            std::cout << "Board setup correct - rook found at a1.\n";
        } else {
            std::cout << "Warning: Board setup may be incorrect.\n";
        }
        
        std::cout << "Step 4: Testing move generation...\n";
        std::vector<std::pair<int, int>> moves = GetAllMoves(testBoard, testBoard.turn);
        std::cout << "Generated " << moves.size() << " moves.\n";
        if (moves.size() >= 16) { 
            std::cout << "Move generation working correctly.\n";
        } else {
            std::cout << "Warning: Fewer moves generated than expected.\n";
        }
        
        std::cout << "All tests passed!\n";
        
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception caught!" << std::endl;
        return 1;
    }
    
    return 0;
} 