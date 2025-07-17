#ifndef TRANSPOSITION_TABLE_V2_H
#define TRANSPOSITION_TABLE_V2_H

#include <cstdint>
#include <memory>
#include <atomic>
#include <cstring>
#ifdef __SSE__
#include <xmmintrin.h>
#endif

namespace TTv2 {

// Move encoding: from (6 bits) | to (6 bits) | promotion (3 bits)
using PackedMove = uint16_t;

inline PackedMove packMove(int from, int to, int promotion = 0) {
    return static_cast<PackedMove>((from & 0x3F) | ((to & 0x3F) << 6) | ((promotion & 0x7) << 12));
}

inline void unpackMove(PackedMove move, int& from, int& to, int& promotion) {
    from = move & 0x3F;
    to = (move >> 6) & 0x3F;
    promotion = (move >> 12) & 0x7;
}

// Bound types for TT entries
enum Bound : uint8_t {
    BOUND_NONE = 0,
    BOUND_UPPER = 1,
    BOUND_LOWER = 2,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

// Single TT entry - 16 bytes for cache efficiency
struct TTEntry {
    uint32_t key32;      // Upper 32 bits of zobrist key
    PackedMove move;     // Best move
    int16_t value;       // Evaluation score
    int16_t eval;        // Static evaluation
    uint8_t genBound;    // Generation (6 bits) + bound type (2 bits)
    int8_t depth;        // Search depth
    
    void save(uint64_t k, int16_t v, int16_t ev, Bound b, int8_t d, PackedMove m, uint8_t gen) {
        // Only update if new entry is more valuable
        if ((d - ((genBound & 0x3F) == gen ? 0 : 4)) > depth - 4) {
            key32 = static_cast<uint32_t>(k >> 32);
            move = m;
            value = v;
            eval = ev;
            genBound = static_cast<uint8_t>((gen & 0x3F) | (b << 6));
            depth = d;
        }
    }
    
    uint8_t generation() const { return genBound & 0x3F; }
    Bound bound() const { return static_cast<Bound>(genBound >> 6); }
};

// Cluster of TT entries for better cache usage
static constexpr int CLUSTER_SIZE = 3;

struct TTCluster {
    TTEntry entry[CLUSTER_SIZE];
    uint8_t padding[28]; // Align to 64 bytes (3*12 + 28 = 64)
    
    void clear() {
        std::memset(this, 0, sizeof(TTCluster));
    }
};

static_assert(sizeof(TTCluster) == 64, "TTCluster must be 64 bytes");

// Main transposition table class
class TranspositionTable {
private:
    size_t clusterCount;
    uint8_t generation8;
    std::unique_ptr<TTCluster[]> table;
    
public:
    // Initialize with size in MB
    void resize(size_t mbSize) {
        size_t newClusterCount = (mbSize * 1024 * 1024) / sizeof(TTCluster);
        
        // Round down to power of 2 for faster indexing
        newClusterCount = 1ULL << (63 - __builtin_clzll(newClusterCount));
        
        if (newClusterCount != clusterCount) {
            clusterCount = newClusterCount;
            table.reset(new TTCluster[clusterCount]);
            clear();
        }
    }
    
    void clear() {
        if (table) {
            std::memset(table.get(), 0, clusterCount * sizeof(TTCluster));
        }
        generation8 = 0;
    }
    
    // Probe transposition table
    TTEntry* probe(uint64_t key, bool& found) const {
        TTCluster* cluster = &table[mulhi64(key, clusterCount)];
        uint32_t key32 = static_cast<uint32_t>(key >> 32);
        
        // Search cluster for matching entry
        for (int i = 0; i < CLUSTER_SIZE; ++i) {
            if (cluster->entry[i].key32 == key32 || !cluster->entry[i].depth) {
                cluster->entry[i].genBound = static_cast<uint8_t>(
                    generation8 | (cluster->entry[i].genBound & 0xC0));
                
                found = cluster->entry[i].depth && cluster->entry[i].key32 == key32;
                return &cluster->entry[i];
            }
        }
        
        // Replace least valuable entry
        TTEntry* replace = &cluster->entry[0];
        for (int i = 1; i < CLUSTER_SIZE; ++i) {
            if (replaceValue(cluster->entry[i]) < replaceValue(*replace)) {
                replace = &cluster->entry[i];
            }
        }
        
        found = false;
        return replace;
    }
    
    // Get current generation
    uint8_t generation() const { return generation8; }
    
    // Increment generation for age-based replacement
    void newSearch() { generation8 += 4; } // Increment by 4 to keep 2 bits for bound
    
    // Prefetch cluster for upcoming probe
    void prefetch(uint64_t key) const {
        #ifdef __SSE__
        char* addr = reinterpret_cast<char*>(&table[mulhi64(key, clusterCount)]);
        _mm_prefetch(addr, _MM_HINT_T0);
        #endif
    }
    
    // Hash table usage in permille (0-1000)
    int hashfull() const {
        int cnt = 0;
        for (int i = 0; i < 1000 / CLUSTER_SIZE; ++i) {
            for (int j = 0; j < CLUSTER_SIZE; ++j) {
                if (table[i].entry[j].depth && 
                    table[i].entry[j].generation() == generation8) {
                    ++cnt;
                }
            }
        }
        return cnt * 1000 / (1000 / CLUSTER_SIZE * CLUSTER_SIZE);
    }
    
private:
    // Fast modulo using multiplication
    static uint64_t mulhi64(uint64_t a, uint64_t b) {
        #ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<__uint128_t>(a) * b) >> 64);
        #else
        // Fallback for systems without 128-bit integers
        uint64_t aLo = static_cast<uint32_t>(a);
        uint64_t aHi = a >> 32;
        uint64_t bLo = static_cast<uint32_t>(b);
        uint64_t bHi = b >> 32;
        
        uint64_t c1 = (aLo * bLo) >> 32;
        uint64_t c2 = aHi * bLo + c1;
        uint64_t c3 = aLo * bHi + static_cast<uint32_t>(c2);
        
        return aHi * bHi + (c2 >> 32) + (c3 >> 32);
        #endif
    }
    
    // Calculate replacement value (lower is more replaceable)
    int replaceValue(const TTEntry& e) const {
        return e.depth - 8 * (e.generation() == generation8);
    }
};

// Global TT instance
extern TranspositionTable TT;

} // namespace TTv2

#endif // TRANSPOSITION_TABLE_V2_H