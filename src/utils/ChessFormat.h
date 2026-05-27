#pragma once

#include "../core/ChessPiece.h"
#include "../core/Move.h"

#include <format>
#include <string>

namespace chess::format {

inline constexpr char kMinFileChar = 'a';
inline constexpr char kMinRankChar = '1';
inline constexpr int kBoardDimension = 8;

[[nodiscard]] inline std::string squareName(int square) {
    if (square < 0 || square >= 64) {
        return "??";
    }
    return std::format("{}{}", static_cast<char>(kMinFileChar + (square % kBoardDimension)),
                       static_cast<char>(kMinRankChar + (square / kBoardDimension)));
}

[[nodiscard]] inline std::string moveToUci(const Move& move) {
    if (!move.isValid()) {
        return "0000";
    }
    return std::format("{}{}{}{}", static_cast<char>(kMinFileChar + (static_cast<int>(move.first) % kBoardDimension)),
                       static_cast<char>(kMinRankChar + (static_cast<int>(move.first) / kBoardDimension)),
                       static_cast<char>(kMinFileChar + (static_cast<int>(move.second) % kBoardDimension)),
                       static_cast<char>(kMinRankChar + (static_cast<int>(move.second) / kBoardDimension)));
}

[[nodiscard]] inline std::string pieceLabel(const Piece& piece) {
    if (piece.PieceType == ChessPieceType::NONE) {
        return ".";
    }
    return std::format("{}{}", piece.PieceColor == ChessPieceColor::WHITE ? 'W' : 'B',
                       static_cast<int>(piece.PieceType));
}

[[nodiscard]] inline std::string debugMove(int from, int to) {
    return std::format("Move from {} to {}", from, to);
}

} // namespace chess::format
