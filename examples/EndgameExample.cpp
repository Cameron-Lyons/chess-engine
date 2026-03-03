#include "src/ai/EndgameTablebase.h"
#include "src/core/ChessBoard.h"
#include <iostream>

int main() { // NOLINT(bugprone-exception-escape)

    EndgameTablebase tablebase("./tablebases/");
    Board board;
    board.InitializeEmpty();
    board.squares[28].piece = Piece(ChessPieceType::KING, ChessPieceColor::WHITE);
    board.squares[35].piece = Piece(ChessPieceType::QUEEN, ChessPieceColor::WHITE);
    board.squares[60].piece = Piece(ChessPieceType::KING, ChessPieceColor::BLACK);
    board.turn = ChessPieceColor::WHITE;
    board.updateBitboards();

    if (tablebase.isInTablebase(board)) {
        std::cout << "Position found in tablebase!" << '\n';
        EndgameTablebase::TablebaseResult result;
        if (tablebase.probe(board, result)) {
            if (result.distanceToMate > 0) {
                std::cout << "White is winning" << '\n';
            } else if (result.distanceToMate < 0) {
                std::cout << "Black is winning" << '\n';
            } else {
                std::cout << "Position is drawn" << '\n';
            }

            std::pair<int, int> bestMove;
            if (tablebase.getBestMove(board, bestMove)) {
                int fromRow = bestMove.first / 8;
                int fromCol = bestMove.first % 8;
                int toRow = bestMove.second / 8;
                int toCol = bestMove.second % 8;

                std::cout << "Best move: " << char('a' + fromCol) << (fromRow + 1) << " to "
                          << char('a' + toCol) << (toRow + 1) << '\n';
            }
        }
    } else {
        std::cout << "Position not in tablebase (or tablebase not loaded)" << '\n';
        int eval = EndgameKnowledge::evaluateEndgame(board);
        std::cout << "Endgame evaluation: " << eval << '\n';
    }

    return 0;
}
