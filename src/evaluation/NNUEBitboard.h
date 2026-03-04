#pragma once

#include "core/BitboardOnly.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace NNUEBitboard {

constexpr int INPUT_DIMENSIONS = 768;
constexpr int L1_SIZE = 256;
constexpr int L2_SIZE = 32;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_SIZE = 1;
constexpr int COLOR_COUNT = 2;
constexpr int WHITE_PERSPECTIVE = 0;
constexpr int BLACK_PERSPECTIVE = 1;
constexpr int NO_INDEX = 0;
constexpr int ONE = 1;
constexpr int PIECE_TYPE_OFFSET = 1;

constexpr int QA = 255;
constexpr int QB = 64;
constexpr int QAB = QA * QB;

constexpr size_t SIMD_ALIGN = 64;

struct FeatureTransformer {
    static constexpr int KING_BUCKETS = 32;
    static constexpr int SQUARES_PER_PIECE = 64;
    static constexpr int PIECE_TYPES = 6;
    static constexpr int FILE_MASK = 7;
    static constexpr int RANK_SHIFT = 3;
    static constexpr int MIRROR_FILE_THRESHOLD = 4;
    static constexpr int MIRROR_FILE_BASE = 7;
    static constexpr int FILE_BUCKET_COUNT = 4;

    static int getKingBucket(int kingSquare) {
        int file = kingSquare & FILE_MASK;
        int rank = kingSquare >> RANK_SHIFT;

        if (file >= MIRROR_FILE_THRESHOLD) {
            file = MIRROR_FILE_BASE - file;
        }

        return (rank * FILE_BUCKET_COUNT) + file;
    }

    static int makeIndex(int square, int piece, int color, int kingBucket) {
        constexpr int kSquaresPerColor = SQUARES_PER_PIECE * PIECE_TYPES;
        constexpr int kSquaresPerKingBucket = kSquaresPerColor * COLOR_COUNT;
        return square + (SQUARES_PER_PIECE * piece) + (kSquaresPerColor * color) +
               (kSquaresPerKingBucket * kingBucket);
    }
};

class alignas(SIMD_ALIGN) Accumulator {
private:
    alignas(SIMD_ALIGN) std::array<int16_t, L1_SIZE> perspective[COLOR_COUNT];
    bool computed[COLOR_COUNT] = {false, false};
    const int16_t* weights = nullptr;

public:
    void init(const int16_t* featureWeights) {
        weights = featureWeights;
        reset();
    }

    void reset() {
        computed[WHITE_PERSPECTIVE] = computed[BLACK_PERSPECTIVE] = false;
        std::fill(perspective[WHITE_PERSPECTIVE].begin(), perspective[WHITE_PERSPECTIVE].end(),
                  NO_INDEX);
        std::fill(perspective[BLACK_PERSPECTIVE].begin(), perspective[BLACK_PERSPECTIVE].end(),
                  NO_INDEX);
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
    enum SIMDType : std::uint8_t { SCALAR, AVX2, AVX512, AVX512_VNNI };
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
    ~NNUEEvaluator() = default;
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

extern std::unique_ptr<NNUEEvaluator> globalEvaluator;

bool init(const std::string& networkPath);

int evaluate(const BitboardPosition& pos);

} // namespace NNUEBitboard
