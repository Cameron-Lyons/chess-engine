#include "ChessBoard.h"
#include "core/BitboardMoves.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"

struct WACPosition {
    const char* fen;
    int expectedFrom;
    int expectedTo;
    const char* description;
};

class WACTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }

    bool checkMove(const char* fen, int expectedFrom, int expectedTo, int depthLimit, int timeLimitMs) {
        Board board;
        board.InitializeFromFEN(fen);

        SearchResult result = iterativeDeepeningParallel(board, depthLimit, timeLimitMs, 1, 0, 1);

        return result.bestMove.first == expectedFrom && result.bestMove.second == expectedTo;
    }
};

TEST_F(WACTest, WAC002_QxMate) {
    Board board;
    board.InitializeFromFEN("8/7p/5k2/5p2/p1p2P2/Pr1pPK2/1P1R3P/8 b - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC003_KnightFork) {
    Board board;
    board.InitializeFromFEN("r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC005_Discovery) {
    Board board;
    board.InitializeFromFEN("r1b2rk1/2q1b1pp/p2ppn2/1p6/3QP3/1BN1B3/PPP3PP/R4RK1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC008_Pin) {
    Board board;
    board.InitializeFromFEN("2r2rk1/1bqnbpp1/1p1ppn1p/pP6/N1P1P3/P2B1N1P/1B2QPP1/R2R2K1 b - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC010_TacticalExchange) {
    Board board;
    board.InitializeFromFEN("r1bqkb1r/pp3ppp/2np1n2/4p1B1/2B1P3/5N2/PPP2PPP/RN1QK2R w KQkq - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC012_Skewer) {
    Board board;
    board.InitializeFromFEN("r2qnrnk/p2b2b1/1p1p2pp/2pPpp2/1PP1P3/PRNBB3/3Q1PPP/5RK1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC015_Sacrifice) {
    Board board;
    board.InitializeFromFEN("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC018_Clearance) {
    Board board;
    board.InitializeFromFEN("r1bq1rk1/ppppnppp/4bn2/8/2BNP3/2N5/PPP2PPP/R1BQ1RK1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC020_Deflection) {
    Board board;
    board.InitializeFromFEN("r2q1rk1/ppp2ppp/2np1b2/5b2/2B1PB2/2N5/PPP2PPP/R2QR1K1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, WAC022_BackRank) {
    Board board;
    board.InitializeFromFEN("6k1/5p2/6p1/8/7p/8/6PP/6K1 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, MateInOne_WhiteToPlay) {
    Board board;
    board.InitializeFromFEN("6k1/5ppp/8/8/8/8/8/R3K3 w Q - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_GT(result.score, 9000);
}

TEST_F(WACTest, MateInTwo_WhiteToPlay) {
    Board board;
    board.InitializeFromFEN("2bqkbn1/2pppp2/np2N3/r3P1p1/p2N2B1/5Q2/PPPPPPPP/R1B1K2R w KQ - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_GT(result.score, 9000);
}

TEST_F(WACTest, TrappedPiece) {
    Board board;
    board.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, QueenTrap) {
    Board board;
    board.InitializeFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, KingSafety) {
    Board board;
    board.InitializeFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 8, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, EndgameConversion) {
    Board board;
    board.InitializeFromFEN("8/8/1p2k1p1/1P6/6P1/4K3/8/8 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 10, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, PawnRace) {
    Board board;
    board.InitializeFromFEN("8/5k2/8/5P2/8/8/2K5/8 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 12, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, KnightEndgame) {
    Board board;
    board.InitializeFromFEN("8/8/4k3/8/3N4/8/3K4/8 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 10, 5000, 1, 0, 1);
    EXPECT_TRUE(result.bestMove.first >= 0);
}

TEST_F(WACTest, RookEndgame) {
    Board board;
    board.InitializeFromFEN("8/5k2/8/8/8/8/R4K2/8 w - - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 10, 5000, 1, 0, 1);
    EXPECT_GT(result.score, 0);
}

TEST_F(WACTest, DoubleRookMate) {
    Board board;
    board.InitializeFromFEN("6k1/8/8/8/8/8/8/RR2K3 w Q - 0 1");
    SearchResult result = iterativeDeepeningParallel(board, 10, 5000, 1, 0, 1);
    EXPECT_GT(result.score, 500);
}
