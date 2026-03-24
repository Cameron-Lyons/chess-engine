#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "core/ChessEngine.h"
#include "core/GameRules.h"
#include "gtest/gtest.h"
#include "search/search.h"

class DrawRulesTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(DrawRulesTest, FenRoundTripPreservesMoveCounters) {
    Board board;
    const std::string fen = "4k3/8/8/8/8/8/8/4K3 w - - 100 75";
    board.InitializeFromFEN(fen);

    EXPECT_EQ(board.halfmoveClock, 100);
    EXPECT_EQ(board.fullmoveNumber, 75);
    EXPECT_EQ(board.toFEN(), fen);
}

TEST_F(DrawRulesTest, QuietMovesAdvanceHalfmoveAndFullmoveCounters) {
    Engine engine;
    ASSERT_TRUE(engine.setPositionFromFEN("4k2n/8/8/8/8/8/8/4K1N1 w - - 0 1").has_value());

    ASSERT_TRUE(engine.applyMove(6, 21).has_value());
    EXPECT_EQ(engine.board().halfmoveClock, 1);
    EXPECT_EQ(engine.board().fullmoveNumber, 1);

    ASSERT_TRUE(engine.applyMove(63, 46).has_value());
    EXPECT_EQ(engine.board().halfmoveClock, 2);
    EXPECT_EQ(engine.board().fullmoveNumber, 2);
}

TEST_F(DrawRulesTest, FiftyMoveRuleTriggersFromFen) {
    Engine engine;
    ASSERT_TRUE(engine.setPositionFromFEN("4k3/8/8/8/8/8/8/R3K3 w - - 100 75").has_value());

    EXPECT_TRUE(engine.isDrawByFiftyMoveRule());
    EXPECT_EQ(checkGameState(engine), GameState::DRAW_FIFTY_MOVE_RULE);
}

TEST_F(DrawRulesTest, ThreefoldRepetitionIsTrackedByEngine) {
    Engine engine;

    const Move repetitionLine[] = {
        {6, 21},  // g1f3
        {62, 45}, // g8f6
        {21, 6},  // f3g1
        {45, 62}, // f6g8
        {6, 21},  // g1f3
        {62, 45}, // g8f6
        {21, 6},  // f3g1
        {45, 62}, // f6g8
    };

    for (const Move& move : repetitionLine) {
        ASSERT_TRUE(engine.applyMove(move).has_value());
    }

    EXPECT_TRUE(engine.isDrawByThreefoldRepetition());
    EXPECT_EQ(checkGameState(engine), GameState::DRAW_THREEFOLD_REPETITION);
}
