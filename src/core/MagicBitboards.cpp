#include "MagicBitboards.h"
#include "Bitboard.h"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

std::array<std::array<uint64_t, 4096>, 64> MagicBitboards::rookAttacks{};
std::array<std::array<uint64_t, 512>, 64> MagicBitboards::bishopAttacks{};
std::array<uint64_t, 64> MagicBitboards::rookMasks{};
std::array<uint64_t, 64> MagicBitboards::bishopMasks{};

namespace {
uint64_t occupancyToIndex(uint64_t occupancy, uint64_t mask) {
    uint64_t index = 0;
    uint64_t tempMask = mask;
    int bit = 0;
    while (tempMask) {
        const int square = lsb(tempMask);
        if (get_bit(occupancy, square)) {
            index |= (1ULL << bit);
        }
        clear_bit(tempMask, square);
        ++bit;
    }
    return index;
}
} // namespace

void MagicBitboards::initialize() {
    for (int square = 0; square < 64; ++square) {
        rookMasks[square] = generateRookMask(square);
        bishopMasks[square] = generateBishopMask(square);
    }

    for (int square = 0; square < 64; ++square) {
        rookAttacks[square].fill(0);
        bishopAttacks[square].fill(0);

        const auto rookOccupancies = generateOccupancyVariations(rookMasks[square]);
        for (uint64_t occupancy : rookOccupancies) {
            const uint64_t attacks = generateRookAttacks(square, occupancy);
            const uint64_t index = occupancyToIndex(occupancy, rookMasks[square]);
            rookAttacks[square][index] = attacks;
        }

        const auto bishopOccupancies = generateOccupancyVariations(bishopMasks[square]);
        for (uint64_t occupancy : bishopOccupancies) {
            const uint64_t attacks = generateBishopAttacks(square, occupancy);
            const uint64_t index = occupancyToIndex(occupancy, bishopMasks[square]);
            bishopAttacks[square][index] = attacks;
        }
    }
}

uint64_t MagicBitboards::getRookAttacks(int square, uint64_t occupancy) {
    const uint64_t relevantOccupancy = occupancy & rookMasks[square];
    const uint64_t index = occupancyToIndex(relevantOccupancy, rookMasks[square]);
    return rookAttacks[square][index];
}

uint64_t MagicBitboards::getBishopAttacks(int square, uint64_t occupancy) {
    const uint64_t relevantOccupancy = occupancy & bishopMasks[square];
    const uint64_t index = occupancyToIndex(relevantOccupancy, bishopMasks[square]);
    return bishopAttacks[square][index];
}

uint64_t MagicBitboards::getQueenAttacks(int square, uint64_t occupancy) {
    return getRookAttacks(square, occupancy) | getBishopAttacks(square, occupancy);
}

constexpr uint64_t MagicBitboards::generateRookMask(int square) {
    uint64_t mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = 0; r < 8; ++r) {
        if (r != rank) {
            set_bit(mask, (r * 8) + file);
        }
    }
    for (int f = 0; f < 8; ++f) {
        if (f != file) {
            set_bit(mask, (rank * 8) + f);
        }
    }

    if (rank != 0) {
        clear_bit(mask, file);
    }
    if (rank != 7) {
        clear_bit(mask, 56 + file);
    }
    if (file != 0) {
        clear_bit(mask, rank * 8);
    }
    if (file != 7) {
        clear_bit(mask, (rank * 8) + 7);
    }
    return mask;
}

constexpr uint64_t MagicBitboards::generateBishopMask(int square) {
    uint64_t mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (int r = rank + 1, f = file + 1; r < 7 && f < 7; ++r, ++f) {
        set_bit(mask, (r * 8) + f);
    }
    for (int r = rank + 1, f = file - 1; r < 7 && f > 0; ++r, --f) {
        set_bit(mask, (r * 8) + f);
    }
    for (int r = rank - 1, f = file + 1; r > 0 && f < 7; --r, ++f) {
        set_bit(mask, (r * 8) + f);
    }
    for (int r = rank - 1, f = file - 1; r > 0 && f > 0; --r, --f) {
        set_bit(mask, (r * 8) + f);
    }

    return mask;
}

uint64_t MagicBitboards::generateRookAttacks(int square, uint64_t occupancy) {
    std::vector<std::pair<int, int>> directions = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    return generateSlidingAttacks(square, directions, occupancy);
}

uint64_t MagicBitboards::generateBishopAttacks(int square, uint64_t occupancy) {
    std::vector<std::pair<int, int>> directions = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    return generateSlidingAttacks(square, directions, occupancy);
}

uint64_t MagicBitboards::generateSlidingAttacks(int square,
                                                const std::vector<std::pair<int, int>>& directions,
                                                uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    for (const auto& [dr, df] : directions) {
        int r = rank + dr;
        int f = file + df;

        while (r >= 0 && r < 8 && f >= 0 && f < 8) {
            int targetSquare = (r * 8) + f;
            set_bit(attacks, targetSquare);

            if (get_bit(occupancy, targetSquare)) {
                break;
            }

            r += dr;
            f += df;
        }
    }

    return attacks;
}

std::vector<uint64_t> MagicBitboards::generateOccupancyVariations(uint64_t mask) {
    std::vector<uint64_t> variations;
    int numBits = popcount(mask);
    uint64_t maxVariations = 1ULL << numBits;

    for (uint64_t i = 0; i < maxVariations; ++i) {
        uint64_t variation = 0ULL;
        uint64_t tempMask = mask;
        uint64_t tempIndex = i;

        while (tempMask) {
            int square = lsb(tempMask);
            if (tempIndex & 1) {
                set_bit(variation, square);
            }
            clear_bit(tempMask, square);
            tempIndex >>= 1;
        }

        variations.push_back(variation);
    }

    return variations;
}
