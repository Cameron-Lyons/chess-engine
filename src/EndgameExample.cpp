#include "ai/EndgameTablebase.h"
#include "core/ChessBoard.h"
#include <iostream>

// Example of using endgame tablebases
int main() {
    // Initialize tablebase with path to Syzygy files
    EndgameTablebase tablebase("./tablebases/");
    
    // Create a simple KQ vs K endgame position
    Board board;
    board.InitializeEmpty();
    
    // Place white king on e4
    board.squares[28].piece = Piece(ChessPieceType::KING, ChessPieceColor::WHITE);
    
    // Place white queen on d5
    board.squares[35].piece = Piece(ChessPieceType::QUEEN, ChessPieceColor::WHITE);
    
    // Place black king on e8
    board.squares[60].piece = Piece(ChessPieceType::KING, ChessPieceColor::BLACK);
    
    board.turn = ChessPieceColor::WHITE;
    board.updateBitboards();
    
    // Check if position is in tablebase
    if (tablebase.isInTablebase(board)) {
        std::cout << "Position found in tablebase!" << std::endl;
        
        // Probe the tablebase
        EndgameTablebase::TablebaseResult result;
        if (tablebase.probe(board, result)) {
            if (result.distanceToMate > 0) {
                std::cout << "White is winning" << std::endl;
            } else if (result.distanceToMate < 0) {
                std::cout << "Black is winning" << std::endl;
            } else {
                std::cout << "Position is drawn" << std::endl;
            }
            
            // Get best move
            std::pair<int, int> bestMove;
            if (tablebase.getBestMove(board, bestMove)) {
                int fromRow = bestMove.first / 8;
                int fromCol = bestMove.first % 8;
                int toRow = bestMove.second / 8;
                int toCol = bestMove.second % 8;
                
                std::cout << "Best move: " << char('a' + fromCol) << (fromRow + 1)
                         << " to " << char('a' + toCol) << (toRow + 1) << std::endl;
            }
        }
    } else {
        std::cout << "Position not in tablebase (or tablebase not loaded)" << std::endl;
        
        // Use endgame knowledge instead
        int eval = EndgameKnowledge::evaluateEndgame(board);
        std::cout << "Endgame evaluation: " << eval << std::endl;
    }
    
    return 0;
}