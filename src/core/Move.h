#pragma once

#include "ChessPiece.h"

#include <compare>

namespace chess {
inline constexpr int kInvalidSquare = -1;
} // namespace chess

struct SquareIndex {
    int value = chess::kInvalidSquare;

    constexpr SquareIndex() = default;
    explicit constexpr SquareIndex(int square) : value(square) {}

    constexpr SquareIndex& operator=(int square) {
        value = square;
        return *this;
    }

    [[nodiscard]] constexpr bool isValid() const {
        return value >= 0 && value < 64;
    }

    [[nodiscard]] constexpr bool operator==(int rhs) const {
        return value == rhs;
    }

    [[nodiscard]] constexpr bool operator!=(int rhs) const {
        return value != rhs;
    }

    constexpr operator int() const {
        return value;
    }

    auto operator<=>(const SquareIndex& other) const = default;
};

struct Move {
    SquareIndex first;
    SquareIndex second;
    ChessPieceType promotion = ChessPieceType::NONE;

    Move() = default;

    constexpr Move(int from, int to, ChessPieceType promo = ChessPieceType::NONE)
        : first(from), second(to), promotion(promo) {}

    constexpr Move(SquareIndex from, SquareIndex to, ChessPieceType promo = ChessPieceType::NONE)
        : first(from), second(to), promotion(promo) {}

    static constexpr Move invalid() {
        return Move();
    }

    [[nodiscard]] constexpr bool isValid() const {
        return first.isValid() && second.isValid();
    }

    std::strong_ordering operator<=>(const Move& other) const {
        if (const auto fromCmp = first <=> other.first; fromCmp != 0) {
            return fromCmp;
        }
        if (const auto toCmp = second <=> other.second; toCmp != 0) {
            return toCmp;
        }
        if (const auto promoCmp = promotion <=> other.promotion; promoCmp != 0) {
            return promoCmp;
        }
        return std::strong_ordering::equal;
    }

    bool operator==(const Move& other) const {
        return (*this <=> other) == std::strong_ordering::equal;
    }
};
