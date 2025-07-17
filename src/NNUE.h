#ifndef NNUE_H
#define NNUE_H

#include <array>
#include <vector>
#include <cstdint>
#include <memory>
#include <immintrin.h>
#include "core/ChessBoard.h"

namespace NNUE {

// NNUE architecture constants
constexpr int INPUT_DIMENSIONS = 768;  // 64 squares * 6 piece types * 2 colors
constexpr int L1_SIZE = 256;
constexpr int L2_SIZE = 32;
constexpr int L3_SIZE = 32;
constexpr int OUTPUT_SIZE = 1;

// Quantization scale for integer arithmetic
constexpr int SCALE = 64;

// Feature representation
struct FeatureIndex {
    static constexpr int index(int sq, ChessPieceType pt, ChessPieceColor c) {
        return sq + 64 * (static_cast<int>(pt) + 6 * static_cast<int>(c));
    }
};

// Accumulator for incremental updates
struct Accumulator {
    alignas(32) std::array<int16_t, L1_SIZE> white;
    alignas(32) std::array<int16_t, L1_SIZE> black;
    
    void reset();
    void refresh(const Board& board);
    void addFeature(int feature);
    void removeFeature(int feature);
};

// Network layers
class Layer {
public:
    virtual ~Layer() = default;
    virtual void forward(const void* input, void* output) const = 0;
};

class LinearLayer : public Layer {
private:
    const int inputSize;
    const int outputSize;
    alignas(32) std::vector<int16_t> weights;
    alignas(32) std::vector<int32_t> biases;
    
public:
    LinearLayer(int in, int out);
    void forward(const void* input, void* output) const override;
    void loadWeights(const int16_t* w, const int32_t* b);
};

class ClippedReLU : public Layer {
private:
    const int size;
    
public:
    explicit ClippedReLU(int s) : size(s) {}
    void forward(const void* input, void* output) const override;
};

// Main NNUE evaluator
class NNUEEvaluator {
private:
    std::unique_ptr<LinearLayer> inputLayer;
    std::unique_ptr<ClippedReLU> activation1;
    std::unique_ptr<LinearLayer> hidden1;
    std::unique_ptr<ClippedReLU> activation2;
    std::unique_ptr<LinearLayer> hidden2;
    std::unique_ptr<LinearLayer> outputLayer;
    
    // Accumulators for both perspectives
    mutable Accumulator accumulator[2];
    
    // Transform features based on side to move
    void transformFeatures(const Board& board, ChessPieceColor perspective, 
                          int16_t* output) const;
    
public:
    NNUEEvaluator();
    ~NNUEEvaluator();
    
    // Initialize network with weights
    bool loadNetwork(const std::string& filename);
    
    // Evaluate position from perspective of side to move
    int evaluate(const Board& board, ChessPieceColor sideToMove) const;
    
    // Update accumulator incrementally
    void updateAccumulator(const Board& board, int from, int to);
    
    // Reset accumulator for new position
    void resetAccumulator(const Board& board);
};

// Global NNUE instance
extern std::unique_ptr<NNUEEvaluator> globalEvaluator;

// Initialize NNUE system
bool init(const std::string& networkPath);

// Evaluate position using NNUE
int evaluate(const Board& board);

} // namespace NNUE

#endif // NNUE_H