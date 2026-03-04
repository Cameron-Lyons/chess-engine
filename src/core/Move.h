#pragma once

#include "ChessPiece.h"

#include <compare>
#include <cstdint>

enum class MoveFlag : std::uint8_t {
    NONE = 0,
    CAPTURE = 1 << 0,
    EN_PASSANT = 1 << 1,
    CASTLE = 1 << 2,
    PROMOTION = 1 << 3,
    DOUBLE_PAWN_PUSH = 1 << 4,
};

constexpr MoveFlag operator|(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr MoveFlag operator&(MoveFlag lhs, MoveFlag rhs) {
    return static_cast<MoveFlag>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

constexpr bool hasFlag(MoveFlag flags, MoveFlag flag) {
    return (flags & flag) != MoveFlag::NONE;
}

struct Move {
    int first;
    int second;
    ChessPieceType promotion;
    MoveFlag flags;

    Move() : first(-1), second(-1), promotion(ChessPieceType::NONE), flags(MoveFlag::NONE) {}

    Move(int from, int to, ChessPieceType promo = ChessPieceType::NONE,
         MoveFlag moveFlags = MoveFlag::NONE)
        : first(from), second(to), promotion(promo), flags(moveFlags) {}

    static Move invalid() {
        return Move();
    }

    [[nodiscard]] bool isValid() const {
        return first >= 0 && first < 64 && second >= 0 && second < 64;
    }

    [[nodiscard]] std::uint32_t pack() const {
        std::uint32_t packed = 0;
        packed |= static_cast<std::uint32_t>(first & 0x3F);
        packed |= static_cast<std::uint32_t>((second & 0x3F) << 6);
        packed |= static_cast<std::uint32_t>((static_cast<int>(promotion) & 0x7) << 12);
        packed |= static_cast<std::uint32_t>((static_cast<int>(flags) & 0xFF) << 16);
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
