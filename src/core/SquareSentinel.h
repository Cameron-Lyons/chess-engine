#pragma once

#include "Move.h"

#include <compare>
#include <optional>

namespace chess {

inline constexpr int kBoardSquareCount = 64;

[[nodiscard]] constexpr bool isValidSquare(int square) {
    return square >= 0 && square < kBoardSquareCount;
}

[[nodiscard]] constexpr bool isValidSquare(SquareIndex square) {
    return square.isValid();
}

[[nodiscard]] constexpr Move invalidMove() {
    return Move::invalid();
}

struct EnPassantSquare {
    std::optional<SquareIndex> square;

    constexpr EnPassantSquare() = default;

    constexpr explicit EnPassantSquare(SquareIndex value)
        : square(value.isValid() ? std::optional{value} : std::nullopt) {}

    constexpr explicit EnPassantSquare(int value) : EnPassantSquare(SquareIndex(value)) {}

    [[nodiscard]] constexpr bool hasTarget() const {
        return square.has_value();
    }

    [[nodiscard]] constexpr const std::optional<SquareIndex>& target() const {
        return square;
    }

    constexpr EnPassantSquare& operator=(SquareIndex value) {
        square = value.isValid() ? std::optional{value} : std::nullopt;
        return *this;
    }

    constexpr EnPassantSquare& operator=(int value) {
        return *this = SquareIndex(value);
    }

    constexpr EnPassantSquare& operator=(std::nullopt_t) {
        square = std::nullopt;
        return *this;
    }

    [[nodiscard]] constexpr friend bool operator==(const EnPassantSquare& lhs, SquareIndex rhs) {
        return lhs.square.has_value() && lhs.square->value == rhs.value;
    }

    [[nodiscard]] constexpr friend bool operator==(SquareIndex lhs, const EnPassantSquare& rhs) {
        return rhs == lhs;
    }

    [[nodiscard]] constexpr friend bool operator==(const EnPassantSquare& lhs, int rhs) {
        return lhs.square.has_value() && lhs.square->value == rhs;
    }

    [[nodiscard]] constexpr friend bool operator==(int lhs, const EnPassantSquare& rhs) {
        return rhs == lhs;
    }

    [[nodiscard]] constexpr friend bool operator!=(const EnPassantSquare& lhs, SquareIndex rhs) {
        return !(lhs == rhs);
    }

    [[nodiscard]] constexpr friend bool operator!=(SquareIndex lhs, const EnPassantSquare& rhs) {
        return !(rhs == lhs);
    }

    [[nodiscard]] constexpr friend bool operator!=(const EnPassantSquare& lhs, int rhs) {
        return !(lhs == rhs);
    }

    [[nodiscard]] constexpr friend bool operator!=(int lhs, const EnPassantSquare& rhs) {
        return !(rhs == lhs);
    }

    [[nodiscard]] constexpr bool operator==(const EnPassantSquare& other) const {
        return square == other.square;
    }

    [[nodiscard]] constexpr bool operator!=(const EnPassantSquare& other) const {
        return !(*this == other);
    }
};

[[nodiscard]] constexpr EnPassantSquare noEnPassantSquare() {
    return EnPassantSquare{};
}

} // namespace chess
