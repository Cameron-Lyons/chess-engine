#pragma once

#include <vector>
#include <array>
#include <cstring>
#include <immintrin.h>
#include <memory>
#include <fstream>
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"

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
    
    struct alignas(64) Accumulator {
        alignas(32) int16_t values[L1_SIZE];
        bool computed[2];
        
        void reset() {
            std::memset(values, 0, sizeof(values));
            computed[0] = computed[1] = false;
        }
        
        void addFeature(int index, const int16_t* weights) {
            #ifdef __AVX2__
            for (int i = 0; i < L1_SIZE; i += 16) {
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
            for (int i = 0; i < L1_SIZE; i += 16) {
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
        
        alignas(64) int8_t l1Weights[L1_SIZE * L2_SIZE * 2];
        alignas(64) int32_t l1Bias[L2_SIZE];
        
        alignas(64) int8_t l2Weights[L2_SIZE * L3_SIZE];
        alignas(64) int32_t l2Bias[L3_SIZE];
        
        alignas(64) int8_t outputWeights[L3_SIZE * OUTPUT_SIZE];
        alignas(64) int32_t outputBias[OUTPUT_SIZE];
        
        bool loaded = false;
        
        void initialize() {
            for (int i = 0; i < INPUT_DIMENSIONS * L1_SIZE; i++) {
                inputWeights[i] = (rand() % 256 - 128) * 2;
            }
            
            for (int i = 0; i < L1_SIZE; i++) {
                inputBias[i] = 0;
            }
            
            for (int i = 0; i < L1_SIZE * L2_SIZE * 2; i++) {
                l1Weights[i] = rand() % 256 - 128;
            }
            
            for (int i = 0; i < L2_SIZE; i++) {
                l1Bias[i] = 0;
            }
            
            for (int i = 0; i < L2_SIZE * L3_SIZE; i++) {
                l2Weights[i] = rand() % 256 - 128;
            }
            
            for (int i = 0; i < L3_SIZE; i++) {
                l2Bias[i] = 0;
            }
            
            for (int i = 0; i < L3_SIZE * OUTPUT_SIZE; i++) {
                outputWeights[i] = rand() % 256 - 128;
            }
            
            outputBias[0] = 0;
            loaded = true;
        }
        
        bool loadFromFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::binary);
            if (!file) return false;
            
            file.read((char*)inputWeights, sizeof(inputWeights));
            file.read((char*)inputBias, sizeof(inputBias));
            file.read((char*)l1Weights, sizeof(l1Weights));
            file.read((char*)l1Bias, sizeof(l1Bias));
            file.read((char*)l2Weights, sizeof(l2Weights));
            file.read((char*)l2Bias, sizeof(l2Bias));
            file.read((char*)outputWeights, sizeof(outputWeights));
            file.read((char*)outputBias, sizeof(outputBias));
            
            loaded = file.good();
            return loaded;
        }
        
        void saveToFile(const std::string& filename) {
            std::ofstream file(filename, std::ios::binary);
            if (!file) return;
            
            file.write((char*)inputWeights, sizeof(inputWeights));
            file.write((char*)inputBias, sizeof(inputBias));
            file.write((char*)l1Weights, sizeof(l1Weights));
            file.write((char*)l1Bias, sizeof(l1Bias));
            file.write((char*)l2Weights, sizeof(l2Weights));
            file.write((char*)l2Bias, sizeof(l2Bias));
            file.write((char*)outputWeights, sizeof(outputWeights));
            file.write((char*)outputBias, sizeof(outputBias));
        }
    };
    
    Network network;
    Accumulator accumulator[2];
    std::vector<int> activeFeatures[2];
    
    static int getFeatureIndex(ChessPieceType piece, ChessPieceColor color, int square, int kingSquare) {
        int pieceType = static_cast<int>(piece) - 1;
        int colorIdx = (color == ChessPieceColor::WHITE) ? 0 : 1;
        
        if (colorIdx == 1) {
            square = square ^ 56;
            kingSquare = kingSquare ^ 56;
        }
        
        int index = kingSquare * (PIECE_TYPES * COLORS * SQUARES) +
                   pieceType * (COLORS * SQUARES) +
                   colorIdx * SQUARES +
                   square;
        
        return std::min(index, INPUT_DIMENSIONS - 1);
    }
    
    void refreshAccumulator(const Board& board, ChessPieceColor perspective) {
        int perspectiveIdx = (perspective == ChessPieceColor::WHITE) ? 0 : 1;
        accumulator[perspectiveIdx].reset();
        activeFeatures[perspectiveIdx].clear();
        
        std::memcpy(accumulator[perspectiveIdx].values, network.inputBias, sizeof(network.inputBias));
        
        int kingSquare = -1;
        for (int sq = 0; sq < 64; sq++) {
            if (board.squares[sq].piece.PieceType == ChessPieceType::KING &&
                board.squares[sq].piece.PieceColor == perspective) {
                kingSquare = sq;
                break;
            }
        }
        
        if (kingSquare == -1) return;
        
        for (int sq = 0; sq < 64; sq++) {
            const Piece& piece = board.squares[sq].piece;
            if (piece.PieceType == ChessPieceType::NONE) continue;
            
            int featureIdx = getFeatureIndex(piece.PieceType, piece.PieceColor, sq, kingSquare);
            activeFeatures[perspectiveIdx].push_back(featureIdx);
            accumulator[perspectiveIdx].addFeature(featureIdx, network.inputWeights);
        }
        
        accumulator[perspectiveIdx].computed[perspectiveIdx] = true;
    }
    
    int32_t forward(const Board& board) {
        if (!accumulator[0].computed[0]) {
            refreshAccumulator(board, ChessPieceColor::WHITE);
        }
        if (!accumulator[1].computed[1]) {
            refreshAccumulator(board, ChessPieceColor::BLACK);
        }
        
        alignas(32) int32_t l1Output[L2_SIZE];
        std::memcpy(l1Output, network.l1Bias, sizeof(network.l1Bias));
        
        int stm = (board.turn == ChessPieceColor::WHITE) ? 0 : 1;
        int nstm = 1 - stm;
        
        #ifdef __AVX2__
        for (int i = 0; i < L2_SIZE; i++) {
            __m256i sum = _mm256_setzero_si256();
            
            for (int j = 0; j < L1_SIZE; j += 16) {
                __m256i acc_stm = _mm256_load_si256((__m256i*)&accumulator[stm].values[j]);
                __m256i acc_nstm = _mm256_load_si256((__m256i*)&accumulator[nstm].values[j]);
                
                acc_stm = _mm256_max_epi16(acc_stm, _mm256_setzero_si256());
                acc_nstm = _mm256_max_epi16(acc_nstm, _mm256_setzero_si256());
                
                __m128i w_stm = _mm_load_si128((__m128i*)&network.l1Weights[i * L1_SIZE * 2 + j]);
                __m128i w_nstm = _mm_load_si128((__m128i*)&network.l1Weights[i * L1_SIZE * 2 + L1_SIZE + j]);
                
                __m256i w_stm_256 = _mm256_cvtepi8_epi16(w_stm);
                __m256i w_nstm_256 = _mm256_cvtepi8_epi16(w_nstm);
                
                __m256i prod_stm = _mm256_mullo_epi16(acc_stm, w_stm_256);
                __m256i prod_nstm = _mm256_mullo_epi16(acc_nstm, w_nstm_256);
                
                sum = _mm256_add_epi32(sum, _mm256_cvtepi16_epi32(_mm256_extracti128_si256(prod_stm, 0)));
                sum = _mm256_add_epi32(sum, _mm256_cvtepi16_epi32(_mm256_extracti128_si256(prod_nstm, 0)));
            }
            
            int32_t* sum_array = (int32_t*)&sum;
            int32_t total = 0;
            for (int k = 0; k < 8; k++) {
                total += sum_array[k];
            }
            l1Output[i] += total / Q_SCALE;
        }
        #else
        for (int i = 0; i < L2_SIZE; i++) {
            int32_t sum = 0;
            
            for (int j = 0; j < L1_SIZE; j++) {
                int16_t val_stm = std::max((int16_t)0, accumulator[stm].values[j]);
                int16_t val_nstm = std::max((int16_t)0, accumulator[nstm].values[j]);
                
                sum += val_stm * network.l1Weights[i * L1_SIZE * 2 + j];
                sum += val_nstm * network.l1Weights[i * L1_SIZE * 2 + L1_SIZE + j];
            }
            
            l1Output[i] += sum / Q_SCALE;
        }
        #endif
        
        for (int i = 0; i < L2_SIZE; i++) {
            l1Output[i] = std::max(0, std::min(127, l1Output[i]));
        }
        
        alignas(32) int32_t l2Output[L3_SIZE];
        std::memcpy(l2Output, network.l2Bias, sizeof(network.l2Bias));
        
        for (int i = 0; i < L3_SIZE; i++) {
            int32_t sum = 0;
            for (int j = 0; j < L2_SIZE; j++) {
                sum += l1Output[j] * network.l2Weights[i * L2_SIZE + j];
            }
            l2Output[i] += sum / Q_SCALE;
            l2Output[i] = std::max(0, std::min(127, l2Output[i]));
        }
        
        int32_t output = network.outputBias[0];
        for (int i = 0; i < L3_SIZE; i++) {
            output += l2Output[i] * network.outputWeights[i];
        }
        
        return output / (Q_SCALE * 4);
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
        accumulator[0].reset();
        accumulator[1].reset();
        activeFeatures[0].clear();
        activeFeatures[1].clear();
    }
    
    int evaluate(const Board& board) {
        if (!network.loaded) {
            return 0;
        }
        
        int32_t nnueScore = forward(board);
        
        int material = 0;
        for (int sq = 0; sq < 64; sq++) {
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
        
        int phase = std::min(24, (material + 5000) / 500);
        int blend = (nnueScore * phase + material * (24 - phase)) / 24;
        
        return blend;
    }
    
    void updateAccumulator(const Board& board, const MoveContent& move) {
        for (int perspective = 0; perspective < 2; perspective++) {
            ChessPieceColor color = (perspective == 0) ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
            
            int kingSquare = -1;
            for (int sq = 0; sq < 64; sq++) {
                if (board.squares[sq].piece.PieceType == ChessPieceType::KING &&
                    board.squares[sq].piece.PieceColor == color) {
                    kingSquare = sq;
                    break;
                }
            }
            
            if (kingSquare == -1) continue;
            
            if (move.capture != ChessPieceType::NONE) {
                int captureIdx = getFeatureIndex(move.capture, oppositeColor(board.turn), 
                                                move.dest, kingSquare);
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
            case ChessPieceType::PAWN: return 100;
            case ChessPieceType::KNIGHT: return 320;
            case ChessPieceType::BISHOP: return 330;
            case ChessPieceType::ROOK: return 500;
            case ChessPieceType::QUEEN: return 900;
            case ChessPieceType::KING: return 20000;
            default: return 0;
        }
    }
    
    static ChessPieceColor oppositeColor(ChessPieceColor color) {
        return color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    }
};