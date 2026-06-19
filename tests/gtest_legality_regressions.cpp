#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <algorithm>

namespace {
bool containsMove(const std::vector<Move>& moves, int from, int to) {
    return std::ranges::any_of(
        moves, [&](const Move& move) { return move.first == from && move.second == to; });
}
} // namespace

class LegalityRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(LegalityRegressionTest, PinnedPawnAdvanceIsIllegalInPosition3) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    constexpr int b5 = 33;
    constexpr int b6 = 41;
    EXPECT_FALSE(IsMoveLegal(board, b5, b6));
}

TEST_F(LegalityRegressionTest, PawnAttackOnB6CountsAsCheck) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    ASSERT_TRUE(board.movePiece(32, 41));
    EXPECT_TRUE(IsKingInCheck(board, ChessPieceColor::WHITE));
}

TEST_F(LegalityRegressionTest, BishopAttackBlocksIllegalBlackQueensideCastle) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    ASSERT_TRUE(applySearchMove(board, 12, 40, false));
    board.turn = ChessPieceColor::BLACK;

    const std::vector<Move> blackMoves = GetAllMoves(board, ChessPieceColor::BLACK);
    EXPECT_FALSE(containsMove(blackMoves, 60, 58));
    EXPECT_TRUE(containsMove(blackMoves, 60, 62));
}

TEST_F(LegalityRegressionTest, BlackKingOnC8IsInCheckFromBishopOnA6) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");

    ASSERT_TRUE(applySearchMove(board, 12, 40, false));
    ASSERT_TRUE(board.movePiece(60, 58));
    EXPECT_TRUE(IsKingInCheck(board, ChessPieceColor::BLACK));
}
