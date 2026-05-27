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
    UpdateCheckState(tacticalBoard);
    int qScore = QuiescenceSearch(tacticalBoard, -10000, 10000, true, historyTable, context, 0);
    ASSERT_GE(qScore, staticEval);
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

TEST(EngineImprovements, SearchReturnsLegalMoveAfterFourPlyOpening) {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();

    Board board;
    board.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");
    Board searchBoard = board;
    const std::string originalFen = searchBoard.toFEN();

    SearchConfig config;
    config.maxDepth = 64;
    config.timeLimitMs = 80;

    SearchContext context;
    context.threads = 1;

    SearchResult result = iterativeDeepeningParallel(searchBoard, config, context);

    ASSERT_EQ(searchBoard.toFEN(), originalFen);
    ASSERT_GE(result.bestMove.first, 0);
    ASSERT_GE(result.bestMove.second, 0);
    ASSERT_TRUE(result.bestMove.first != 2 || result.bestMove.second != 38);

    std::vector<Move> legalMoves = GetAllMoves(board, board.turn);
    bool found = false;
    for (const auto& move : legalMoves) {
        if (move.first == result.bestMove.first && move.second == result.bestMove.second) {
            found = true;
            break;
        }
    }

    ASSERT_TRUE(found);
}
