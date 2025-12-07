#ifndef MAGIC_BITBOARDS_H
#define MAGIC_BITBOARDS_H

#include "Bitboard.h"
#include <array>
#include <vector>

class MagicBitboards {
public:
    static const std::array<uint64_t, 64> ROOK_MAGIC;
    static const std::array<uint64_t, 64> BISHOP_MAGIC;

    static std::array<std::array<uint64_t, 4096>, 64> rookAttacks;
    static std::array<std::array<uint64_t, 512>, 64> bishopAttacks;

    static std::array<uint64_t, 64> rookMasks;
    static std::array<uint64_t, 64> bishopMasks;

    static void initialize();

    static uint64_t getRookAttacks(int square, uint64_t occupancy);
    static uint64_t getBishopAttacks(int square, uint64_t occupancy);
    static uint64_t getQueenAttacks(int square, uint64_t occupancy);

    static uint64_t generateRookMask(int square);
    static uint64_t generateBishopMask(int square);

    static uint64_t generateRookAttacks(int square, uint64_t occupancy);
    static uint64_t generateBishopAttacks(int square, uint64_t occupancy);

    static uint64_t findMagicNumber(int square, bool isRook);
    static bool isMagicNumberValid(int square, uint64_t magic, bool isRook);

private:
    static uint64_t generateSlidingAttacks(int square,
                                           const std::vector<std::pair<int, int>>& directions,
                                           uint64_t occupancy);
    static std::vector<uint64_t> generateOccupancyVariations(uint64_t mask);
};

#endif