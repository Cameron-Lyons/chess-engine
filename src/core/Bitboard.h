#pragma once

#include <bit>
#include <cstdint>

using Bitboard = uint64_t;

constexpr Bitboard EMPTY = 0ULL;
constexpr Bitboard FULL = ~0ULL;

constexpr int BOARD_SIZE = 8;
constexpr int NUM_SQUARES = 64;

inline int popcount(Bitboard b) {
    return std::popcount(b);
}

inline int lsb(Bitboard b) {
    return b ? std::countr_zero(b) : -1;
}

inline int msb(Bitboard b) {
    return b ? std::bit_width(b) - 1 : -1;
}

inline void set_bit(Bitboard& b, int sq) {
    b |= (1ULL << sq);
}

inline void clear_bit(Bitboard& b, int sq) {
    b &= ~(1ULL << sq);
}

inline bool get_bit(Bitboard b, int sq) {
    return (b >> sq) & 1ULL;
}