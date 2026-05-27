#pragma once

#include "ValidMoves.h"

namespace ValidMovesInternal {
inline constexpr int kZero = 0;
inline constexpr int kInvalidSquare = -1;
inline constexpr int kBoardSquareCount = kValidMovesBoardSquareCount;
inline constexpr int kBoardDimension = 8;
inline constexpr int kSinglePawnPush = 8;
inline constexpr int kDoublePawnPush = 16;
inline constexpr int kWhitePawnStartRank = 1;
inline constexpr int kBlackPawnStartRank = 6;
inline constexpr int kMinFile = 0;
inline constexpr int kMaxFile = kBoardDimension - 1;
inline constexpr int kWhiteEnPassantCaptureLeftOffset = 7;
inline constexpr int kWhiteEnPassantCaptureRightOffset = 9;
inline constexpr int kBlackEnPassantCaptureLeftOffset = 9;
inline constexpr int kBlackEnPassantCaptureRightOffset = 7;

inline constexpr int kWhiteKingStartSquare = 4;
inline constexpr int kBlackKingStartSquare = 60;
inline constexpr int kWhiteKingsideTransitSquare = 5;
inline constexpr int kWhiteKingsideDestination = 6;
inline constexpr int kWhiteQueensideFirstTransit = 3;
inline constexpr int kWhiteQueensideSecondTransit = 2;
inline constexpr int kWhiteQueensideEmptySquare = 1;
inline constexpr int kBlackKingsideTransitSquare = 61;
inline constexpr int kBlackKingsideDestination = 62;
inline constexpr int kBlackQueensideFirstTransit = 59;
inline constexpr int kBlackQueensideSecondTransit = 58;
inline constexpr int kBlackQueensideEmptySquare = 57;
inline constexpr int kWhiteKingsideRookSquare = 7;
inline constexpr int kWhiteQueensideRookSquare = 0;
inline constexpr int kBlackKingsideRookSquare = 63;
inline constexpr int kBlackQueensideRookSquare = 56;
inline constexpr std::size_t kTypicalMoveCapacity = 96;

struct CastlingSideConfig {
    ChessPieceColor color;
    int kingStart;
    int kingsideRookSquare;
    int queensideRookSquare;
    int kingsideTransitSquare;
    int kingsideDestination;
    int queensideEmptySquare;
    int queensideFirstTransit;
    int queensideSecondTransit;
    std::uint8_t kingsideCastlingRight;
    std::uint8_t queensideCastlingRight;
};

inline constexpr CastlingSideConfig kWhiteCastlingSide{
    ChessPieceColor::WHITE,
    kWhiteKingStartSquare,
    kWhiteKingsideRookSquare,
    kWhiteQueensideRookSquare,
    kWhiteKingsideTransitSquare,
    kWhiteKingsideDestination,
    kWhiteQueensideEmptySquare,
    kWhiteQueensideFirstTransit,
    kWhiteQueensideSecondTransit,
    CastlingConstants::kWhiteKingsideCastlingRight,
    CastlingConstants::kWhiteQueensideCastlingRight,
};

inline constexpr CastlingSideConfig kBlackCastlingSide{
    ChessPieceColor::BLACK,
    kBlackKingStartSquare,
    kBlackKingsideRookSquare,
    kBlackQueensideRookSquare,
    kBlackKingsideTransitSquare,
    kBlackKingsideDestination,
    kBlackQueensideEmptySquare,
    kBlackQueensideFirstTransit,
    kBlackQueensideSecondTransit,
    CastlingConstants::kBlackKingsideCastlingRight,
    CastlingConstants::kBlackQueensideCastlingRight,
};

inline void tryAppendEnPassantMove(const Board& board, ChessPieceColor color, int src,
                                   std::vector<Move>& moves) {
    const auto enPassantTarget = board.enPassantSquare.target();
    if (!enPassantTarget) {
        return;
    }

    const int epSquare = *enPassantTarget;
    const int srcFile = src % kBoardDimension;
    const ChessPieceColor enemyColor =
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    const bool canCaptureLeft =
        srcFile > kMinFile &&
        epSquare == (color == ChessPieceColor::WHITE
                         ? src + kWhiteEnPassantCaptureLeftOffset
                         : src - kBlackEnPassantCaptureLeftOffset);
    const bool canCaptureRight =
        srcFile < kMaxFile &&
        epSquare == (color == ChessPieceColor::WHITE
                         ? src + kWhiteEnPassantCaptureRightOffset
                         : src - kBlackEnPassantCaptureRightOffset);
    if (!canCaptureLeft && !canCaptureRight) {
        return;
    }

    const int capturedPawnSquare =
        epSquare + (color == ChessPieceColor::WHITE ? -kSinglePawnPush : kSinglePawnPush);
    if (capturedPawnSquare < kZero || capturedPawnSquare >= kBoardSquareCount) {
        return;
    }

    const Piece& capturedPawn = board.squares[capturedPawnSquare].piece;
    if (capturedPawn.PieceType != ChessPieceType::PAWN ||
        capturedPawn.PieceColor != enemyColor ||
        board.squares[epSquare].piece.PieceType != ChessPieceType::NONE) {
        return;
    }

    moves.emplace_back(src, epSquare);
}

bool canCastleKingside(const Board& board, const CastlingSideConfig& config, Bitboard occupancy);
bool canCastleQueenside(const Board& board, const CastlingSideConfig& config, Bitboard occupancy);

void appendPawnMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendKnightMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendBishopMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendRookMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendQueenMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendKingMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves);

void appendPawnCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendKnightCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendBishopCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendRookCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendQueenCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);
void appendKingCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves);

bool hasLegalMoveFromCandidates(Board& board, ChessPieceColor color,
                                const std::vector<Move>& moves);
} // namespace ValidMovesInternal
