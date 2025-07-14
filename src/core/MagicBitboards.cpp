#include "MagicBitboards.h"
#include <random>
#include <algorithm>

// Initialize static members
std::array<std::array<uint64_t, 4096>, 64> MagicBitboards::rookAttacks;
std::array<std::array<uint64_t, 512>, 64> MagicBitboards::bishopAttacks;
std::array<uint64_t, 64> MagicBitboards::rookMasks;
std::array<uint64_t, 64> MagicBitboards::bishopMasks;

void MagicBitboards::initialize() {
    // Initialize masks
    for (int square = 0; square < 64; ++square) {
        rookMasks[square] = generateRookMask(square);
        bishopMasks[square] = generateBishopMask(square);
    }
    
    // Initialize attack tables
    for (int square = 0; square < 64; ++square) {
        // Generate all possible occupancies for rook
        auto rookOccupancies = generateOccupancyVariations(rookMasks[square]);
        for (uint64_t occupancy : rookOccupancies) {
            uint64_t attacks = generateRookAttacks(square, occupancy);
            uint64_t index = (occupancy * ROOK_MAGIC[square]) >> 52;
            rookAttacks[square][index] = attacks;
        }
        
        // Generate all possible occupancies for bishop
        auto bishopOccupancies = generateOccupancyVariations(bishopMasks[square]);
        for (uint64_t occupancy : bishopOccupancies) {
            uint64_t attacks = generateBishopAttacks(square, occupancy);
            uint64_t index = (occupancy * BISHOP_MAGIC[square]) >> 55;
            bishopAttacks[square][index] = attacks;
        }
    }
}

uint64_t MagicBitboards::getRookAttacks(int square, uint64_t occupancy) {
    uint64_t relevantOccupancy = occupancy & rookMasks[square];
    uint64_t index = (relevantOccupancy * ROOK_MAGIC[square]) >> 52;
    return rookAttacks[square][index];
}

uint64_t MagicBitboards::getBishopAttacks(int square, uint64_t occupancy) {
    uint64_t relevantOccupancy = occupancy & bishopMasks[square];
    uint64_t index = (relevantOccupancy * BISHOP_MAGIC[square]) >> 55;
    return bishopAttacks[square][index];
}

uint64_t MagicBitboards::getQueenAttacks(int square, uint64_t occupancy) {
    return getRookAttacks(square, occupancy) | getBishopAttacks(square, occupancy);
}

uint64_t MagicBitboards::generateRookMask(int square) {
    uint64_t mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    // Add all squares in the same rank and file, excluding the square itself and edges
    for (int r = 0; r < 8; ++r) {
        if (r != rank) {
            set_bit(mask, r * 8 + file);
        }
    }
    for (int f = 0; f < 8; ++f) {
        if (f != file) {
            set_bit(mask, rank * 8 + f);
        }
    }
    
    // Remove edge squares
    if (rank != 0) clear_bit(mask, file);
    if (rank != 7) clear_bit(mask, 56 + file);
    if (file != 0) clear_bit(mask, rank * 8);
    if (file != 7) clear_bit(mask, rank * 8 + 7);
    
    return mask;
}

uint64_t MagicBitboards::generateBishopMask(int square) {
    uint64_t mask = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    // Add diagonal squares, excluding the square itself and edges
    for (int r = 0; r < 8; ++r) {
        for (int f = 0; f < 8; ++f) {
            if (r != rank && f != file && abs(r - rank) == abs(f - file)) {
                set_bit(mask, r * 8 + f);
            }
        }
    }
    
    // Remove edge squares
    if (rank != 0 && file != 0) clear_bit(mask, (rank - 1) * 8 + (file - 1));
    if (rank != 0 && file != 7) clear_bit(mask, (rank - 1) * 8 + (file + 1));
    if (rank != 7 && file != 0) clear_bit(mask, (rank + 1) * 8 + (file - 1));
    if (rank != 7 && file != 7) clear_bit(mask, (rank + 1) * 8 + (file + 1));
    
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

uint64_t MagicBitboards::generateSlidingAttacks(int square, const std::vector<std::pair<int, int>>& directions, uint64_t occupancy) {
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;
    
    for (const auto& [dr, df] : directions) {
        int r = rank + dr;
        int f = file + df;
        
        while (r >= 0 && r < 8 && f >= 0 && f < 8) {
            int targetSquare = r * 8 + f;
            set_bit(attacks, targetSquare);
            
            if (get_bit(occupancy, targetSquare)) {
                break; // Blocked by a piece
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

uint64_t MagicBitboards::findMagicNumber(int square, bool isRook) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    
    uint64_t mask = isRook ? generateRookMask(square) : generateBishopMask(square);
    auto occupancies = generateOccupancyVariations(mask);
    std::vector<uint64_t> attacks;
    
    for (uint64_t occupancy : occupancies) {
        uint64_t attack = isRook ? generateRookAttacks(square, occupancy) : generateBishopAttacks(square, occupancy);
        attacks.push_back(attack);
    }
    
    for (int attempt = 0; attempt < 1000000; ++attempt) {
        uint64_t magic = dist(gen) & dist(gen) & dist(gen);
        
        if (isMagicNumberValid(square, magic, isRook)) {
            return magic;
        }
    }
    
    return 0; // Failed to find magic number
}

bool MagicBitboards::isMagicNumberValid(int square, uint64_t magic, bool isRook) {
    uint64_t mask = isRook ? generateRookMask(square) : generateBishopMask(square);
    auto occupancies = generateOccupancyVariations(mask);
    std::vector<uint64_t> usedIndices(4096, 0ULL);
    
    for (uint64_t occupancy : occupancies) {
        uint64_t index = isRook ? 
            (occupancy * magic) >> 52 : 
            (occupancy * magic) >> 55;
        
        if (usedIndices[index] != 0ULL && usedIndices[index] != occupancy) {
            return false; // Collision detected
        }
        usedIndices[index] = occupancy;
    }
    
    return true;
} 