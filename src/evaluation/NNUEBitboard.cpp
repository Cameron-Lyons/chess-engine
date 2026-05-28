#include "NNUEBitboard.h"
#include "Bitboard.h"
#include "BitboardOnly.h"
#include "ChessPiece.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <string>

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define NNUEBITBOARD_X86_SIMD 1
#ifdef __has_include
#if __has_include(<cpuid.h>)
#define NNUEBITBOARD_HAS_CPUID_H 1
#include <cpuid.h>
#else
#define NNUEBITBOARD_HAS_CPUID_H 0
#endif
#else
#define NNUEBITBOARD_HAS_CPUID_H 0
#endif
#include <immintrin.h>
#else
#define NNUEBITBOARD_X86_SIMD 0
#define NNUEBITBOARD_HAS_CPUID_H 0
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define NNUEBITBOARD_ARM_NEON 1
#include <arm_neon.h>
#else
#define NNUEBITBOARD_ARM_NEON 0
#endif

#if NNUEBITBOARD_ARM_NEON && defined(__APPLE__)
#include <sys/sysctl.h>
#endif

#if NNUEBITBOARD_ARM_NEON && defined(__linux__)
#include <sys/auxv.h>
#if defined(__has_include)
#if __has_include(<asm/hwcap.h>)
#include <asm/hwcap.h>
#define NNUEBITBOARD_HAS_LINUX_HWCAP 1
#else
#define NNUEBITBOARD_HAS_LINUX_HWCAP 0
#endif
#else
#define NNUEBITBOARD_HAS_LINUX_HWCAP 0
#endif
#else
#define NNUEBITBOARD_HAS_LINUX_HWCAP 0
#endif

#if NNUEBITBOARD_ARM_NEON
#ifdef __has_builtin
#if __has_builtin(__builtin_neon_vdotq_s32)
#define NNUEBITBOARD_HAS_VDOTQ 1
#else
#define NNUEBITBOARD_HAS_VDOTQ 0
#endif
#if __has_builtin(__builtin_neon_vusdotq_s32)
#define NNUEBITBOARD_HAS_VUSDOTQ 1
#else
#define NNUEBITBOARD_HAS_VUSDOTQ 0
#endif
#else
#define NNUEBITBOARD_HAS_VDOTQ 1
#define NNUEBITBOARD_HAS_VUSDOTQ 1
#endif
#else
#define NNUEBITBOARD_HAS_VDOTQ 0
#define NNUEBITBOARD_HAS_VUSDOTQ 0
#endif

namespace NNUEBitboard {

std::unique_ptr<NNUEEvaluator> globalEvaluator;

namespace {
constexpr unsigned int kCpuidBaseLeaf = 0;
#if NNUEBITBOARD_X86_SIMD && NNUEBITBOARD_HAS_CPUID_H
constexpr unsigned int kCpuidFeatureLeaf = 7;
constexpr unsigned int kCpuidFeatureSubLeaf = 0;
#endif
#if NNUEBITBOARD_X86_SIMD
constexpr int kAvx2FeatureStride = 16;
constexpr int kAvx2DotProductStride = 32;
constexpr int kAvx2ReluStride = 8;
#ifdef __AVX512F__
constexpr int kAvx512DotProductStride = 64;
constexpr int kAvx512ReluStride = 16;
#endif
#endif
#if NNUEBITBOARD_ARM_NEON
constexpr int kNeonFeatureStride = 8;
constexpr int kNeonDotProductStride = 16;
constexpr int kNeonReluStride = 8;
#endif
constexpr int kActivationShiftBits = 6;
constexpr int kMaxOutputActivation = 127;
constexpr int kScoreScale = 100;
constexpr uint32_t kNetworkMagic = 0x4E4E5545;
constexpr uint32_t kNetworkVersion = 2;

int32_t dotProductScalar(const int8_t* input, const int8_t* weights, int size) {
#if NNUEBITBOARD_ARM_NEON
    int32x4_t sumVec = vdupq_n_s32(0);
    int i = NO_INDEX;
    for (; i + kNeonDotProductStride <= size; i += kNeonDotProductStride) {
        const uint8x16_t inU8 =
            vld1q_u8(reinterpret_cast<const uint8_t*>(input + static_cast<std::ptrdiff_t>(i)));
        const int8x16_t wS8 = vld1q_s8(weights + static_cast<std::ptrdiff_t>(i));

        const int16x8_t inLo = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(inU8)));
        const int16x8_t inHi = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(inU8)));
        const int16x8_t wLo = vmovl_s8(vget_low_s8(wS8));
        const int16x8_t wHi = vmovl_s8(vget_high_s8(wS8));

        sumVec = vaddq_s32(sumVec, vmull_s16(vget_low_s16(inLo), vget_low_s16(wLo)));
        sumVec = vaddq_s32(sumVec, vmull_s16(vget_high_s16(inLo), vget_high_s16(wLo)));
        sumVec = vaddq_s32(sumVec, vmull_s16(vget_low_s16(inHi), vget_low_s16(wHi)));
        sumVec = vaddq_s32(sumVec, vmull_s16(vget_high_s16(inHi), vget_high_s16(wHi)));
    }

    int32_t sum = vaddvq_s32(sumVec);
    for (; i < size; ++i) {
        const auto activation =
            static_cast<int32_t>(static_cast<uint8_t>(input[static_cast<std::ptrdiff_t>(i)]));
        const auto weight = static_cast<int32_t>(weights[static_cast<std::ptrdiff_t>(i)]); // NOLINT
        sum += activation * weight;
    }
    return sum;
#else
    int32_t sum = 0;
    for (int i = NO_INDEX; i < size; ++i) {
        const auto activation = static_cast<int32_t>(static_cast<uint8_t>(input[i]));
        const auto weight = static_cast<int32_t>(weights[i]); // NOLINT(bugprone-signed-char-misuse)
        sum += activation * weight;
    }
    return sum;
#endif
}

#if !NNUEBITBOARD_X86_SIMD
void applyClippedReluScalar(const int32_t* input, int8_t* output, int size) {
#if NNUEBITBOARD_ARM_NEON
    int i = NO_INDEX;
    const int32x4_t zero = vdupq_n_s32(NO_INDEX);
    const int32x4_t maxVal = vdupq_n_s32(QA);
    for (; i + kNeonReluStride <= size; i += kNeonReluStride) {
        int32x4_t lo = vld1q_s32(input + static_cast<std::ptrdiff_t>(i));
        int32x4_t hi = vld1q_s32(input + static_cast<std::ptrdiff_t>(i + 4));
        lo = vshrq_n_s32(vminq_s32(vmaxq_s32(lo, zero), maxVal), kActivationShiftBits);
        hi = vshrq_n_s32(vminq_s32(vmaxq_s32(hi, zero), maxVal), kActivationShiftBits);

        int16x8_t packed16 = vcombine_s16(vqmovn_s32(lo), vqmovn_s32(hi));
        int8x8_t packed8 = vqmovn_s16(packed16);
        vst1_s8(output + static_cast<std::ptrdiff_t>(i), packed8);
    }

    for (; i < size; ++i) {
        const int32_t clamped = std::clamp(input[static_cast<std::ptrdiff_t>(i)], NO_INDEX, QA);
        output[static_cast<std::ptrdiff_t>(i)] =
            static_cast<int8_t>(clamped >> kActivationShiftBits);
    }
#else
    for (int i = NO_INDEX; i < size; ++i) {
        const int32_t clamped = std::clamp(input[i], NO_INDEX, QA);
        output[i] = static_cast<int8_t>(clamped >> kActivationShiftBits);
    }
#endif
}
#endif
} // namespace

#if NNUEBITBOARD_X86_SIMD
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
} // namespace SIMDOps
#endif

static bool hasAVX512() {
#if NNUEBITBOARD_X86_SIMD && NNUEBITBOARD_HAS_CPUID_H
    unsigned int eax = kCpuidBaseLeaf;
    unsigned int ebx = kCpuidBaseLeaf;
    unsigned int ecx = kCpuidBaseLeaf;
    unsigned int edx = kCpuidBaseLeaf;
    if (__get_cpuid_max(kCpuidBaseLeaf, nullptr) >= kCpuidFeatureLeaf) {
        __cpuid_count(kCpuidFeatureLeaf, kCpuidFeatureSubLeaf, eax, ebx, ecx, edx);
        return (ebx & bit_AVX512F) != kCpuidBaseLeaf;
    }
#endif
    return false;
}

#if NNUEBITBOARD_X86_SIMD && NNUEBITBOARD_HAS_CPUID_H
static bool hasAVX512VNNI() {
    unsigned int eax = kCpuidBaseLeaf;
    unsigned int ebx = kCpuidBaseLeaf;
    unsigned int ecx = kCpuidBaseLeaf;
    unsigned int edx = kCpuidBaseLeaf;
    if (__get_cpuid_max(kCpuidBaseLeaf, nullptr) >= kCpuidFeatureLeaf) {
        __cpuid_count(kCpuidFeatureLeaf, kCpuidFeatureSubLeaf, eax, ebx, ecx, edx);
        return (ecx & bit_AVX512VNNI) != kCpuidBaseLeaf;
    }
    return false;
}
#endif

#if NNUEBITBOARD_ARM_NEON
bool queryArmFeature(const char* featureName) {
#ifdef __APPLE__
    int value = 0;
    size_t size = sizeof(value);
    return sysctlbyname(featureName, &value, &size, nullptr, 0) == 0 && value != 0;
#else
    (void)featureName;
    return false;
#endif
}

bool hasArmDotProd() {
#ifdef __APPLE__
    return queryArmFeature("hw.optional.arm.FEAT_DotProd");
#elif NNUEBITBOARD_HAS_LINUX_HWCAP
#if defined(HWCAP_ASIMDDP) && defined(AT_HWCAP)
    const unsigned long hwcap = getauxval(AT_HWCAP);
    return (hwcap & HWCAP_ASIMDDP) != 0;
#else
    return false;
#endif
#else
    return false;
#endif
}

bool hasArmI8MM() {
#ifdef __APPLE__
    return queryArmFeature("hw.optional.arm.FEAT_I8MM");
#elif NNUEBITBOARD_HAS_LINUX_HWCAP
#if defined(HWCAP2_I8MM) && defined(AT_HWCAP2)
    const unsigned long hwcap2 = getauxval(AT_HWCAP2);
    return (hwcap2 & HWCAP2_I8MM) != 0;
#else
    return false;
#endif
#else
    return false;
#endif
}
#endif

void Accumulator::refresh(const BitboardPosition& pos) {
    reset();

    for (int color = WHITE_PERSPECTIVE; color < COLOR_COUNT; ++color) {
        ChessPieceColor c =
            color == WHITE_PERSPECTIVE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;

        Bitboard kingBB = pos.getPieceBitboard(ChessPieceType::KING, c);
        int kingSquare = std::countr_zero(kingBB);
        int kingBucket = FeatureTransformer::getKingBucket(kingSquare);

        for (int pt = NO_INDEX; pt < FeatureTransformer::PIECE_TYPES; ++pt) {
            auto pieceType = static_cast<ChessPieceType>(pt);
            Bitboard pieceBB = pos.getPieceBitboard(pieceType, c);

            while (pieceBB) {
                int square = std::countr_zero(pieceBB);
                int featureIdx = FeatureTransformer::makeIndex(square, pt, color, kingBucket);
                addFeature(WHITE_PERSPECTIVE, featureIdx);
                addFeature(BLACK_PERSPECTIVE, featureIdx);
                pieceBB &= pieceBB - ONE;
            }
        }
    }

    computed[WHITE_PERSPECTIVE] = computed[BLACK_PERSPECTIVE] = true;
}

void Accumulator::addFeature(int color, int featureIdx) {
    const auto* featureWeights =
        weights + (static_cast<std::ptrdiff_t>(featureIdx) * static_cast<std::ptrdiff_t>(L1_SIZE));
    auto* acc = perspective[color].data();

#if NNUEBITBOARD_X86_SIMD
    for (int i = NO_INDEX; i < L1_SIZE; i += kAvx2FeatureStride) {
        __m256i acc_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i));
        __m256i weight_vec =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(featureWeights + i));
        __m256i sum = _mm256_add_epi16(acc_vec, weight_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i), sum);
    }
#elif NNUEBITBOARD_ARM_NEON
    for (int i = NO_INDEX; i < L1_SIZE; i += kNeonFeatureStride) {
        int16x8_t accVec = vld1q_s16(acc + static_cast<std::ptrdiff_t>(i));
        int16x8_t weightVec = vld1q_s16(featureWeights + static_cast<std::ptrdiff_t>(i));
        vst1q_s16(acc + static_cast<std::ptrdiff_t>(i), vaddq_s16(accVec, weightVec));
    }
#else
    for (int i = NO_INDEX; i < L1_SIZE; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] + featureWeights[i]);
    }
#endif

    computed[color] = false;
}

void Accumulator::removeFeature(int color, int featureIdx) {
    const auto* featureWeights =
        weights + (static_cast<std::ptrdiff_t>(featureIdx) * static_cast<std::ptrdiff_t>(L1_SIZE));
    auto* acc = perspective[color].data();

#if NNUEBITBOARD_X86_SIMD
    for (int i = NO_INDEX; i < L1_SIZE; i += kAvx2FeatureStride) {
        __m256i acc_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i));
        __m256i weight_vec =
            _mm256_loadu_si256(reinterpret_cast<const __m256i*>(featureWeights + i));
        __m256i diff = _mm256_sub_epi16(acc_vec, weight_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i), diff);
    }
#elif NNUEBITBOARD_ARM_NEON
    for (int i = NO_INDEX; i < L1_SIZE; i += kNeonFeatureStride) {
        int16x8_t accVec = vld1q_s16(acc + static_cast<std::ptrdiff_t>(i));
        int16x8_t weightVec = vld1q_s16(featureWeights + static_cast<std::ptrdiff_t>(i));
        vst1q_s16(acc + static_cast<std::ptrdiff_t>(i), vsubq_s16(accVec, weightVec));
    }
#else
    for (int i = NO_INDEX; i < L1_SIZE; ++i) {
        acc[i] = static_cast<int16_t>(acc[i] - featureWeights[i]);
    }
#endif

    computed[color] = false;
}

void Accumulator::moveFeature(int color, int fromIdx, int toIdx) {
    const auto* fromWeights =
        weights + (static_cast<std::ptrdiff_t>(fromIdx) * static_cast<std::ptrdiff_t>(L1_SIZE));
    const auto* toWeights =
        weights + (static_cast<std::ptrdiff_t>(toIdx) * static_cast<std::ptrdiff_t>(L1_SIZE));
    auto* acc = perspective[color].data();

#if NNUEBITBOARD_X86_SIMD
    for (int i = NO_INDEX; i < L1_SIZE; i += kAvx2FeatureStride) {
        __m256i acc_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(acc + i));
        __m256i from_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(fromWeights + i));
        __m256i to_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(toWeights + i));
        acc_vec = _mm256_sub_epi16(acc_vec, from_vec);
        acc_vec = _mm256_add_epi16(acc_vec, to_vec);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(acc + i), acc_vec);
    }
#elif NNUEBITBOARD_ARM_NEON
    for (int i = NO_INDEX; i < L1_SIZE; i += kNeonFeatureStride) {
        int16x8_t accVec = vld1q_s16(acc + static_cast<std::ptrdiff_t>(i));
        int16x8_t fromVec = vld1q_s16(fromWeights + static_cast<std::ptrdiff_t>(i));
        int16x8_t toVec = vld1q_s16(toWeights + static_cast<std::ptrdiff_t>(i));
        accVec = vsubq_s16(accVec, fromVec);
        accVec = vaddq_s16(accVec, toVec);
        vst1q_s16(acc + static_cast<std::ptrdiff_t>(i), accVec);
    }
#else
    for (int i = NO_INDEX; i < L1_SIZE; ++i) {
        acc[i] = static_cast<int16_t>((acc[i] - fromWeights[i]) + toWeights[i]);
    }
#endif

    computed[color] = false;
}

LinearLayer::SIMDType LinearLayer::detectSIMD() {
#if NNUEBITBOARD_X86_SIMD
#if NNUEBITBOARD_HAS_CPUID_H
    if (hasAVX512VNNI()) {
        return AVX512_VNNI;
    }
    if (hasAVX512()) {
        return AVX512;
    }
#endif
    return AVX2;
#elif NNUEBITBOARD_ARM_NEON
#if NNUEBITBOARD_HAS_VUSDOTQ
    if (hasArmI8MM()) {
        return ARM_I8MM;
    }
#endif
#if NNUEBITBOARD_HAS_VDOTQ
    if (hasArmDotProd()) {
        return ARM_DOTPROD;
    }
#endif
    return ARM_NEON;
#else
    return SCALAR;
#endif
}

LinearLayer::LinearLayer(int in, int out)
    : inputSize(in), outputSize(out), weights(static_cast<size_t>(in) * static_cast<size_t>(out)),
      biases(out), dotprodBiasAdjust(out), simdType(detectSIMD()) {}

void LinearLayer::loadWeights(const int8_t* w, const int32_t* b) {
    std::copy(w, w + weights.size(), weights.begin());
    std::copy(b, b + biases.size(), biases.begin());

    // vdotq_s32 uses signed-signed products; inputs are unsigned activations in [0, 255].
    // Convert u8 -> s8 by subtracting 128 and compensate with this per-row constant:
    // sum(u*w) = sum((u-128)*w) + 128*sum(w).
    for (int i = NO_INDEX; i < outputSize; ++i) {
        const auto rowOffset = static_cast<std::size_t>(i) * static_cast<std::size_t>(inputSize);
        int32_t weightSum = 0;
        for (int j = NO_INDEX; j < inputSize; ++j) {
            weightSum += static_cast<int32_t>(weights[rowOffset + static_cast<std::size_t>(j)]);
        }
        dotprodBiasAdjust[static_cast<std::size_t>(i)] = 128 * weightSum;
    }
}

void LinearLayer::forward_arm_neon(const int8_t* input, int32_t* output) const {
    for (int i = NO_INDEX; i < outputSize; ++i) {
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));
        output[i] = dotProductScalar(input, w, inputSize) + biases[static_cast<std::size_t>(i)];
    }
}

void LinearLayer::forward_arm_dotprod(const int8_t* input, int32_t* output) const {
#if NNUEBITBOARD_ARM_NEON && NNUEBITBOARD_HAS_VDOTQ
    const uint8x16_t signFlip = vdupq_n_u8(0x80);
    for (int i = NO_INDEX; i < outputSize; ++i) {
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));

        int32x4_t sumVec = vdupq_n_s32(0);
        int j = NO_INDEX;
        for (; j + kNeonDotProductStride <= inputSize; j += kNeonDotProductStride) {
            const uint8x16_t inU8 =
                vld1q_u8(reinterpret_cast<const uint8_t*>(input + static_cast<std::ptrdiff_t>(j)));
            const int8x16_t inS8 = vreinterpretq_s8_u8(veorq_u8(inU8, signFlip));
            const int8x16_t wS8 = vld1q_s8(w + static_cast<std::ptrdiff_t>(j));
            sumVec = vdotq_s32(sumVec, inS8, wS8);
        }

        int32_t sum = vaddvq_s32(sumVec) + dotprodBiasAdjust[static_cast<std::size_t>(i)];
        for (; j < inputSize; ++j) {
            const auto inputVal =
                static_cast<int32_t>(static_cast<uint8_t>(input[static_cast<std::ptrdiff_t>(j)]));
            const auto weightVal = static_cast<int32_t>(
                w[static_cast<std::ptrdiff_t>(j)]); // NOLINT(bugprone-signed-char-misuse)
            sum += inputVal * weightVal;
        }
        output[i] = sum + biases[static_cast<std::size_t>(i)];
    }
#else
    forward_arm_neon(input, output);
#endif
}

void LinearLayer::forward_arm_i8mm(const int8_t* input, int32_t* output) const {
#if NNUEBITBOARD_ARM_NEON && NNUEBITBOARD_HAS_VUSDOTQ
    for (int i = NO_INDEX; i < outputSize; ++i) {
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));

        int32x4_t sumVec = vdupq_n_s32(0);
        int j = NO_INDEX;
        for (; j + kNeonDotProductStride <= inputSize; j += kNeonDotProductStride) {
            const uint8x16_t inU8 =
                vld1q_u8(reinterpret_cast<const uint8_t*>(input + static_cast<std::ptrdiff_t>(j)));
            const int8x16_t wS8 = vld1q_s8(w + static_cast<std::ptrdiff_t>(j));
            sumVec = vusdotq_s32(sumVec, inU8, wS8);
        }

        int32_t sum = vaddvq_s32(sumVec);
        for (; j < inputSize; ++j) {
            const auto inputVal =
                static_cast<int32_t>(static_cast<uint8_t>(input[static_cast<std::ptrdiff_t>(j)]));
            const auto weightVal = static_cast<int32_t>(w[static_cast<std::ptrdiff_t>(j)]);
            sum += inputVal * weightVal;
        }
        output[i] = sum + biases[static_cast<std::size_t>(i)];
    }
#else
    forward_arm_dotprod(input, output);
#endif
}

void LinearLayer::forward_avx2(const int8_t* input, int32_t* output) const {
#if NNUEBITBOARD_X86_SIMD
    for (int i = NO_INDEX; i < outputSize; ++i) {
        __m256i sum = _mm256_setzero_si256();
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));

        for (int j = NO_INDEX; j < inputSize; j += kAvx2DotProductStride) {
            __m256i in_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + j));
            __m256i w_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(w + j));
            sum = SIMDOps::dpbusd_epi32(sum, in_vec, w_vec);
        }

        output[i] = SIMDOps::hadd_epi32_avx2(sum) + biases[i];
    }
#else
    for (int i = NO_INDEX; i < outputSize; ++i) {
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));
        output[i] = dotProductScalar(input, w, inputSize) + biases[i];
    }
#endif
}

void LinearLayer::forward_avx512(const int8_t* input, int32_t* output) const {
#ifdef __AVX512F__
    for (int i = NO_INDEX; i < outputSize; ++i) {
        __m512i sum = _mm512_setzero_si512();
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));

        for (int j = NO_INDEX; j < inputSize; j += kAvx512DotProductStride) {
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
    for (int i = NO_INDEX; i < outputSize; ++i) {
        __m512i sum = _mm512_setzero_si512();
        const auto* w = weights.data() +
                        (static_cast<std::ptrdiff_t>(i) * static_cast<std::ptrdiff_t>(inputSize));

        for (int j = NO_INDEX; j < inputSize; j += kAvx512DotProductStride) {
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
    const auto* in = static_cast<const int8_t*>(input);
    auto* out = static_cast<int32_t*>(output);

    switch (simdType) {
        case ARM_I8MM:
            forward_arm_i8mm(in, out);
            break;
        case ARM_DOTPROD:
            forward_arm_dotprod(in, out);
            break;
        case ARM_NEON:
            forward_arm_neon(in, out);
            break;
        case SCALAR:
            forward_avx2(in, out);
            break;
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
#if NNUEBITBOARD_X86_SIMD
    const __m256i zero = _mm256_setzero_si256();
    const __m256i max_val = _mm256_set1_epi32(QA);

    for (int i = NO_INDEX; i < size; i += kAvx2ReluStride) {
        __m256i val = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(input + i));
        val = _mm256_max_epi32(val, zero);
        val = _mm256_min_epi32(val, max_val);
        val = _mm256_srai_epi32(val, SHIFT);
        __m128i val_lo = _mm256_castsi256_si128(val);
        __m128i val_hi = _mm256_extracti128_si256(val, ONE);
        __m128i packed = _mm_packs_epi32(val_lo, val_hi);
        packed = _mm_packs_epi16(packed, packed);
        _mm_storel_epi64(reinterpret_cast<__m128i*>(output + i), packed);
    }
#else
    applyClippedReluScalar(input, output, size);
#endif
}

void ClippedReLU::forward_avx512(const int32_t* input, int8_t* output) const {
#ifdef __AVX512F__
    const __m512i zero = _mm512_setzero_si512();
    const __m512i max_val = _mm512_set1_epi32(QA);

    for (int i = NO_INDEX; i < size; i += kAvx512ReluStride) {
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
    const auto* in = static_cast<const int32_t*>(input);
    auto* out = static_cast<int8_t*>(output);

    if (hasAVX512()) {
        forward_avx512(in, out);
    } else {
        forward_avx2(in, out);
    }
}

NNUEEvaluator::NNUEEvaluator() {
    fc1 = std::make_unique<LinearLayer>(COLOR_COUNT * L1_SIZE, L2_SIZE);
    ac1 = std::make_unique<ClippedReLU>(L2_SIZE);
    fc2 = std::make_unique<LinearLayer>(L2_SIZE, L3_SIZE);
    ac2 = std::make_unique<ClippedReLU>(L3_SIZE);
    fc3 = std::make_unique<LinearLayer>(L3_SIZE, L3_SIZE);
    fc4 = std::make_unique<LinearLayer>(L3_SIZE, OUTPUT_SIZE);
    featureWeights.resize(static_cast<size_t>(INPUT_DIMENSIONS) * static_cast<size_t>(L1_SIZE));
}

bool NNUEEvaluator::loadNetwork(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return false;
    }

    uint32_t magic = kCpuidBaseLeaf;
    uint32_t version = kCpuidBaseLeaf;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != kNetworkMagic || version != kNetworkVersion) {
        return false;
    }

    file.read(reinterpret_cast<char*>(featureWeights.data()),
              static_cast<std::streamsize>(featureWeights.size() * sizeof(int16_t)));

    accumulator.init(featureWeights.data());
    return true;
}

void NNUEEvaluator::transformInput(const BitboardPosition& pos, int8_t* output) const {
    int perspective =
        pos.getSideToMove() == ChessPieceColor::WHITE ? WHITE_PERSPECTIVE : BLACK_PERSPECTIVE;

    if (!accumulator.isComputed(perspective)) {
        accumulator.refresh(pos);
    }

    const auto* acc = accumulator.getAccumulation(perspective);

    for (int i = NO_INDEX; i < COLOR_COUNT * L1_SIZE; ++i) {
        int32_t val = acc[i];
        val = std::max(NO_INDEX, std::min(QA, val));
        output[i] = static_cast<int8_t>(val >> kActivationShiftBits);
    }
}

int NNUEEvaluator::evaluate(const BitboardPosition& pos) const {
    alignas(SIMD_ALIGN) int8_t input[COLOR_COUNT * L1_SIZE];
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

    for (int i = NO_INDEX; i < L3_SIZE; ++i) {
        hidden3_relu[i] = static_cast<int8_t>(
            std::max(NO_INDEX, std::min(kMaxOutputActivation, hidden3[i] >> kActivationShiftBits)));
    }

    fc4->forward(hidden3_relu, output);
    return output[NO_INDEX] * kScoreScale / QAB;
}

void NNUEEvaluator::updateBeforeMove(const BitboardPosition& pos, int from, int to,
                                     ChessPieceType piece, ChessPieceType captured) {

    int color =
        pos.getColorAt(from) == ChessPieceColor::WHITE ? WHITE_PERSPECTIVE : BLACK_PERSPECTIVE;
    int kingSquare = std::countr_zero(pos.getPieceBitboard(
        ChessPieceType::KING,
        color == WHITE_PERSPECTIVE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK));
    int kingBucket = FeatureTransformer::getKingBucket(kingSquare);

    int fromFeature = FeatureTransformer::makeIndex(
        from, static_cast<int>(piece) - PIECE_TYPE_OFFSET, color, kingBucket);
    accumulator.removeFeature(WHITE_PERSPECTIVE, fromFeature);
    accumulator.removeFeature(BLACK_PERSPECTIVE, fromFeature);

    if (captured != ChessPieceType::NONE) {
        int capturedColor = ONE - color;
        int capturedFeature = FeatureTransformer::makeIndex(
            to, static_cast<int>(captured) - PIECE_TYPE_OFFSET, capturedColor, kingBucket);
        accumulator.removeFeature(WHITE_PERSPECTIVE, capturedFeature);
        accumulator.removeFeature(BLACK_PERSPECTIVE, capturedFeature);
    }
}

void NNUEEvaluator::updateAfterMove(const BitboardPosition& pos, int from, int to,
                                    ChessPieceType piece, ChessPieceType promotion) {
    (void)from;

    int color =
        pos.getColorAt(to) == ChessPieceColor::WHITE ? WHITE_PERSPECTIVE : BLACK_PERSPECTIVE;
    int kingSquare = std::countr_zero(pos.getPieceBitboard(
        ChessPieceType::KING,
        color == WHITE_PERSPECTIVE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK));
    int kingBucket = FeatureTransformer::getKingBucket(kingSquare);
    ChessPieceType finalPiece = promotion != ChessPieceType::NONE ? promotion : piece;
    int toFeature = FeatureTransformer::makeIndex(
        to, static_cast<int>(finalPiece) - PIECE_TYPE_OFFSET, color, kingBucket);
    accumulator.addFeature(WHITE_PERSPECTIVE, toFeature);
    accumulator.addFeature(BLACK_PERSPECTIVE, toFeature);
}

bool init(const std::string& networkPath) {
    globalEvaluator = std::make_unique<NNUEEvaluator>();
    if (!globalEvaluator->loadNetwork(networkPath)) {
        globalEvaluator.reset();
        return false;
    }
    return true;
}

int evaluate(const BitboardPosition& pos) {
    if (!globalEvaluator) {
        return NO_INDEX;
    }
    return globalEvaluator->evaluate(pos);
}

int evaluate(const Board& board) {
    if (!globalEvaluator) {
        return NO_INDEX;
    }
    return globalEvaluator->evaluate(board);
}

} // namespace NNUEBitboard
