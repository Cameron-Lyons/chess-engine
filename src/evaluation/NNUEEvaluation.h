#pragma once

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"

#include <array>
#include <cstring>
#include <fstream>
#include <immintrin.h>
#include <memory>
#include <vector>

class NNUEEvaluation {
private:
    static constexpr int INPUT_DIMENSIONS = 768;
    static constexpr int L1_SIZE = 256;
    static constexpr int L2_SIZE = 32;
    static constexpr int L3_SIZE = 32;
    static constexpr int OUTPUT_SIZE = 1;

    static constexpr int KING_BUCKETS = 64;
    static constexpr int PIECE_TYPES = 6;
    static constexpr int COLORS = 2;
    static constexpr int SQUARES = 64;

    static constexpr int SCALE = 128;
    static constexpr int Q_SCALE = 64;
    static constexpr int kWhitePerspectiveIndex = 0;
    static constexpr int kBlackPerspectiveIndex = 1;
    static constexpr int kNoSquareFound = -1;
    static constexpr int kPieceTypeOffset = 1;
    static constexpr int kSquareMirrorMask = 56;
    static constexpr int kAvxFeatureChunk = 16;
    static constexpr int kAvxLaneCount = 8;
    static constexpr int kMinActivation = 0;
    static constexpr int kMaxActivation = 127;
    static constexpr int kPrimaryOutputIndex = 0;
    static constexpr int kOutputScaleFactor = 4;
    static constexpr int kRandomBucketSize = 256;
    static constexpr int kRandomCenter = 128;
    static constexpr int kInputWeightScale = 2;
    static constexpr int kMaterialPhaseMax = 24;
    static constexpr int kMaterialPhaseOffset = 5000;
    static constexpr int kMaterialPhaseDivisor = 500;
    static constexpr int kPawnValue = 100;
    static constexpr int kKnightValue = 320;
    static constexpr int kBishopValue = 330;
    static constexpr int kRookValue = 500;
    static constexpr int kQueenValue = 900;
    static constexpr int kKingValue = 20000;
    static constexpr int kZeroScore = 0;

    struct alignas(64) Accumulator {
        alignas(32) int16_t values[L1_SIZE];
        bool computed[COLORS];

        void reset() {
            std::memset(values, 0, sizeof(values));
            computed[kWhitePerspectiveIndex] = computed[kBlackPerspectiveIndex] = false;
        }

        void addFeature(int index, const int16_t* weights) {
#ifdef __AVX2__
            for (int i = 0; i < L1_SIZE; i += kAvxFeatureChunk) {
                __m256i acc = _mm256_load_si256((__m256i*)&values[i]);
                __m256i w = _mm256_load_si256((__m256i*)&weights[index * L1_SIZE + i]);
                acc = _mm256_add_epi16(acc, w);
                _mm256_store_si256((__m256i*)&values[i], acc);
            }
#else
            for (int i = 0; i < L1_SIZE; i++) {
                values[i] += weights[index * L1_SIZE + i];
            }
#endif
        }

        void removeFeature(int index, const int16_t* weights) {
#ifdef __AVX2__
            for (int i = 0; i < L1_SIZE; i += kAvxFeatureChunk) {
                __m256i acc = _mm256_load_si256((__m256i*)&values[i]);
                __m256i w = _mm256_load_si256((__m256i*)&weights[index * L1_SIZE + i]);
                acc = _mm256_sub_epi16(acc, w);
                _mm256_store_si256((__m256i*)&values[i], acc);
            }
#else
            for (int i = 0; i < L1_SIZE; i++) {
                values[i] -= weights[index * L1_SIZE + i];
            }
#endif
        }
    };

    struct Network {
        alignas(64) int16_t inputWeights[INPUT_DIMENSIONS * L1_SIZE];
        alignas(64) int16_t inputBias[L1_SIZE];

        alignas(64) int8_t l1Weights[L1_SIZE * L2_SIZE * COLORS];
        alignas(64) int32_t l1Bias[L2_SIZE];

        alignas(64) int8_t l2Weights[L2_SIZE * L3_SIZE];
        alignas(64) int32_t l2Bias[L3_SIZE];

        alignas(64) int8_t outputWeights[L3_SIZE * OUTPUT_SIZE];
        alignas(64) int32_t outputBias[OUTPUT_SIZE];

        bool loaded = false;

        void initialize() {
            for (int i = 0; i < INPUT_DIMENSIONS * L1_SIZE; i++) {
                inputWeights[i] = (rand() % kRandomBucketSize - kRandomCenter) * kInputWeightScale;
            }

            for (int i = 0; i < L1_SIZE; i++) {
                inputBias[i] = kZeroScore;
            }

            for (int i = 0; i < L1_SIZE * L2_SIZE * COLORS; i++) {
                l1Weights[i] = rand() % kRandomBucketSize - kRandomCenter;
            }

            for (int i = 0; i < L2_SIZE; i++) {
                l1Bias[i] = kZeroScore;
            }

            for (int i = 0; i < L2_SIZE * L3_SIZE; i++) {
                l2Weights[i] = rand() % kRandomBucketSize - kRandomCenter;
            }

            for (int i = 0; i < L3_SIZE; i++) {
                l2Bias[i] = kZeroScore;
            }

            for (int i = 0; i < L3_SIZE * OUTPUT_SIZE; i++) {
                outputWeights[i] = rand() % kRandomBucketSize - kRandomCenter;
            }

            outputBias[kPrimaryOutputIndex] = kZeroScore;
            loaded = true;
        }

        bool loadFromFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::binary);
            if (!file) {
                return false;
            }
            file.read(reinterpret_cast<char*>(inputWeights), sizeof(inputWeights));
            file.read(reinterpret_cast<char*>(inputBias), sizeof(inputBias));
            file.read(reinterpret_cast<char*>(l1Weights), sizeof(l1Weights));
            file.read(reinterpret_cast<char*>(l1Bias), sizeof(l1Bias));
            file.read(reinterpret_cast<char*>(l2Weights), sizeof(l2Weights));
            file.read(reinterpret_cast<char*>(l2Bias), sizeof(l2Bias));
            file.read(reinterpret_cast<char*>(outputWeights), sizeof(outputWeights));
            file.read(reinterpret_cast<char*>(outputBias), sizeof(outputBias));

            loaded = file.good();
            return loaded;
        }

        void saveToFile(const std::string& filename) {
            std::ofstream file(filename, std::ios::binary);
            if (!file) {
                return;
            }
            file.write(reinterpret_cast<const char*>(inputWeights), sizeof(inputWeights));
            file.write(reinterpret_cast<const char*>(inputBias), sizeof(inputBias));
            file.write(reinterpret_cast<const char*>(l1Weights), sizeof(l1Weights));
            file.write(reinterpret_cast<const char*>(l1Bias), sizeof(l1Bias));
            file.write(reinterpret_cast<const char*>(l2Weights), sizeof(l2Weights));
            file.write(reinterpret_cast<const char*>(l2Bias), sizeof(l2Bias));
            file.write(reinterpret_cast<const char*>(outputWeights), sizeof(outputWeights));
            file.write(reinterpret_cast<const char*>(outputBias), sizeof(outputBias));
        }
    };

    Network network;
    Accumulator accumulator[COLORS];
    std::vector<int> activeFeatures[COLORS];

    static int getFeatureIndex(ChessPieceType piece, ChessPieceColor color, int square,
                               int kingSquare) {
        int pieceType = static_cast<int>(piece) - kPieceTypeOffset;
        int colorIdx =
            (color == ChessPieceColor::WHITE) ? kWhitePerspectiveIndex : kBlackPerspectiveIndex;

        if (colorIdx == kBlackPerspectiveIndex) {
            square = square ^ kSquareMirrorMask;
            kingSquare = kingSquare ^ kSquareMirrorMask;
        }

        int index = kingSquare * (PIECE_TYPES * COLORS * SQUARES) + pieceType * (COLORS * SQUARES) +
                    (colorIdx * SQUARES) + square;

        return std::min(index, INPUT_DIMENSIONS - kPieceTypeOffset);
    }

    void refreshAccumulator(const Board& board, ChessPieceColor perspective) {
        int perspectiveIdx = (perspective == ChessPieceColor::WHITE) ? kWhitePerspectiveIndex
                                                                     : kBlackPerspectiveIndex;
        accumulator[perspectiveIdx].reset();
        activeFeatures[perspectiveIdx].clear();

        std::memcpy(accumulator[perspectiveIdx].values, network.inputBias,
                    sizeof(network.inputBias));

        int kingSquare = kNoSquareFound;
        for (int sq = 0; sq < SQUARES; sq++) {
            if (board.squares[sq].piece.PieceType == ChessPieceType::KING &&
                board.squares[sq].piece.PieceColor == perspective) {
                kingSquare = sq;
                break;
            }
        }

        if (kingSquare == kNoSquareFound)
            return;

        for (int sq = 0; sq < SQUARES; sq++) {
            const Piece& piece = board.squares[sq].piece;
            if (piece.PieceType == ChessPieceType::NONE)
                continue;

            int featureIdx = getFeatureIndex(piece.PieceType, piece.PieceColor, sq, kingSquare);
            activeFeatures[perspectiveIdx].push_back(featureIdx);
            accumulator[perspectiveIdx].addFeature(featureIdx, network.inputWeights);
        }

        accumulator[perspectiveIdx].computed[perspectiveIdx] = true;
    }

    int32_t forward(const Board& board) {
        if (!accumulator[kWhitePerspectiveIndex].computed[kWhitePerspectiveIndex]) {
            refreshAccumulator(board, ChessPieceColor::WHITE);
        }
        if (!accumulator[kBlackPerspectiveIndex].computed[kBlackPerspectiveIndex]) {
            refreshAccumulator(board, ChessPieceColor::BLACK);
        }

        alignas(32) int32_t l1Output[L2_SIZE];
        std::memcpy(l1Output, network.l1Bias, sizeof(network.l1Bias));

        int stm = (board.turn == ChessPieceColor::WHITE) ? kWhitePerspectiveIndex
                                                         : kBlackPerspectiveIndex;
        int nstm =
            (stm == kWhitePerspectiveIndex) ? kBlackPerspectiveIndex : kWhitePerspectiveIndex;

#ifdef __AVX2__
        for (int i = 0; i < L2_SIZE; i++) {
            __m256i sum = _mm256_setzero_si256();

            for (int j = 0; j < L1_SIZE; j += kAvxFeatureChunk) {
                __m256i acc_stm = _mm256_load_si256((__m256i*)&accumulator[stm].values[j]);
                __m256i acc_nstm = _mm256_load_si256((__m256i*)&accumulator[nstm].values[j]);

                acc_stm = _mm256_max_epi16(acc_stm, _mm256_setzero_si256());
                acc_nstm = _mm256_max_epi16(acc_nstm, _mm256_setzero_si256());

                __m128i w_stm =
                    _mm_load_si128((__m128i*)&network.l1Weights[i * L1_SIZE * COLORS + j]);
                __m128i w_nstm = _mm_load_si128(
                    (__m128i*)&network.l1Weights[i * L1_SIZE * COLORS + L1_SIZE + j]);

                __m256i w_stm_256 = _mm256_cvtepi8_epi16(w_stm);
                __m256i w_nstm_256 = _mm256_cvtepi8_epi16(w_nstm);

                __m256i prod_stm = _mm256_mullo_epi16(acc_stm, w_stm_256);
                __m256i prod_nstm = _mm256_mullo_epi16(acc_nstm, w_nstm_256);

                sum = _mm256_add_epi32(
                    sum, _mm256_cvtepi16_epi32(_mm256_extracti128_si256(prod_stm, 0)));
                sum = _mm256_add_epi32(
                    sum, _mm256_cvtepi16_epi32(_mm256_extracti128_si256(prod_nstm, 0)));
            }

            int32_t* sum_array = reinterpret_cast<int32_t*>(&sum);
            int32_t total = kZeroScore;
            for (int k = 0; k < kAvxLaneCount; k++) {
                total += sum_array[k];
            }
            l1Output[i] += total / Q_SCALE;
        }
#else
        for (int i = 0; i < L2_SIZE; i++) {
            int32_t sum = kZeroScore;

            for (int j = 0; j < L1_SIZE; j++) {
                int16_t val_stm =
                    std::max(static_cast<int16_t>(kMinActivation), accumulator[stm].values[j]);
                int16_t val_nstm =
                    std::max(static_cast<int16_t>(kMinActivation), accumulator[nstm].values[j]);

                sum += val_stm * network.l1Weights[i * L1_SIZE * COLORS + j];
                sum += val_nstm * network.l1Weights[i * L1_SIZE * COLORS + L1_SIZE + j];
            }

            l1Output[i] += sum / Q_SCALE;
        }
#endif

        for (int i = 0; i < L2_SIZE; i++) {
            l1Output[i] = std::max(kMinActivation, std::min(kMaxActivation, l1Output[i]));
        }

        alignas(32) int32_t l2Output[L3_SIZE];
        std::memcpy(l2Output, network.l2Bias, sizeof(network.l2Bias));

        for (int i = 0; i < L3_SIZE; i++) {
            int32_t sum = kZeroScore;
            for (int j = 0; j < L2_SIZE; j++) {
                sum += l1Output[j] * network.l2Weights[i * L2_SIZE + j];
            }
            l2Output[i] += sum / Q_SCALE;
            l2Output[i] = std::max(kMinActivation, std::min(kMaxActivation, l2Output[i]));
        }

        int32_t output = network.outputBias[kPrimaryOutputIndex];
        for (int i = 0; i < L3_SIZE; i++) {
            output += l2Output[i] * network.outputWeights[i];
        }

        return output / (Q_SCALE * kOutputScaleFactor);
    }

public:
    NNUEEvaluation() {
        network.initialize();
        reset();
    }

    bool loadNetwork(const std::string& filename) {
        return network.loadFromFile(filename);
    }

    void saveNetwork(const std::string& filename) {
        network.saveToFile(filename);
    }

    void reset() {
        accumulator[kWhitePerspectiveIndex].reset();
        accumulator[kBlackPerspectiveIndex].reset();
        activeFeatures[kWhitePerspectiveIndex].clear();
        activeFeatures[kBlackPerspectiveIndex].clear();
    }

    int evaluate(const Board& board) {
        if (!network.loaded) {
            return kZeroScore;
        }

        int32_t nnueScore = forward(board);

        int material = kZeroScore;
        for (int sq = 0; sq < SQUARES; sq++) {
            const Piece& piece = board.squares[sq].piece;
            if (piece.PieceType != ChessPieceType::NONE) {
                int value = getPieceValue(piece.PieceType);
                if (piece.PieceColor == board.turn) {
                    material += value;
                } else {
                    material -= value;
                }
            }
        }

        int phase =
            std::min(kMaterialPhaseMax, (material + kMaterialPhaseOffset) / kMaterialPhaseDivisor);
        int blend =
            (nnueScore * phase + material * (kMaterialPhaseMax - phase)) / kMaterialPhaseMax;

        return blend;
    }

    void updateAccumulator(const Board& board, const MoveContent& move) {
        for (int perspective = kWhitePerspectiveIndex; perspective < COLORS; perspective++) {
            ChessPieceColor color = (perspective == kWhitePerspectiveIndex)
                                        ? ChessPieceColor::WHITE
                                        : ChessPieceColor::BLACK;

            int kingSquare = kNoSquareFound;
            for (int sq = 0; sq < SQUARES; sq++) {
                if (board.squares[sq].piece.PieceType == ChessPieceType::KING &&
                    board.squares[sq].piece.PieceColor == color) {
                    kingSquare = sq;
                    break;
                }
            }

            if (kingSquare == kNoSquareFound)
                continue;

            if (move.capture != ChessPieceType::NONE) {
                int captureIdx =
                    getFeatureIndex(move.capture, oppositeColor(board.turn), move.dest, kingSquare);
                accumulator[perspective].removeFeature(captureIdx, network.inputWeights);
            }

            int fromIdx = getFeatureIndex(move.piece, board.turn, move.src, kingSquare);
            int toIdx = getFeatureIndex(move.piece, board.turn, move.dest, kingSquare);

            accumulator[perspective].removeFeature(fromIdx, network.inputWeights);
            accumulator[perspective].addFeature(toIdx, network.inputWeights);

            if (move.promotion != ChessPieceType::NONE) {
                accumulator[perspective].removeFeature(toIdx, network.inputWeights);
                int promIdx = getFeatureIndex(move.promotion, board.turn, move.dest, kingSquare);
                accumulator[perspective].addFeature(promIdx, network.inputWeights);
            }
        }
    }

    bool isInitialized() const {
        return network.loaded;
    }

private:
    static int getPieceValue(ChessPieceType piece) {
        switch (piece) {
            case ChessPieceType::PAWN:
                return kPawnValue;
            case ChessPieceType::KNIGHT:
                return kKnightValue;
            case ChessPieceType::BISHOP:
                return kBishopValue;
            case ChessPieceType::ROOK:
                return kRookValue;
            case ChessPieceType::QUEEN:
                return kQueenValue;
            case ChessPieceType::KING:
                return kKingValue;
            default:
                return kZeroScore;
        }
    }

    static ChessPieceColor oppositeColor(ChessPieceColor color) {
        return color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    }
};
