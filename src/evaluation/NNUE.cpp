#include "NNUE.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace NNUE {

std::unique_ptr<NNUEEvaluator> globalEvaluator;

void Accumulator::reset() {
    std::fill(white.begin(), white.end(), 0);
    std::fill(black.begin(), black.end(), 0);
}

void Accumulator::refresh(const Board& board) {
    reset();

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int feature = FeatureIndex::index(sq, piece.PieceType, piece.PieceColor);
            addFeature(feature);
        }
    }
}

void Accumulator::addFeature(int feature) {}

void Accumulator::removeFeature(int feature) {}

LinearLayer::LinearLayer(int in, int out)
    : inputSize(in), outputSize(out), weights(in * out), biases(out) {}

void LinearLayer::forward(const void* input, void* output) const {
    const int16_t* in = static_cast<const int16_t*>(input);
    int32_t* out = static_cast<int32_t*>(output);

    for (int i = 0; i < outputSize; ++i) {
        __m256i sum = _mm256_setzero_si256();

        for (int j = 0; j < inputSize; j += 16) {
            __m256i w = _mm256_load_si256((__m256i*)&weights[i * inputSize + j]);
            __m256i x = _mm256_load_si256((__m256i*)&in[j]);
            __m256i prod = _mm256_madd_epi16(w, x);
            sum = _mm256_add_epi32(sum, prod);
        }

        __m128i sum_high = _mm256_extracti128_si256(sum, 1);
        __m128i sum_low = _mm256_castsi256_si128(sum);
        __m128i sum_128 = _mm_add_epi32(sum_high, sum_low);
        __m128i sum_64 = _mm_add_epi32(sum_128, _mm_srli_si128(sum_128, 8));
        __m128i sum_32 = _mm_add_epi32(sum_64, _mm_srli_si128(sum_64, 4));

        out[i] = _mm_cvtsi128_si32(sum_32) + biases[i];
    }
}

void LinearLayer::loadWeights(const int16_t* w, const int32_t* b) {
    std::copy(w, w + weights.size(), weights.begin());
    std::copy(b, b + biases.size(), biases.begin());
}

void ClippedReLU::forward(const void* input, void* output) const {
    const int32_t* in = static_cast<const int32_t*>(input);
    int16_t* out = static_cast<int16_t*>(output);

    for (int i = 0; i < size; ++i) {
        int32_t val = in[i];
        val = std::max(0, val);
        val = std::min(127 * SCALE, val);
        out[i] = static_cast<int16_t>(val / SCALE);
    }
}

NNUEEvaluator::NNUEEvaluator() {
    inputLayer = std::make_unique<LinearLayer>(INPUT_DIMENSIONS, L1_SIZE);
    activation1 = std::make_unique<ClippedReLU>(L1_SIZE);
    hidden1 = std::make_unique<LinearLayer>(2 * L1_SIZE, L2_SIZE);
    activation2 = std::make_unique<ClippedReLU>(L2_SIZE);
    hidden2 = std::make_unique<LinearLayer>(L2_SIZE, L3_SIZE);
    outputLayer = std::make_unique<LinearLayer>(L3_SIZE, OUTPUT_SIZE);
}

NNUEEvaluator::~NNUEEvaluator() = default;

bool NNUEEvaluator::loadNetwork(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        return false;

    int32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (magic != 0x4E4E5545 || version != 1) {
        return false;
    }

    return true;
}

void NNUEEvaluator::transformFeatures(const Board& board, ChessPieceColor perspective,
                                      int16_t* output) const {

    const Accumulator& acc = accumulator[static_cast<int>(perspective)];

    if (perspective == ChessPieceColor::WHITE) {
        std::copy(acc.white.begin(), acc.white.end(), output);
        std::copy(acc.black.begin(), acc.black.end(), output + L1_SIZE);
    } else {
        std::copy(acc.black.begin(), acc.black.end(), output);
        std::copy(acc.white.begin(), acc.white.end(), output + L1_SIZE);
    }
}

int NNUEEvaluator::evaluate(const Board& board, ChessPieceColor sideToMove) const {
    alignas(32) int16_t input[2 * L1_SIZE];
    alignas(32) int32_t hidden1_out[L2_SIZE];
    alignas(32) int16_t hidden1_relu[L2_SIZE];
    alignas(32) int32_t hidden2_out[L3_SIZE];
    alignas(32) int16_t hidden2_relu[L3_SIZE];
    alignas(32) int32_t output[OUTPUT_SIZE];

    transformFeatures(board, sideToMove, input);

    hidden1->forward(input, hidden1_out);
    activation1->forward(hidden1_out, hidden1_relu);
    hidden2->forward(hidden1_relu, hidden2_out);
    activation2->forward(hidden2_out, hidden2_relu);
    outputLayer->forward(hidden2_relu, output);

    return output[0] / SCALE;
}

void NNUEEvaluator::updateAccumulator(const Board& board, int from, int to) {

    const Piece& movingPiece = board.squares[to].piece;
    if (movingPiece.PieceType != ChessPieceType::NONE) {
        int feature = FeatureIndex::index(from, movingPiece.PieceType, movingPiece.PieceColor);
        accumulator[0].removeFeature(feature);
        accumulator[1].removeFeature(feature);

        feature = FeatureIndex::index(to, movingPiece.PieceType, movingPiece.PieceColor);
        accumulator[0].addFeature(feature);
        accumulator[1].addFeature(feature);
    }
}

void NNUEEvaluator::resetAccumulator(const Board& board) {
    accumulator[0].refresh(board);
    accumulator[1].refresh(board);
}

bool init(const std::string& networkPath) {
    globalEvaluator = std::make_unique<NNUEEvaluator>();
    return globalEvaluator->loadNetwork(networkPath);
}

int evaluate(const Board& board) {
    if (!globalEvaluator) {
        return 0;
    }

    ChessPieceColor stm = board.turn;
    return globalEvaluator->evaluate(board, stm);
}

} 