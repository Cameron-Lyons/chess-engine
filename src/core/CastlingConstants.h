#pragma once

#include <cstdint>

namespace CastlingConstants {
inline constexpr int kWhiteQueensideRookSquare = 0;
inline constexpr int kWhiteKingStartSquare = 4;
inline constexpr int kWhiteKingsideRookSquare = 7;
inline constexpr int kBlackQueensideRookSquare = 56;
inline constexpr int kBlackKingStartSquare = 60;
inline constexpr int kBlackKingsideRookSquare = 63;

inline constexpr int kWhiteKingStartCol = 4;
inline constexpr int kKingsideKingDestCol = 6;
inline constexpr int kQueensideKingDestCol = 2;
inline constexpr int kWhiteKingsideRookCastleDest = 5;
inline constexpr int kWhiteQueensideRookCastleDest = 3;
inline constexpr int kBlackKingsideRookCastleDest = 61;
inline constexpr int kBlackQueensideRookCastleDest = 59;

inline constexpr int kPawnDoublePushDistance = 16;
inline constexpr int kKingCastlingDistance = 2;
inline constexpr int kNoEnPassantSquareMailbox = -1;
inline constexpr int kNoEnPassantSquareBitboard = 64;

inline constexpr std::uint8_t kWhiteKingsideCastlingRight = 0x1;
inline constexpr std::uint8_t kWhiteQueensideCastlingRight = 0x2;
inline constexpr std::uint8_t kBlackKingsideCastlingRight = 0x4;
inline constexpr std::uint8_t kBlackQueensideCastlingRight = 0x8;
inline constexpr std::uint8_t kWhiteCastlingRightsMask =
    kWhiteKingsideCastlingRight | kWhiteQueensideCastlingRight;
inline constexpr std::uint8_t kBlackCastlingRightsMask =
    kBlackKingsideCastlingRight | kBlackQueensideCastlingRight;
inline constexpr std::uint8_t kAllCastlingRightsMask =
    kWhiteCastlingRightsMask | kBlackCastlingRightsMask;
} // namespace CastlingConstants
