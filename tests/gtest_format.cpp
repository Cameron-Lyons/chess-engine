#include "core/ChessBoard.h"
#include "core/ChessPiece.h"
#include "core/Move.h"
#include "gtest/gtest.h"
#include "protocol/uci.h"
#include "utils/ChessFormat.h"

TEST(Format, SquareNameMapsBoardIndices) {
    EXPECT_EQ(chess::format::squareName(0), "a1");
    EXPECT_EQ(chess::format::squareName(12), "e2");
    EXPECT_EQ(chess::format::squareName(28), "e4");
    EXPECT_EQ(chess::format::squareName(63), "h8");
    EXPECT_EQ(chess::format::squareName(-1), "??");
    EXPECT_EQ(chess::format::squareName(64), "??");
}

TEST(Format, MoveToUciFormatsValidMoves) {
    EXPECT_EQ(chess::format::moveToUci({12, 28}), "e2e4");
    EXPECT_EQ(chess::format::moveToUci({-1, -1}), "0000");
}

TEST(Format, PieceLabelDescribesOccupiedSquares) {
    EXPECT_EQ(chess::format::pieceLabel(Piece()), ".");
    EXPECT_EQ(chess::format::pieceLabel(Piece(ChessPieceColor::WHITE, ChessPieceType::KNIGHT)),
              "W1");
    EXPECT_EQ(chess::format::pieceLabel(Piece(ChessPieceColor::BLACK, ChessPieceType::QUEEN)),
              "B4");
}

TEST(Format, UciNotationParsesStandardMoves) {
    if (const auto parsed = UCINotation::uciToMove("e2e4"); parsed.has_value()) {
        EXPECT_EQ(parsed->first, 12);
        EXPECT_EQ(parsed->second, 28);
        EXPECT_EQ(UCINotation::moveToUCI(*parsed), "e2e4");
    } else {
        FAIL() << "Expected valid UCI move parse";
    }
}

TEST(Format, UciNotationRejectsInvalidInput) {
    EXPECT_FALSE(UCINotation::uciToMove("").has_value());
    EXPECT_FALSE(UCINotation::uciToMove("e9e4").has_value());
    EXPECT_FALSE(UCINotation::uciToMove("e2e").has_value());
    EXPECT_FALSE(UCINotation::uciToMove("0000").has_value());
    EXPECT_EQ(UCINotation::moveToUCI({-1, -1}), "0000");
}

TEST(Format, UciNotationRejectsFiveCharacterPromotionInput) {
    EXPECT_FALSE(UCINotation::uciToMove("a7a8q").has_value());
}
