#ifndef MAGIC_BITBOARDS_H
#define MAGIC_BITBOARDS_H

#include "Bitboard.h"
#include <array>
#include <vector>

// Magic bitboard implementation for sliding pieces
class MagicBitboards {
public:
    // Magic numbers for rook and bishop
    static const std::array<uint64_t, 64> ROOK_MAGIC;
    static const std::array<uint64_t, 64> BISHOP_MAGIC;

    // Attack tables
    static std::array<std::array<uint64_t, 4096>, 64> rookAttacks;
    static std::array<std::array<uint64_t, 512>, 64> bishopAttacks;
    
    // Precomputed masks
    static std::array<uint64_t, 64> rookMasks;
    static std::array<uint64_t, 64> bishopMasks;
    
    // Initialization
    static void initialize();
    
    // Get attacks for sliding pieces
    static uint64_t getRookAttacks(int square, uint64_t occupancy);
    static uint64_t getBishopAttacks(int square, uint64_t occupancy);
    static uint64_t getQueenAttacks(int square, uint64_t occupancy);
    
    // Generate attack masks
    static uint64_t generateRookMask(int square);
    static uint64_t generateBishopMask(int square);
    
    // Generate attacks for a given occupancy
    static uint64_t generateRookAttacks(int square, uint64_t occupancy);
    static uint64_t generateBishopAttacks(int square, uint64_t occupancy);
    
    // Find magic numbers (for initialization)
    static uint64_t findMagicNumber(int square, bool isRook);
    static bool isMagicNumberValid(int square, uint64_t magic, bool isRook);

private:
    // Helper functions
    static uint64_t generateSlidingAttacks(int square, const std::vector<std::pair<int, int>>& directions, uint64_t occupancy);
    static std::vector<uint64_t> generateOccupancyVariations(uint64_t mask);
};

#endif // MAGIC_BITBOARDS_H 