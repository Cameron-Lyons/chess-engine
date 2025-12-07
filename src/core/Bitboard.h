#ifndef BITBOARD_H
#define BITBOARD_H

#include <cstdint>

using Bitboard = uint64_t;

constexpr Bitboard EMPTY = 0ULL;
constexpr Bitboard FULL = ~0ULL;

inline int popcount(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(b);
#else
    int count = 0;
    while (b) {
        b &= b - 1;
        ++count;
    }
    return count;
#endif
}

inline int lsb(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return b ? __builtin_ctzll(b) : -1;
#else
    if (!b)
        return -1;
    int idx = 0;
    while ((b & 1) == 0) {
        b >>= 1;
        ++idx;
    }
    return idx;
#endif
}

inline int msb(Bitboard b) {
#if defined(__GNUC__) || defined(__clang__)
    return b ? 63 - __builtin_clzll(b) : -1;
#else
    if (!b)
        return -1;
    int idx = 63;
    while ((b & (1ULL << 63)) == 0) {
        b <<= 1;
        --idx;
    }
    return idx;
#endif
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

#endif