#include "ChessBoard.h"
#include "core/BitboardMoves.h"
#include "gtest/gtest.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <array>
#include <cstdint>

static uint64_t perft(Board& board, ChessPieceColor color, int depth) {
    if (depth == 0) {
        return 1;
    }

    UpdateCheckState(board);
    auto moves = generateBitboardMoves(board, color);

    ChessPieceColor nextColor =
        (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    uint64_t nodes = 0;
    for (const auto& move : moves) {
        const Piece movingPiece = board.squares[move.first].piece;
        Board copy = board;
        if (!applySearchMove(copy, move.first, move.second, false)) {
            continue;
        }

        if (isInCheck(copy, color)) {
            continue;
        }

        const int destinationRank = move.second / 8;
        const bool isPromotion = movingPiece.PieceType == ChessPieceType::PAWN &&
                                 (destinationRank == 0 || destinationRank == 7);

        if (isPromotion) {
            constexpr std::array<ChessPieceType, 4> kPromotionPieces = {
                ChessPieceType::QUEEN, ChessPieceType::ROOK, ChessPieceType::BISHOP,
                ChessPieceType::KNIGHT};
            for (ChessPieceType promotionPiece : kPromotionPieces) {
                Board promoted = copy;
                promoted.squares[move.second].piece = Piece(color, promotionPiece);
                promoted.squares[move.second].piece.moved = true;
                promoted.updateBitboards();
                promoted.turn = nextColor;
                nodes += perft(promoted, nextColor, depth - 1);
            }
            continue;
        }

        copy.turn = nextColor;
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
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 3), 8902);
}

TEST_F(PerftTest, StartposDepth4) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 4), 197281);
}

TEST_F(PerftTest, StartposDepth5) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 5), 4865609);
}

TEST_F(PerftTest, KiwipeteDepth1) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 1), 48);
}

TEST_F(PerftTest, KiwipeteDepth2) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 2), 2039);
}

TEST_F(PerftTest, KiwipeteDepth3) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 3), 97862);
}

TEST_F(PerftTest, KiwipeteDepth4) {
    Board board;
    board.InitializeFromFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 4), 4085603);
}

TEST_F(PerftTest, Position3Depth1) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 1), 14);
}

TEST_F(PerftTest, Position3Depth2) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 2), 191);
}

TEST_F(PerftTest, Position3Depth3) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 3), 2812);
}

TEST_F(PerftTest, Position3Depth4) {
    Board board;
    board.InitializeFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 4), 43238);
}

TEST_F(PerftTest, EnPassantPositionDepth1Exact) {
    Board board;
    board.InitializeFromFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::WHITE, 1), 7);
}

TEST_F(PerftTest, StalematePositionDepth1Exact) {
    Board board;
    board.InitializeFromFEN("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    EXPECT_EQ(perft(board, ChessPieceColor::BLACK, 1), 0);
}
