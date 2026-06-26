#pragma once

#include "gtest/gtest.h"
#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

inline void initEngineForTests() {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
}

inline SearchResult runSearch(Board& board, int depth, int timeMs, int threads = 1, int contempt = 0,
                              int multiPV = 1, int optimalTime = 0, int maxTime = 0) {
    SearchConfig config;
    config.maxDepth = depth;
    config.timeLimitMs = timeMs;
    config.contempt = contempt;
    config.multiPV = multiPV;
    config.optimalTimeMs = optimalTime;
    config.maxTimeMs = maxTime;
    SearchContext context;
    context.threads = threads;
    return iterativeDeepeningParallel(board, config, context);
}

inline bool isSearchMoveLegal(const Board& board, const Move& move) {
    if (!move.isValid()) {
        return false;
    }
    Board testBoard = board;
    if (!applySearchMove(testBoard, move.first, move.second)) {
        return false;
    }
    return !isInCheck(testBoard, board.turn);
}

inline void expectFenRoundTrip(const char* fen) {
    Board board;
    board.InitializeFromFEN(fen);
    EXPECT_EQ(board.toFEN(), fen);
}

inline void expectLegalSearchMove(const Board& board, const SearchResult& result) {
    EXPECT_TRUE(isSearchMoveLegal(board, result.bestMove))
        << "illegal move (" << result.bestMove.first << "," << result.bestMove.second << ")";
}
