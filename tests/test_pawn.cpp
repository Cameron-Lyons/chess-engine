#include "ChessBoard.h"
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
    std::cout << "=== Pawn Move Generation Test ===\n\n";
    
    Board testBoard;
    testBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Testing pawn move generation on starting position...\n";
    
    int e2Pos = 12;
    std::cout << "Position e2 (" << e2Pos << "): row=" << e2Pos/8 << ", col=" << e2Pos%8 << "\n";
    
    const Piece& e2Pawn = testBoard.squares[e2Pos].Piece;
    std::cout << "Piece at e2: Type=" << to_string(e2Pawn.PieceType) << ", Color=" << to_string(e2Pawn.PieceColor) << "\n";
    
    std::vector<std::pair<int, int>> allMoves = GetAllMoves(testBoard, ChessPieceColor::WHITE);
    
    std::cout << "\nPawn moves found:\n";
    int pawnMoveCount = 0;
    for (const auto& move : allMoves) {
        const Piece& piece = testBoard.squares[move.first].Piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            int fromRow = move.first / 8;
            int fromCol = move.first % 8;
            int toRow = move.second / 8;
            int toCol = move.second % 8;
            char fromFile = 'a' + fromCol;
            char toFile = 'a' + toCol;
            
            std::cout << "  " << fromFile << (fromRow + 1) << " to " << toFile << (toRow + 1) << "\n";
            pawnMoveCount++;
        }
    }
    
    std::cout << "\nTest Results:\n";
    std::cout << "✓ Total white pawn moves: " << pawnMoveCount << "\n";
    std::cout << "✓ Expected: 16 pawn moves (8 pawns × 1-2 moves each)\n";
    
    if (pawnMoveCount == 16) {
        std::cout << "✅ PASS: Correct number of pawn moves generated!\n";
    } else {
        std::cout << "❌ FAIL: Expected 16 pawn moves, got " << pawnMoveCount << "\n";
    }
    
    std::cout << "\n=== Testing Pawn Promotion ===\n";
    testBoard.InitializeFromFEN("rnbqkb1r/ppppp2p/5n2/5Pp1/8/8/PPPPPP1P/RNBQKBNR w KQkq g6 0 4");
    
    testBoard.squares[53].Piece = Piece(ChessPieceColor::WHITE, ChessPieceType::PAWN); 
    testBoard.squares[61].Piece = Piece(); 
    
    std::vector<std::pair<int, int>> promotionMoves = GetAllMoves(testBoard, ChessPieceColor::WHITE);
    
    bool foundPromotionMove = false;
    for (const auto& move : promotionMoves) {
        if (move.first == 53 && move.second == 61) { 
            foundPromotionMove = true;
            break;
        }
    }
    
    if (foundPromotionMove) {
        std::cout << "✅ PASS: Pawn promotion move detected!\n";
    } else {
        std::cout << "❌ FAIL: Pawn promotion move not found\n";
    }
    
    std::cout << "\n=== Pawn Test Complete ===\n";
    return 0;
} 