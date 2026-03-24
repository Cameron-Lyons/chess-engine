#include "ai/EndgameTablebase.h"
#include "ai/SyzygyTablebase.h"
#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
std::filesystem::path createFakeTablebaseDir() {
    const auto dir = std::filesystem::temp_directory_path() / "fake-syzygy";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "KQPvK.rtbw", std::ios::binary | std::ios::trunc).put('\0');
    std::ofstream(dir / "KQPvK.rtbz", std::ios::binary | std::ios::trunc).put('\0');
    return dir;
}
} // namespace

class TablebaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(TablebaseTest, MissingTablebasePathFailsGracefully) {
    EndgameTablebase tablebase("/definitely/missing/tablebases");
    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    EndgameTablebase::TablebaseResult result{};
    Move bestMove{-1, -1};

    EXPECT_FALSE(tablebase.probe(board, result));
    EXPECT_FALSE(tablebase.getBestMove(board, bestMove));
    EXPECT_FALSE(tablebase.isWinning(board));
    EXPECT_FALSE(tablebase.isLosing(board));
    EXPECT_FALSE(tablebase.isDraw(board));
}

TEST_F(TablebaseTest, NonTablebasePositionDoesNotProbe) {
    EndgameTablebase tablebase;
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    EndgameTablebase::TablebaseResult result{};
    EXPECT_FALSE(tablebase.isInTablebase(board));
    EXPECT_FALSE(tablebase.probe(board, result));
}

TEST_F(TablebaseTest, GeneratedSyzygyAssetsCanProbeSimpleWinningEndgame) {
    const auto dir = createFakeTablebaseDir();
    ASSERT_TRUE(Syzygy::init(dir.string()));

    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/3PQ3/4K3 w - - 0 1");
    ASSERT_TRUE(Syzygy::canProbe(board));

    int success = 0;
    EXPECT_EQ(Syzygy::probeWDL(board, &success), Syzygy::PROBE_WIN);
    EXPECT_EQ(success, 1);
    EXPECT_EQ(Syzygy::probeDTZ(board, &success), 10);

    EndgameTablebase tablebase(dir.string());
    EndgameTablebase::TablebaseResult result{};
    ASSERT_TRUE(tablebase.probe(board, result));
    EXPECT_TRUE(result.isExact);
    EXPECT_EQ(result.distanceToMate, 10);
    EXPECT_TRUE(tablebase.isWinning(board));
    EXPECT_FALSE(tablebase.isLosing(board));
    EXPECT_FALSE(tablebase.isDraw(board));

    std::filesystem::remove_all(dir);
}

TEST_F(TablebaseTest, GeneratedSyzygyAssetsCanProbeSimpleLosingEndgame) {
    const auto dir = createFakeTablebaseDir();
    ASSERT_TRUE(Syzygy::init(dir.string()));

    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/3PQ3/4K3 b - - 0 1");
    ASSERT_TRUE(Syzygy::canProbe(board));

    int success = 0;
    EXPECT_EQ(Syzygy::probeWDL(board, &success), Syzygy::PROBE_LOSS);
    EXPECT_EQ(success, 1);
    EXPECT_EQ(Syzygy::probeDTZ(board, &success), -10);

    EndgameTablebase tablebase(dir.string());
    EXPECT_TRUE(tablebase.isLosing(board));
    EXPECT_FALSE(tablebase.isWinning(board));
    EXPECT_FALSE(tablebase.isDraw(board));

    std::filesystem::remove_all(dir);
}

TEST_F(TablebaseTest, GeneratedSyzygyAssetsReportBareKingsAsDraw) {
    const auto dir = createFakeTablebaseDir();
    ASSERT_TRUE(Syzygy::init(dir.string()));

    Board board;
    board.InitializeFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    ASSERT_TRUE(Syzygy::canProbe(board));

    int success = 0;
    EXPECT_EQ(Syzygy::probeWDL(board, &success), Syzygy::PROBE_DRAW);
    EXPECT_EQ(success, 1);

    EndgameTablebase tablebase(dir.string());
    EXPECT_TRUE(tablebase.isDraw(board));

    std::filesystem::remove_all(dir);
}

TEST_F(TablebaseTest, FailedInitResetsSyzygyGlobalState) {
    const auto dir = createFakeTablebaseDir();
    ASSERT_TRUE(Syzygy::init(dir.string()));
    EXPECT_GT(Syzygy::maxPieces(), 0);
    EXPECT_FALSE(Syzygy::getPath().empty());

    EXPECT_FALSE(Syzygy::init("/definitely/missing/tablebases"));
    EXPECT_EQ(Syzygy::maxPieces(), 0);
    EXPECT_TRUE(Syzygy::getPath().empty());

    std::filesystem::remove_all(dir);
}
