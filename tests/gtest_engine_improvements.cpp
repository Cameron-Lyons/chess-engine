#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

TEST(EngineImprovements, QuiescenceSearch) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board tacticalBoard;
    tacticalBoard.InitializeFromFEN(
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");

    int staticEval = evaluatePosition(tacticalBoard);

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 3000;

    GenValidMoves(tacticalBoard);
    int qScore = QuiescenceSearch(tacticalBoard, -10000, 10000, true, historyTable, context);

    ASSERT_GT(qScore, staticEval);
}

TEST(EngineImprovements, KingSafety) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board safeKing;
    safeKing.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int safeKingSafety = evaluateKingSafety(safeKing, ChessPieceColor::WHITE);

    Board exposedKing;
    exposedKing.InitializeFromFEN("rnbqkbnr/ppp1pppp/8/8/8/8/PPP1PPPP/RNBQKBNR w KQkq - 0 1");
    int exposedKingSafety = evaluateKingSafety(exposedKing, ChessPieceColor::WHITE);

    ASSERT_LT(exposedKingSafety, safeKingSafety);
}
