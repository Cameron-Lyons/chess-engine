#include "ChessEngine.h"
#include <iostream>

int main() {
    // Initialize the engine
    Engine();
    
    std::cout << "Testing pawn move generation...\n";
    
    // Generate initial valid moves
    GenValidMoves(ChessBoard);
    
    // Test the e2 pawn (should be at position 12)
    int e2Pos = 12; // row 1, col 4
    std::cout << "Position e2 (12): row=" << e2Pos/8 << ", col=" << e2Pos%8 << "\n";
    
    Piece& e2Pawn = ChessBoard.squares[e2Pos].Piece;
    std::cout << "Piece at e2: Type=" << e2Pawn.PieceType << ", Color=" << e2Pawn.PieceColor << "\n";
    std::cout << "Valid moves for e2 pawn: ";
    for (int move : e2Pawn.ValidMoves) {
        int moveRow = move / 8;
        int moveCol = move % 8;
        std::cout << "(" << moveCol << "," << moveRow << ") ";
    }
    std::cout << "\n";
    
    // Test all White pawns
    std::cout << "\nAll White pawns:\n";
    for (int i = 8; i <= 15; i++) {
        Piece& pawn = ChessBoard.squares[i].Piece;
        if (pawn.PieceType == PAWN && pawn.PieceColor == WHITE) {
            int row = i / 8;
            int col = i % 8;
            std::cout << "Pawn at (" << col << "," << row << "): ";
            for (int move : pawn.ValidMoves) {
                int moveRow = move / 8;
                int moveCol = move % 8;
                std::cout << "(" << moveCol << "," << moveRow << ") ";
            }
            std::cout << "\n";
        }
    }
    
    return 0;
} 