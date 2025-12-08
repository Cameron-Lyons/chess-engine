#ifndef NNUE_OPTIMIZED_H
#define NNUE_OPTIMIZED_H

#include "core/BitboardOnly.h"
#include <array>
#include <cstdint>
#include <immintrin.h>
#include <memory>
#include <vector>

namespace NNUEOptimized {

constexpr int INPUT_DIMENSIONS = 768;
constexpr int L1_SIZE = 256;
constexpr int L2_SIZE = 32;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_SIZE = 1;

constexpr int QA = 255;
constexpr int QB = 64;
constexpr int QAB = QA * QB;

constexpr size_t SIMD_ALIGN = 64;

struct FeatureTransformer {
    static constexpr int KING_BUCKETS = 32;

    static int getKingBucket(int kingSquare) {

        int file = kingSquare & 7;
        int rank = kingSquare >> 3;

        if (file >= 4)
            file = 7 - file;

        return (rank * 4 + file);
    }

    static int makeIndex(int square, int piece, int color, int kingBucket) {
        return square + 64 * piece + 64 * 6 * color + 64 * 12 * kingBucket;
    }
};

class alignas(SIMD_ALIGN) Accumulator {
private:
    alignas(SIMD_ALIGN) std::array<int16_t, L1_SIZE> perspective[2];

    bool computed[2] = {false, false};

    const int16_t* weights = nullptr;

public:
    void init(const int16_t* featureWeights) {
        weights = featureWeights;
        reset();
    }

    void reset() {
        computed[0] = computed[1] = false;
        std::fill(perspective[0].begin(), perspective[0].end(), 0);
        std::fill(perspective[1].begin(), perspective[1].end(), 0);
    }

    void refresh(const BitboardPosition& pos);

    void addFeature(int color, int featureIdx);
    void removeFeature(int color, int featureIdx);
    void moveFeature(int color, int fromIdx, int toIdx);

    const int16_t* getAccumulation(int perspective) const {
        return this->perspective[perspective].data();
    }

    bool isComputed(int perspective) const {
        return computed[perspective];
    }

    void setComputed(int perspective) {
        computed[perspective] = true;
    }
};

class alignas(SIMD_ALIGN) LinearLayer {
private:
    const int inputSize;
    const int outputSize;
    alignas(SIMD_ALIGN) std::vector<int8_t> weights;
    alignas(SIMD_ALIGN) std::vector<int32_t> biases;

    enum SIMDType { AVX2, AVX512, AVX512_VNNI };
    static SIMDType detectSIMD();
    SIMDType simdType;

public:
    LinearLayer(int in, int out);

    void loadWeights(const int8_t* w, const int32_t* b);

    void forward_avx2(const int8_t* input, int32_t* output) const;
    void forward_avx512(const int8_t* input, int32_t* output) const;
    void forward_avx512_vnni(const int8_t* input, int32_t* output) const;

    void forward(const void* input, void* output) const;
};

class ClippedReLU {
private:
    const int size;
    static constexpr int SHIFT = 6;

public:
    explicit ClippedReLU(int s) : size(s) {}

    void forward_avx2(const int32_t* input, int8_t* output) const;
    void forward_avx512(const int32_t* input, int8_t* output) const;
    void forward(const void* input, void* output) const;
};

class NNUEEvaluator {
private:
    std::unique_ptr<LinearLayer> fc1;
    std::unique_ptr<ClippedReLU> ac1;
    std::unique_ptr<LinearLayer> fc2;
    std::unique_ptr<ClippedReLU> ac2;
    std::unique_ptr<LinearLayer> fc3;
    std::unique_ptr<LinearLayer> fc4;

    alignas(SIMD_ALIGN) std::vector<int16_t> featureWeights;

    mutable Accumulator accumulator;

    void transformInput(const BitboardPosition& pos, int8_t* output) const;

public:
    NNUEEvaluator();
    ~NNUEEvaluator();

    bool loadNetwork(const std::string& filename);

    int evaluate(const BitboardPosition& pos) const;

    void updateBeforeMove(const BitboardPosition& pos, int from, int to, ChessPieceType piece,
                          ChessPieceType captured);
    void updateAfterMove(const BitboardPosition& pos, int from, int to, ChessPieceType piece,
                         ChessPieceType promotion);

    void refreshAccumulator(const BitboardPosition& pos) {
        accumulator.refresh(pos);
    }
};

namespace SIMDOps {

inline int32_t hadd_epi32_avx2(__m256i v) {
    __m128i sum128 = _mm_add_epi32(_mm256_extracti128_si256(v, 1), _mm256_castsi256_si128(v));
    __m128i sum64 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
    __m128i sum32 = _mm_add_epi32(sum64, _mm_srli_si128(sum64, 4));
    return _mm_cvtsi128_si32(sum32);
}

inline __m256i dpbusd_epi32(__m256i acc, __m256i a, __m256i b) {
#ifdef __AVX512VNNI__
    return _mm256_dpbusd_epi32(acc, a, b);
#else

    __m256i a_lo = _mm256_unpacklo_epi8(a, _mm256_setzero_si256());
    __m256i a_hi = _mm256_unpackhi_epi8(a, _mm256_setzero_si256());
    __m256i b_lo = _mm256_unpacklo_epi8(b, _mm256_setzero_si256());
    __m256i b_hi = _mm256_unpackhi_epi8(b, _mm256_setzero_si256());

    __m256i prod_lo = _mm256_madd_epi16(a_lo, b_lo);
    __m256i prod_hi = _mm256_madd_epi16(a_hi, b_hi);

    return _mm256_add_epi32(acc, _mm256_add_epi32(prod_lo, prod_hi));
#endif
}

inline __m256i clip_epi32(__m256i v, int32_t max_val) {
    __m256i zero = _mm256_setzero_si256();
    __m256i max = _mm256_set1_epi32(max_val);
    v = _mm256_max_epi32(v, zero);
    v = _mm256_min_epi32(v, max);
    return v;
}
} // namespace SIMDOps

extern std::unique_ptr<NNUEEvaluator> globalEvaluator;

bool init(const std::string& networkPath);

int evaluate(const BitboardPosition& pos);

} // namespace NNUEOptimized

#endif