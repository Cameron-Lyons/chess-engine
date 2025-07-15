#include "MagicBitboards.h"
#include <random>
#include <algorithm>

// Initialize static members
std::array<std::array<uint64_t, 4096>, 64> MagicBitboards::rookAttacks;
std::array<std::array<uint64_t, 512>, 64> MagicBitboards::bishopAttacks;
std::array<uint64_t, 64> MagicBitboards::rookMasks;
std::array<uint64_t, 64> MagicBitboards::bishopMasks;

// Magic numbers for rook and bishop
const std::array<uint64_t, 64> MagicBitboards::ROOK_MAGIC = {{
    0x8a80104000800020ULL, 0x140002000100040ULL, 0x2801880a0017001ULL, 0x100081001000420ULL,
    0x200020010080420ULL, 0x3001c0002010008ULL, 0x8480008002000100ULL, 0x2080088004402900ULL,
    0x800098204000ULL, 0x2024401000200040ULL, 0x100802000801000ULL, 0x120800800801000ULL,
    0x208808088000400ULL, 0x2802200800400ULL, 0x2200800100020080ULL, 0x801000060821100ULL,
    0x80044006422000ULL, 0x100808020004000ULL, 0x12108a0010204200ULL, 0x140848010000802ULL,
    0x481828014002800ULL, 0x8094004002004100ULL, 0x4010040010010802ULL, 0x20008806104ULL,
    0x100400080208000ULL, 0x2040002120081000ULL, 0x21200680100080ULL, 0x20100080080080ULL,
    0x2000a00200410ULL, 0x20080800400ULL, 0x80088400100102ULL, 0x80004600042881ULL,
    0x4040008040800020ULL, 0x440003000200801ULL, 0x4200011004500ULL, 0x188020010100100ULL,
    0x14800401802800ULL, 0x2080040080800200ULL, 0x124080204001001ULL, 0x200046502000484ULL,
    0x480400080088020ULL, 0x1000422010034000ULL, 0x30200100110040ULL, 0x100021010009ULL,
    0x2002080100110004ULL, 0x202008004008002ULL, 0x20020004010100ULL, 0x2048440040820001ULL,
    0x101002200408200ULL, 0x40802000401080ULL, 0x4008142004410100ULL, 0x2060820c0120200ULL,
    0x1001004080100ULL, 0x20c020080040080ULL, 0x2935610830022400ULL, 0x44440041009200ULL,
    0x280001040802101ULL, 0x2100190040002085ULL, 0x80c0084100102001ULL, 0x4024081001000421ULL
}};

const std::array<uint64_t, 64> MagicBitboards::BISHOP_MAGIC = {{
    0x40040844404084ULL, 0x2004208a004208ULL, 0x10190041080202ULL, 0x108060845042010ULL,
    0x581104180800210ULL, 0x2112080446200010ULL, 0x1080820820060210ULL, 0x3c0808410220200ULL,
    0x4050404440404ULL, 0x21001420088ULL, 0x24d0080801082102ULL, 0x1020a0a020400ULL,
    0x40308200402ULL, 0x4011002100800ULL, 0x401484104104005ULL, 0x801010402020200ULL,
    0x400210c3880100ULL, 0x404022024108200ULL, 0x810018200204102ULL, 0x4002801a02003ULL,
    0x85040820080400ULL, 0x810102c808880400ULL, 0xe900410884800ULL, 0x8002020480840102ULL,
    0x220200865090201ULL, 0x2010100a02021202ULL, 0x152048408022401ULL, 0x20080002081110ULL,
    0x4001001021004000ULL, 0x800040400a011002ULL, 0xe4004081011002ULL, 0x1c004001012080ULL,
    0x8004200962a00220ULL, 0x8422100208500202ULL, 0x2000402200300c08ULL, 0x8646020080080080ULL,
    0x80020a0200100808ULL, 0x2010004880111000ULL, 0x623000a080011400ULL, 0x42008c0340209202ULL,
    0x209188240001000ULL, 0x400408a884001800ULL, 0x110400a6080400ULL, 0x1840060a44020800ULL,
    0x90080104000041ULL, 0x201011000808101ULL, 0x1a2208080504f080ULL, 0x8012020600211212ULL,
    0x500861011240000ULL, 0x180806108200800ULL, 0x4000020e01040044ULL, 0x300000261044000aULL,
    0x802241102020002ULL, 0x20906061210001ULL, 0x5a84841004010310ULL, 0x4010801011c04ULL
}};

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