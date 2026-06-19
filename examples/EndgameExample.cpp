#include "ai/EndgameTablebase.h"
#include "core/ChessBoard.h"
#include "core/Move.h"
#include "utils/ChessFormat.h"

#include <iostream>

int main() {
    EndgameTablebase tablebase("./tablebases/");
    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/3PQ3/4K3 w - - 0 1");

    if (tablebase.isInTablebase(board)) {
        std::cout << "Position found in tablebase!\n";
        EndgameTablebase::TablebaseResult result{};
        if (tablebase.probe(board, result)) {
            if (result.distanceToMate > 0) {
                std::cout << "White is winning\n";
            } else if (result.distanceToMate < 0) {
                std::cout << "Black is winning\n";
            } else {
                std::cout << "Position is drawn\n";
            }

            Move bestMove{};
            if (tablebase.getBestMove(board, bestMove)) {
                std::cout << "Best move: " << chess::format::squareName(bestMove.first) << " to "
                          << chess::format::squareName(bestMove.second) << '\n';
            }
        }
    } else {
        std::cout << "Position not in tablebase (or tablebase not loaded)\n";
        const int eval = EndgameKnowledge::evaluateEndgame(board);
        std::cout << "Endgame evaluation: " << eval << '\n';
    }

    return 0;
}
