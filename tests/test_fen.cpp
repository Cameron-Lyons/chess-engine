#include "ChessBoard.h"
#include <iostream>

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    std::cout << "  ---------------\n";
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << " ";
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece piece = board.squares[pos].piece;
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
    std::cout << "Testing FEN parsing using Board::InitializeFromFEN...\n";
    Board testBoard;
    std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    testBoard.InitializeFromFEN(fen);
    
    testBoard.updateBitboards();
    
    std::cout << "Board after FEN parsing:\n";
    printBoard(testBoard);
    
    std::cout << "\nDebug: Checking piece placement...\n";
    for (int i = 0; i < 64; i++) {
        const Piece& piece = testBoard.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int row = i / 8;
            int col = i % 8;
            std::cout << "Piece at (" << col << "," << row << "): " 
                      << (piece.PieceColor == ChessPieceColor::WHITE ? "White" : "Black") << " "
                      << (piece.PieceType == ChessPieceType::PAWN ? "Pawn" : 
                          piece.PieceType == ChessPieceType::KNIGHT ? "Knight" :
                          piece.PieceType == ChessPieceType::BISHOP ? "Bishop" :
                          piece.PieceType == ChessPieceType::ROOK ? "Rook" :
                          piece.PieceType == ChessPieceType::QUEEN ? "Queen" : "King") << "\n";
        }
    }
    
    return 0;
} 