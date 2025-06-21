#include "ChessBoard.h"
#include <iostream>

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    std::cout << "  ---------------\n";
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << " ";
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece piece = board.squares[pos].Piece;
            char symbol = '.';
            if (piece.PieceType != NONE) {
                switch (piece.PieceType) {
                    case PAWN: symbol = piece.PieceColor == WHITE ? 'P' : 'p'; break;
                    case KNIGHT: symbol = piece.PieceColor == WHITE ? 'N' : 'n'; break;
                    case BISHOP: symbol = piece.PieceColor == WHITE ? 'B' : 'b'; break;
                    case ROOK: symbol = piece.PieceColor == WHITE ? 'R' : 'r'; break;
                    case QUEEN: symbol = piece.PieceColor == WHITE ? 'Q' : 'q'; break;
                    case KING: symbol = piece.PieceColor == WHITE ? 'K' : 'k'; break;
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
    std::cout << "Board after FEN parsing:\n";
    printBoard(testBoard);
    return 0;
} 