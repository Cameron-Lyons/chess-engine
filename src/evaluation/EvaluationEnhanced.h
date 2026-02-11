#pragma once

#include "../ai/NeuralNetwork.h"
#include "../core/ChessBoard.h"

#include <memory>

class EnhancedEvaluator {
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
            : useNeuralNetwork(true), nnWeight(0.7f), useTraditionalEval(true),
              traditionalWeight(0.3f), modelPath("models/chess_nn.bin"), materialWeight(1.0f),
              positionalWeight(0.8f), tacticalWeight(0.9f), endgameWeight(1.0f) {}
    };

    EnhancedEvaluator(const EvaluationConfig& config = EvaluationConfig{});
    ~EnhancedEvaluator() = default;

    int evaluatePosition(const Board& board);

    int evaluateMaterial(const Board& board);
    int evaluatePositional(const Board& board);
    int evaluateTactical(const Board& board);
    int evaluateEndgame(const Board& board);

    float getNeuralNetworkEvaluation(const Board& board);

    float getHybridEvaluation(const Board& board);

    void setConfig(const EvaluationConfig& config);
    const EvaluationConfig& getConfig() const;

    bool loadNeuralNetwork(const std::string& path);
    bool isNeuralNetworkLoaded() const;
    NeuralNetworkEvaluator* getNeuralNetwork() const;

private:
    EvaluationConfig config;
    std::unique_ptr<NeuralNetworkEvaluator> neuralNetwork;

    float getGamePhase(const Board& board);
    int interpolateEvaluation(int openingEval, int endgameEval, float phase);
};

extern std::unique_ptr<EnhancedEvaluator> g_enhancedEvaluator;

void initializeEnhancedEvaluator(
    const EnhancedEvaluator::EvaluationConfig& config = EnhancedEvaluator::EvaluationConfig{});

EnhancedEvaluator* getEnhancedEvaluator();

int evaluateEnhancedPosition(const Board& board);
int evaluateEnhancedMaterial(const Board& board);
int evaluateEnhancedPositional(const Board& board);
int evaluateEnhancedTactical(const Board& board);
int evaluateEnhancedEndgame(const Board& board);

float evaluateWithNeuralNetwork(const Board& board);
float evaluateHybrid(const Board& board, float nnWeight = 0.7f);
