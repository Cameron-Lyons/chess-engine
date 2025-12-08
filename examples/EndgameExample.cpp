#include "src/ai/EndgameTablebase.h"
#include "src/core/ChessBoard.h"
#include <iostream>

int main() {

    EndgameTablebase tablebase("./tablebases/");

    Board board;
    board.InitializeEmpty();

    board.squares[28].piece = Piece(ChessPieceType::KING, ChessPieceColor::WHITE);

    board.squares[35].piece = Piece(ChessPieceType::QUEEN, ChessPieceColor::WHITE);

    board.squares[60].piece = Piece(ChessPieceType::KING, ChessPieceColor::BLACK);

    board.turn = ChessPieceColor::WHITE;
    board.updateBitboards();

    if (tablebase.isInTablebase(board)) {
        std::cout << "Position found in tablebase!" << std::endl;

        EndgameTablebase::TablebaseResult result;
        if (tablebase.probe(board, result)) {
            if (result.distanceToMate > 0) {
                std::cout << "White is winning" << std::endl;
            } else if (result.distanceToMate < 0) {
                std::cout << "Black is winning" << std::endl;
            } else {
                std::cout << "Position is drawn" << std::endl;
            }

            std::pair<int, int> bestMove;
            if (tablebase.getBestMove(board, bestMove)) {
                int fromRow = bestMove.first / 8;
                int fromCol = bestMove.first % 8;
                int toRow = bestMove.second / 8;
                int toCol = bestMove.second % 8;

                std::cout << "Best move: " << char('a' + fromCol) << (fromRow + 1) << " to "
                          << char('a' + toCol) << (toRow + 1) << std::endl;
            }
        }
    } else {
        std::cout << "Position not in tablebase (or tablebase not loaded)" << std::endl;

        int eval = EndgameKnowledge::evaluateEndgame(board);
        std::cout << "Endgame evaluation: " << eval << std::endl;
    }

    return 0;
}