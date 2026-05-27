#pragma once

#include "Move.h"

#include <compare>
#include <optional>

namespace chess {

inline constexpr int kInvalidSquare = -1;
inline constexpr int kNoEnPassantSquare = -1;

[[nodiscard]] constexpr bool isValidSquare(int square) {
    return square >= 0 && square < 64;
}

[[nodiscard]] constexpr bool isValidSquare(SquareIndex square) {
    return square.isValid();
}

[[nodiscard]] constexpr Move invalidMove() {
    return Move::invalid();
}

struct EnPassantSquare {
    std::optional<int> square;

    constexpr EnPassantSquare() = default;

    constexpr explicit EnPassantSquare(int value)
        : square(isValidSquare(value) ? std::optional<int>{value} : std::nullopt) {}

    [[nodiscard]] constexpr bool hasTarget() const {
        return square.has_value();
    }

    [[nodiscard]] constexpr const std::optional<int>& target() const {
        return square;
    }

    constexpr EnPassantSquare& operator=(int value) {
        square = isValidSquare(value) ? std::optional<int>{value} : std::nullopt;
        return *this;
    }

    constexpr EnPassantSquare& operator=(std::nullopt_t) {
        square = std::nullopt;
        return *this;
    }

    [[nodiscard]] constexpr friend bool operator==(const EnPassantSquare& lhs, int rhs) {
        return lhs.square.has_value() && *lhs.square == rhs;
    }

    [[nodiscard]] constexpr friend bool operator==(int lhs, const EnPassantSquare& rhs) {
        return rhs == lhs;
    }

    [[nodiscard]] constexpr friend bool operator!=(const EnPassantSquare& lhs, int rhs) {
        return !(lhs == rhs);
    }

    [[nodiscard]] constexpr friend bool operator!=(int lhs, const EnPassantSquare& rhs) {
        return !(rhs == lhs);
    }

    auto operator<=>(const EnPassantSquare& other) const = default;
    bool operator==(const EnPassantSquare& other) const = default;
};

[[nodiscard]] constexpr EnPassantSquare noEnPassantSquare() {
    return EnPassantSquare{};
}

} // namespace chess
