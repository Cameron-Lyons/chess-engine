#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

TEST(TacticalSuite, MateInOne) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");

    SearchResult result = iterativeDeepeningParallel(board, 3, 5000, 1);
    ASSERT_GT(result.score, 9000);
}

TEST(TacticalSuite, Fork) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    board.InitializeFromFEN("r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");

    SearchResult result = iterativeDeepeningParallel(board, 4, 5000, 1);

    ASSERT_NE(result.bestMove.first, result.bestMove.second);
}
