#pragma once

#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#ifdef __SSE__
#include <xmmintrin.h>
#endif

namespace TTv2 {

using PackedMove = uint16_t;

constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kToShift = 6;
constexpr int kPromotionShift = 12;
constexpr int kSquareMask = 0x3F;
constexpr int kPromotionMask = 0x7;
constexpr int kBoundShift = 6;
constexpr int kBoundUpper = 1;
constexpr int kBoundLower = 2;
constexpr int kGenBoundMask = 0x3F;
constexpr int kBoundBitsMask = 0xC0;
constexpr int kGenerationIncrement = 4;
constexpr int kDepthReplacementMargin = 4;
constexpr int kReplacementAgeBonus = 8;
constexpr int kClusterBytes = 64;
constexpr int kClusterPaddingBytes = 28;
constexpr int kKeyHighShift = 32;
constexpr int kMulHiShift = 64;
constexpr int kHashfullSample = 1000;

inline PackedMove packMove(int from, int to, int promotion = kZero) {
    return static_cast<PackedMove>((from & kSquareMask) | ((to & kSquareMask) << kToShift) |
                                   ((promotion & kPromotionMask) << kPromotionShift));
}

inline void unpackMove(PackedMove move, int& from, int& to, int& promotion) {
    from = move & kSquareMask;
    to = (move >> kToShift) & kSquareMask;
    promotion = (move >> kPromotionShift) & kPromotionMask;
}

enum Bound : uint8_t {
    BOUND_NONE = kZero,
    BOUND_UPPER = kBoundUpper,
    BOUND_LOWER = kBoundLower,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

struct TTEntry {
    uint32_t key32;
    PackedMove move;
    int16_t value;
    int16_t eval;
    uint8_t genBound;
    int8_t depth;

    void save(uint64_t k, int16_t v, int16_t ev, Bound b, int8_t d, PackedMove m, uint8_t gen) {

        if ((d - ((genBound & kGenBoundMask) == gen ? kZero : kDepthReplacementMargin)) >
            depth - kDepthReplacementMargin) {
            key32 = static_cast<uint32_t>(k >> kKeyHighShift);
            move = m;
            value = v;
            eval = ev;
            genBound = static_cast<uint8_t>((gen & kGenBoundMask) | (b << kBoundShift));
            depth = d;
        }
    }

    uint8_t generation() const {
        return genBound & kGenBoundMask;
    }
    Bound bound() const {
        return static_cast<Bound>(genBound >> kBoundShift);
    }
};

static constexpr int CLUSTER_SIZE = 3;

struct TTCluster {
    TTEntry entry[CLUSTER_SIZE];
    uint8_t padding[kClusterPaddingBytes];

    void clear() {
        std::memset(this, kZero, sizeof(TTCluster));
    }
};

static_assert(sizeof(TTCluster) == kClusterBytes, "TTCluster must be 64 bytes");

class TranspositionTable {
private:
    size_t clusterCount = kZero;
    uint8_t generation8 = kZero;
    std::unique_ptr<TTCluster[]> table;

public:
    void resize(size_t mbSize) {
        size_t newClusterCount = (mbSize * 1024 * 1024) / sizeof(TTCluster);

        newClusterCount = std::bit_floor(newClusterCount);

        if (newClusterCount != clusterCount) {
            clusterCount = newClusterCount;
            table.reset(new TTCluster[clusterCount]);
            clear();
        }
    }

    void clear() {
        if (table) {
            std::memset(table.get(), kZero, clusterCount * sizeof(TTCluster));
        }
        generation8 = kZero;
    }

    TTEntry* probe(uint64_t key, bool& found) const {
        TTCluster* cluster = &table[mulhi64(key, clusterCount)];
        auto key32 = static_cast<uint32_t>(key >> kKeyHighShift);

        for (auto& i : cluster->entry) {
            if (i.key32 == key32 || !i.depth) {
                i.genBound = static_cast<uint8_t>(generation8 | (i.genBound & kBoundBitsMask));

                found = i.depth && i.key32 == key32;
                return &i;
            }
        }

        TTEntry* replace = &cluster->entry[kZero];
        for (int i = kOne; i < CLUSTER_SIZE; ++i) {
            if (replaceValue(cluster->entry[i]) < replaceValue(*replace)) {
                replace = &cluster->entry[i];
            }
        }

        found = false;
        return replace;
    }

    uint8_t generation() const {
        return generation8;
    }

    void newSearch() {
        generation8 += kGenerationIncrement;
    }

    void prefetch(uint64_t key) const {
#ifdef __SSE__
        char* addr = reinterpret_cast<char*>(&table[mulhi64(key, clusterCount)]);
        _mm_prefetch(addr, _MM_HINT_T0);
#else
        (void)key;
#endif
    }

    int hashfull() const {
        int cnt = kZero;
        for (int i = kZero; i < kHashfullSample / CLUSTER_SIZE; ++i) {
            for (auto& j : table[i].entry) {
                if (j.depth && j.generation() == generation8) {
                    ++cnt;
                }
            }
        }
        return cnt * kHashfullSample / (kHashfullSample / CLUSTER_SIZE * CLUSTER_SIZE);
    }

private:
    static uint64_t mulhi64(uint64_t a, uint64_t b) {
#ifdef __SIZEOF_INT128__
        return static_cast<uint64_t>((static_cast<__uint128_t>(a) * b) >> kMulHiShift);
#else

        uint64_t aLo = static_cast<uint32_t>(a);
        uint64_t aHi = a >> kKeyHighShift;
        uint64_t bLo = static_cast<uint32_t>(b);
        uint64_t bHi = b >> kKeyHighShift;

        uint64_t c1 = (aLo * bLo) >> kKeyHighShift;
        uint64_t c2 = aHi * bLo + c1;
        uint64_t c3 = aLo * bHi + static_cast<uint32_t>(c2);

        return aHi * bHi + (c2 >> kKeyHighShift) + (c3 >> kKeyHighShift);
#endif
    }

    int replaceValue(const TTEntry& e) const {
        return e.depth - (kReplacementAgeBonus * (e.generation() == generation8));
    }
};

extern TranspositionTable TT;

} // namespace TTv2
