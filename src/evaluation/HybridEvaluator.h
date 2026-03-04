#pragma once

#include "../ai/NeuralNetwork.h"
#include "../core/ChessBoard.h"

#include <memory>

class HybridEvaluator {
public:
    struct EvaluationConfig {
        bool useNeuralNetwork;
        float nnWeight;
        bool useTraditionalEval;
        float traditionalWeight;
        std::string modelPath;
        float materialWeight;
        float positionalWeight;
        float tacticalWeight;
        float endgameWeight;

        EvaluationConfig()
            : useNeuralNetwork(true), nnWeight(0.7F), useTraditionalEval(true),
              traditionalWeight(0.3F), modelPath("models/chess_nn.bin"), materialWeight(1.0F),
              positionalWeight(0.8F), tacticalWeight(0.9F), endgameWeight(1.0F) {}
    };
    HybridEvaluator(const EvaluationConfig& config = EvaluationConfig{});
    ~HybridEvaluator() = default;
    int evaluatePosition(const Board& board);
    int evaluateMaterial(const Board& board) const;
    int evaluatePositional(const Board& board) const;
    int evaluateTactical(const Board& board) const;
    int evaluateEndgame(const Board& board);
    float getNeuralNetworkEvaluation(const Board& board);
    float getHybridEvaluation(const Board& board);
    bool loadNeuralNetwork(const std::string& path);
    NeuralNetworkEvaluator* getNeuralNetwork() const;

private:
    EvaluationConfig config;
    std::unique_ptr<NeuralNetworkEvaluator> neuralNetwork;
    float getGamePhase(const Board& board);
};

extern std::unique_ptr<HybridEvaluator> g_hybridEvaluator;

void initializeHybridEvaluator(
    const HybridEvaluator::EvaluationConfig& config = HybridEvaluator::EvaluationConfig{});

HybridEvaluator* getHybridEvaluator();

float evaluateWithNeuralNetwork(const Board& board);
float evaluateHybrid(const Board& board, float nnWeight = 0.7F);
