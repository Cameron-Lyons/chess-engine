#pragma once

#include <array>
#include <cstdint>

namespace SearchInternal {

inline constexpr std::uint64_t kZobristSeed = 202406ULL;

constexpr std::uint64_t zobristNext(std::uint64_t& state) {
    state = state * 6364136223846793005ULL + 1ULL;
    return (state >> 11U) * 2862933555777941757ULL;
}

struct ZobristTables {
    std::array<std::array<std::uint64_t, 12>, 64> table{};
    std::uint64_t blackToMove = 0;
    std::array<std::uint64_t, 16> castling{};
    std::array<std::uint64_t, 64> enPassant{};
};

consteval ZobristTables makeZobristTables() {
    ZobristTables keys{};
    std::uint64_t state = kZobristSeed;
    for (auto& squareKeys : keys.table) {
        for (auto& pieceKey : squareKeys) {
            pieceKey = zobristNext(state);
        }
    }
    keys.blackToMove = zobristNext(state);
    for (auto& castlingKey : keys.castling) {
        castlingKey = zobristNext(state);
    }
    for (auto& epKey : keys.enPassant) {
        epKey = zobristNext(state);
    }
    return keys;
}

inline constexpr ZobristTables kZobristKeys = makeZobristTables();
inline constexpr auto& ZobristTable = kZobristKeys.table;
inline constexpr std::uint64_t ZobristBlackToMove = kZobristKeys.blackToMove;
inline constexpr auto& ZobristCastling = kZobristKeys.castling;
inline constexpr auto& ZobristEnPassant = kZobristKeys.enPassant;

} // namespace SearchInternal
