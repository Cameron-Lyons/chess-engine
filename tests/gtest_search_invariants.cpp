#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"
#include "search/search_internal.h"

#include <cstdint>
#include <string>

namespace {
class SearchInvariantTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }

    static void expectRoundTrip(const char* fen, int from, int to) {
        Board board;
        board.InitializeFromFEN(fen);

        const std::string originalFen = board.toFEN();
        const uint64_t originalHash = ComputeZobrist(board);

        SearchInternal::MoveApplicationData moveData{};
        ASSERT_TRUE(SearchInternal::applySearchMoveWithData(board, from, to, true, &moveData));

        const uint64_t childHash =
            SearchInternal::computeChildZobrist(originalHash, board, from, to, moveData);
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;
        EXPECT_EQ(childHash, ComputeZobrist(board));

        SearchInternal::undoSearchMoveWithData(board, moveData);
        EXPECT_EQ(board.toFEN(), originalFen);
        EXPECT_EQ(ComputeZobrist(board), originalHash);
    }
};
} // namespace

TEST_F(SearchInvariantTest, NormalMoveRoundTripPreservesFenAndHash) {
    expectRoundTrip("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 12, 28);
}

TEST_F(SearchInvariantTest, CaptureRoundTripPreservesFenAndHash) {
    expectRoundTrip("4k3/8/3p4/4P3/8/8/8/4K3 w - - 0 1", 36, 43);
}

TEST_F(SearchInvariantTest, EnPassantRoundTripPreservesFenAndHash) {
    expectRoundTrip("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", 36, 43);
}

TEST_F(SearchInvariantTest, CastlingRoundTripPreservesFenAndHash) {
    expectRoundTrip("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 4, 6);
}

TEST_F(SearchInvariantTest, PromotionRoundTripPreservesFenAndHash) {
    expectRoundTrip("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", 48, 56);
}
