#include "NNUEOptimized.h"

#include <algorithm>
#include <cpuid.h>
#include <cstring>
#include <fstream>

namespace NNUEOptimized {

std::unique_ptr<NNUEEvaluator> globalEvaluator;

static bool hasAVX512() {
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        return (ebx & bit_AVX512F) != 0;
    }
    return false;
}

static bool hasAVX512VNNI() {
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid_max(0, nullptr) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        return (ecx & bit_AVX512VNNI) != 0;
    }
    return false;
}

void Accumulator::refresh(const BitboardPosition& pos) {
    reset();

    for (int color = 0; color < 2; ++color) {
        ChessPieceColor c = color == 0 ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;

        Bitboard kingBB = pos.getPieceBitboard(ChessPieceType::KING, c);
        int kingSquare = __builtin_ctzll(kingBB);
        int kingBucket = FeatureTransformer::getKingBucket(kingSquare);

        for (int pt = 0; pt < 6; ++pt) {
            ChessPieceType pieceType = static_cast<ChessPieceType>(pt);
            Bitboard pieceBB = pos.getPieceBitboard(pieceType, c);

            while (pieceBB) {
                int square = __builtin_ctzll(pieceBB);
                int featureIdx = FeatureTransformer::makeIndex(square, pt, color, kingBucket);

                addFeature(0, featureIdx);
                addFeature(1, featureIdx);

                pieceBB &= pieceBB - 1;
            }
        }
    }

    computed[0] = computed[1] = true;
}

void Accumulator::addFeature(int color, int featureIdx) {
    const int16_t* featureWeights = weights + featureIdx * L1_SIZE;
    int16_t* acc = perspective[color].data();

    for (int i = 0; i < L1_SIZE; i += 16) {
        __m256i acc_vec = _mm256_load_si256((__m256i*)(acc + i));
        __m256i weight_vec = _mm256_load_si256((__m256i*)(featureWeights + i));
        __m256i sum = _mm256_add_epi16(acc_vec, weight_vec);
        _mm256_store_si256((__m256i*)(acc + i), sum);
    }

    computed[color] = false;
}

void Accumulator::removeFeature(int color, int featureIdx) {
    const int16_t* featureWeights = weights + featureIdx * L1_SIZE;
    int16_t* acc = perspective[color].data();

    for (int i = 0; i < L1_SIZE; i += 16) {
        __m256i acc_vec = _mm256_load_si256((__m256i*)(acc + i));
        __m256i weight_vec = _mm256_load_si256((__m256i*)(featureWeights + i));
        __m256i diff = _mm256_sub_epi16(acc_vec, weight_vec);
        _mm256_store_si256((__m256i*)(acc + i), diff);
    }

    computed[color] = false;
}

void Accumulator::moveFeature(int color, int fromIdx, int toIdx) {
    const int16_t* fromWeights = weights + fromIdx * L1_SIZE;
    const int16_t* toWeights = weights + toIdx * L1_SIZE;
    int16_t* acc = perspective[color].data();

    for (int i = 0; i < L1_SIZE; i += 16) {
        __m256i acc_vec = _mm256_load_si256((__m256i*)(acc + i));
        __m256i from_vec = _mm256_load_si256((__m256i*)(fromWeights + i));
        __m256i to_vec = _mm256_load_si256((__m256i*)(toWeights + i));

        acc_vec = _mm256_sub_epi16(acc_vec, from_vec);
        acc_vec = _mm256_add_epi16(acc_vec, to_vec);

        _mm256_store_si256((__m256i*)(acc + i), acc_vec);
    }

    computed[color] = false;
}

LinearLayer::SIMDType LinearLayer::detectSIMD() {
    if (hasAVX512VNNI())
        return AVX512_VNNI;
    if (hasAVX512())
        return AVX512;
    return AVX2;
}

LinearLayer::LinearLayer(int in, int out)
    : inputSize(in), outputSize(out), weights(in * out), biases(out) {
    simdType = detectSIMD();
}

void LinearLayer::loadWeights(const int8_t* w, const int32_t* b) {
    std::copy(w, w + weights.size(), weights.begin());
    std::copy(b, b + biases.size(), biases.begin());
}

void LinearLayer::forward_avx2(const int8_t* input, int32_t* output) const {
    for (int i = 0; i < outputSize; ++i) {
        __m256i sum = _mm256_setzero_si256();
        const int8_t* w = weights.data() + i * inputSize;

        for (int j = 0; j < inputSize; j += 32) {
            __m256i in_vec = _mm256_load_si256((__m256i*)(input + j));
            __m256i w_vec = _mm256_load_si256((__m256i*)(w + j));
            sum = SIMDOps::dpbusd_epi32(sum, in_vec, w_vec);
        }

        output[i] = SIMDOps::hadd_epi32_avx2(sum) + biases[i];
    }
}

void LinearLayer::forward_avx512(const int8_t* input, int32_t* output) const {
#ifdef __AVX512F__
    for (int i = 0; i < outputSize; ++i) {
        __m512i sum = _mm512_setzero_si512();
        const int8_t* w = weights.data() + i * inputSize;

        for (int j = 0; j < inputSize; j += 64) {
            __m512i in_vec = _mm512_load_si512((__m512i*)(input + j));
            __m512i w_vec = _mm512_load_si512((__m512i*)(w + j));

            __m512i in_lo = _mm512_unpacklo_epi8(in_vec, _mm512_setzero_si512());
            __m512i in_hi = _mm512_unpackhi_epi8(in_vec, _mm512_setzero_si512());
            __m512i w_lo = _mm512_unpacklo_epi8(w_vec, _mm512_setzero_si512());
            __m512i w_hi = _mm512_unpackhi_epi8(w_vec, _mm512_setzero_si512());

            sum = _mm512_add_epi32(sum, _mm512_madd_epi16(in_lo, w_lo));
            sum = _mm512_add_epi32(sum, _mm512_madd_epi16(in_hi, w_hi));
        }

        output[i] = _mm512_reduce_add_epi32(sum) + biases[i];
    }
#else
    forward_avx2(input, output);
#endif
}

void LinearLayer::forward_avx512_vnni(const int8_t* input, int32_t* output) const {
#ifdef __AVX512VNNI__
    for (int i = 0; i < outputSize; ++i) {
        __m512i sum = _mm512_setzero_si512();
        const int8_t* w = weights.data() + i * inputSize;

        for (int j = 0; j < inputSize; j += 64) {
            __m512i in_vec = _mm512_load_si512((__m512i*)(input + j));
            __m512i w_vec = _mm512_load_si512((__m512i*)(w + j));
            sum = _mm512_dpbusd_epi32(sum, in_vec, w_vec);
        }

        output[i] = _mm512_reduce_add_epi32(sum) + biases[i];
    }
#else
    forward_avx512(input, output);
#endif
}

void LinearLayer::forward(const void* input, void* output) const {
    const int8_t* in = static_cast<const int8_t*>(input);
    int32_t* out = static_cast<int32_t*>(output);

    switch (simdType) {
        case AVX512_VNNI:
            forward_avx512_vnni(in, out);
            break;
        case AVX512:
            forward_avx512(in, out);
            break;
        default:
            forward_avx2(in, out);
            break;
    }
}

void ClippedReLU::forward_avx2(const int32_t* input, int8_t* output) const {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_val = _mm256_set1_epi32(QA);

    for (int i = 0; i < size; i += 8) {
        __m256i val = _mm256_load_si256((__m256i*)(input + i));

        val = _mm256_max_epi32(val, zero);
        val = _mm256_min_epi32(val, max_val);

        val = _mm256_srai_epi32(val, SHIFT);

        __m128i val_lo = _mm256_castsi256_si128(val);
        __m128i val_hi = _mm256_extracti128_si256(val, 1);
        __m128i packed = _mm_packs_epi32(val_lo, val_hi);
        packed = _mm_packs_epi16(packed, packed);

        _mm_storel_epi64((__m128i*)(output + i), packed);
    }
}

void ClippedReLU::forward_avx512(const int32_t* input, int8_t* output) const {
#ifdef __AVX512F__
    const __m512i zero = _mm512_setzero_si512();
    const __m512i max_val = _mm512_set1_epi32(QA);

    for (int i = 0; i < size; i += 16) {
        __m512i val = _mm512_load_si512((__m512i*)(input + i));

        val = _mm512_max_epi32(val, zero);
        val = _mm512_min_epi32(val, max_val);

        val = _mm512_srai_epi32(val, SHIFT);

        __m128i packed = _mm512_cvtepi32_epi8(val);
        _mm_store_si128((__m128i*)(output + i), packed);
    }
#else
    forward_avx2(input, output);
#endif
}

void ClippedReLU::forward(const void* input, void* output) const {
    const int32_t* in = static_cast<const int32_t*>(input);
    int8_t* out = static_cast<int8_t*>(output);

    if (hasAVX512()) {
        forward_avx512(in, out);
    } else {
        forward_avx2(in, out);
    }
}

NNUEEvaluator::NNUEEvaluator() {
    fc1 = std::make_unique<LinearLayer>(2 * L1_SIZE, L2_SIZE);
    ac1 = std::make_unique<ClippedReLU>(L2_SIZE);
    fc2 = std::make_unique<LinearLayer>(L2_SIZE, L3_SIZE);
    ac2 = std::make_unique<ClippedReLU>(L3_SIZE);
    fc3 = std::make_unique<LinearLayer>(L3_SIZE, L3_SIZE);
    fc4 = std::make_unique<LinearLayer>(L3_SIZE, OUTPUT_SIZE);

    featureWeights.resize(INPUT_DIMENSIONS * L1_SIZE);
}

NNUEEvaluator::~NNUEEvaluator() = default;

bool NNUEEvaluator::loadNetwork(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        return false;

    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != 0x4E4E5545 || version != 2) {
        return false;
    }

    file.read(reinterpret_cast<char*>(featureWeights.data()),
              featureWeights.size() * sizeof(int16_t));

    accumulator.init(featureWeights.data());

    return true;
}

void NNUEEvaluator::transformInput(const BitboardPosition& pos, int8_t* output) const {

    int perspective = pos.getSideToMove() == ChessPieceColor::WHITE ? 0 : 1;

    if (!accumulator.isComputed(perspective)) {
        accumulator.refresh(pos);
    }

    const int16_t* acc = accumulator.getAccumulation(perspective);

    const int SHIFT_BITS = 6;
    for (int i = 0; i < 2 * L1_SIZE; ++i) {
        int32_t val = acc[i];
        val = std::max(0, std::min(QA, val));
        output[i] = static_cast<int8_t>(val >> SHIFT_BITS);
    }
}

int NNUEEvaluator::evaluate(const BitboardPosition& pos) const {
    alignas(SIMD_ALIGN) int8_t input[2 * L1_SIZE];
    alignas(SIMD_ALIGN) int32_t hidden1[L2_SIZE];
    alignas(SIMD_ALIGN) int8_t hidden1_relu[L2_SIZE];
    alignas(SIMD_ALIGN) int32_t hidden2[L3_SIZE];
    alignas(SIMD_ALIGN) int8_t hidden2_relu[L3_SIZE];
    alignas(SIMD_ALIGN) int32_t hidden3[L3_SIZE];
    alignas(SIMD_ALIGN) int8_t hidden3_relu[L3_SIZE];
    alignas(SIMD_ALIGN) int32_t output[OUTPUT_SIZE];

    transformInput(pos, input);

    fc1->forward(input, hidden1);
    ac1->forward(hidden1, hidden1_relu);

    fc2->forward(hidden1_relu, hidden2);
    ac2->forward(hidden2, hidden2_relu);

    fc3->forward(hidden2_relu, hidden3);

    const int SHIFT_BITS = 6;
    for (int i = 0; i < L3_SIZE; ++i) {
        hidden3_relu[i] = static_cast<int8_t>(std::max(0, std::min(127, hidden3[i] >> SHIFT_BITS)));
    }

    fc4->forward(hidden3_relu, output);

    return output[0] * 100 / QAB;
}

void NNUEEvaluator::updateBeforeMove(const BitboardPosition& pos, int from, int to,
                                     ChessPieceType piece, ChessPieceType captured) {

    int color = pos.getColorAt(from) == ChessPieceColor::WHITE ? 0 : 1;
    int kingSquare = __builtin_ctzll(pos.getPieceBitboard(
        ChessPieceType::KING, color == 0 ? ChessPieceColor::WHITE : ChessPieceColor::BLACK));
    int kingBucket = FeatureTransformer::getKingBucket(kingSquare);

    int fromFeature =
        FeatureTransformer::makeIndex(from, static_cast<int>(piece) - 1, color, kingBucket);
    accumulator.removeFeature(0, fromFeature);
    accumulator.removeFeature(1, fromFeature);

    if (captured != ChessPieceType::NONE) {
        int capturedColor = 1 - color;
        int capturedFeature = FeatureTransformer::makeIndex(to, static_cast<int>(captured) - 1,
                                                            capturedColor, kingBucket);
        accumulator.removeFeature(0, capturedFeature);
        accumulator.removeFeature(1, capturedFeature);
    }
}

void NNUEEvaluator::updateAfterMove(const BitboardPosition& pos, int from, int to,
                                    ChessPieceType piece, ChessPieceType promotion) {
    (void)from;

    int color = pos.getColorAt(to) == ChessPieceColor::WHITE ? 0 : 1;
    int kingSquare = __builtin_ctzll(pos.getPieceBitboard(
        ChessPieceType::KING, color == 0 ? ChessPieceColor::WHITE : ChessPieceColor::BLACK));
    int kingBucket = FeatureTransformer::getKingBucket(kingSquare);

    ChessPieceType finalPiece = promotion != ChessPieceType::NONE ? promotion : piece;
    int toFeature =
        FeatureTransformer::makeIndex(to, static_cast<int>(finalPiece) - 1, color, kingBucket);
    accumulator.addFeature(0, toFeature);
    accumulator.addFeature(1, toFeature);
}

bool init(const std::string& networkPath) {
    globalEvaluator = std::make_unique<NNUEEvaluator>();
    return globalEvaluator->loadNetwork(networkPath);
}

int evaluate(const BitboardPosition& pos) {
    if (!globalEvaluator)
        return 0;
    return globalEvaluator->evaluate(pos);
}

} // namespace NNUEOptimized