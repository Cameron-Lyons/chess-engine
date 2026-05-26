#pragma once

#include "ChessPiece.h"

#include <compare>
#include <cstdint>
#include <utility>

struct SquareIndex {
    int value = -1;

    constexpr SquareIndex() = default;
    explicit constexpr SquareIndex(int square) : value(square) {}

    constexpr SquareIndex& operator=(int square) {
        value = square;
        return *this;
    }

    [[nodiscard]] constexpr bool isValid() const {
        return value >= 0 && value < 64;
    }

    constexpr operator int() const {
        return value;
    }

    auto operator<=>(const SquareIndex& other) const = default;
};

enum class MoveFlag : std::uint8_t {
    NONE = 0,
    CAPTURE = 1 << 0,
    EN_PASSANT = 1 << 1,
    CASTLE = 1 << 2,
    PROMOTION = 1 << 3,
    DOUBLE_PAWN_PUSH = 1 << 4,
};

constexpr MoveFlag operator|(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

constexpr MoveFlag operator&(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

constexpr bool hasFlag(MoveFlag flags, MoveFlag flag) {
    return (flags & flag) != MoveFlag::NONE;
}

struct Move {
    SquareIndex first;
    SquareIndex second;
    ChessPieceType promotion = ChessPieceType::NONE;
    MoveFlag flags = MoveFlag::NONE;

    Move() = default;

    constexpr Move(int from, int to, ChessPieceType promo = ChessPieceType::NONE,
                   MoveFlag moveFlags = MoveFlag::NONE)
        : first(SquareIndex(from)), second(SquareIndex(to)), promotion(promo), flags(moveFlags) {}

    constexpr Move(SquareIndex from, SquareIndex to, ChessPieceType promo = ChessPieceType::NONE,
                   MoveFlag moveFlags = MoveFlag::NONE)
        : first(from), second(to), promotion(promo), flags(moveFlags) {}

    static constexpr Move invalid() {
        return Move();
    }

    [[nodiscard]] constexpr bool isValid() const {
        return first.isValid() && second.isValid();
    }

    [[nodiscard]] std::uint32_t pack() const {
        std::uint32_t packed = 0;
        packed |= static_cast<std::uint32_t>(static_cast<int>(first) & 0x3F);
        packed |= static_cast<std::uint32_t>((static_cast<int>(second) & 0x3F) << 6);
        packed |= static_cast<std::uint32_t>((std::to_underlying(promotion) & 0x7) << 12);
        packed |= static_cast<std::uint32_t>((std::to_underlying(flags) & 0xFF) << 16);
        return packed;
    }

    static Move unpack(std::uint32_t packed) {
        const int from = static_cast<int>(packed & 0x3F);
        const int to = static_cast<int>((packed >> 6) & 0x3F);
        const auto promo = static_cast<ChessPieceType>((packed >> 12) & 0x7);
        const auto moveFlags = static_cast<MoveFlag>((packed >> 16) & 0xFF);
        return Move(from, to, promo, moveFlags);
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
        return static_cast<int>(flags) <=> static_cast<int>(other.flags);
    }

    bool operator==(const Move& other) const = default;
};
