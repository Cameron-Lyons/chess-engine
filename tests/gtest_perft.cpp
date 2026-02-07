#include "ChessBoard.h"
#include "core/BitboardMoves.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"

std::vector<std::pair<int, int>> generateBitboardMoves(Board& board, ChessPieceColor color);

static uint64_t perft(Board& board, ChessPieceColor color, int depth) {
    if (depth == 0) return 1;

    GenValidMoves(board);
    auto moves = generateBitboardMoves(board, color);

    ChessPieceColor nextColor =
        (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    uint64_t nodes = 0;
    for (const auto& move : moves) {
        Board copy = board;
        copy.movePiece(move.first, move.second);

        if (isInCheck(copy, color)) continue;

        copy.turn = nextColor;
        copy.updateBitboards();
        nodes += perft(copy, nextColor, depth - 1);
    }
    return nodes;
}

class PerftTest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
    }
};

TEST_F(PerftTest, StartposDepth1) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 1), 20);
}

TEST_F(PerftTest, StartposDepth2) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 2), 400);
}

TEST_F(PerftTest, StartposDepth3) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 3);
    EXPECT_GT(nodes, 8000);
    EXPECT_LT(nodes, 9500);
}

TEST_F(PerftTest, StartposDepth4) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 4);
    EXPECT_GT(nodes, 170000);
    EXPECT_LT(nodes, 210000);
}

TEST_F(PerftTest, StartposDepth5) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 5);
    EXPECT_GT(nodes, 4000000);
    EXPECT_LT(nodes, 5500000);
}

TEST_F(PerftTest, KiwipeteDepth1) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 1);
    EXPECT_GT(nodes, 40);
    EXPECT_LT(nodes, 52);
}

TEST_F(PerftTest, KiwipeteDepth2) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 2);
    EXPECT_GT(nodes, 1400);
    EXPECT_LT(nodes, 2200);
}

TEST_F(PerftTest, KiwipeteDepth3) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 3);
    EXPECT_GT(nodes, 65000);
    EXPECT_LT(nodes, 105000);
}

TEST_F(PerftTest, KiwipeteDepth4) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 4);
    EXPECT_GT(nodes, 2200000);
    EXPECT_LT(nodes, 4500000);
}

TEST_F(PerftTest, Position3Depth1) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 1);
    EXPECT_GT(nodes, 12);
    EXPECT_LT(nodes, 18);
}

TEST_F(PerftTest, Position3Depth2) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 2);
    EXPECT_GT(nodes, 170);
    EXPECT_LT(nodes, 250);
}

TEST_F(PerftTest, Position3Depth3) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 3);
    EXPECT_GT(nodes, 2500);
    EXPECT_LT(nodes, 4000);
}

TEST_F(PerftTest, Position3Depth4) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    uint64_t nodes = perft(board, ChessPieceColor::WHITE, 4);
    EXPECT_GT(nodes, 38000);
    EXPECT_LT(nodes, 62000);
}
