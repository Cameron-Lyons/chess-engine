#include "ChessEngine.h"
#include <iostream>

std::string to_string(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN: return "Pawn";
        case ChessPieceType::KNIGHT: return "Knight";
        case ChessPieceType::BISHOP: return "Bishop";
        case ChessPieceType::ROOK: return "Rook";
        case ChessPieceType::QUEEN: return "Queen";
        case ChessPieceType::KING: return "King";
        default: return "None";
    }
}

std::string to_string(ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? "White" : "Black";
}

int main() {
    Engine();
    
    std::cout << "Testing pawn move generation...\n";
    
    GenValidMoves(ChessBoard);
    
    int e2Pos = 12;
    std::cout << "Position e2 (12): row=" << e2Pos/8 << ", col=" << e2Pos%8 << "\n";
    
    Piece& e2Pawn = ChessBoard.squares[e2Pos].Piece;
    std::cout << "Piece at e2: Type=" << to_string(e2Pawn.PieceType) << ", Color=" << to_string(e2Pawn.PieceColor) << "\n";
    std::cout << "Valid moves for e2 pawn: ";
    for (int move : e2Pawn.ValidMoves) {
        int moveRow = move / 8;
        int moveCol = move % 8;
        std::cout << "(" << moveCol << "," << moveRow << ") ";
    }
    std::cout << "\n";
    
    std::cout << "\nAll White pawns:\n";
    for (int i = 8; i <= 15; i++) {
        Piece& pawn = ChessBoard.squares[i].Piece;
        if (pawn.PieceType == ChessPieceType::PAWN && pawn.PieceColor == ChessPieceColor::WHITE) {
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