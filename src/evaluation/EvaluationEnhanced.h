#ifndef EVALUATION_ENHANCED_H
#define EVALUATION_ENHANCED_H

#include "../core/ChessBoard.h"
#include "../ai/NeuralNetwork.h"
#include <memory>

// Enhanced evaluation with neural network integration
class EnhancedEvaluator {
public:
    struct EvaluationConfig {
        bool useNeuralNetwork;
        float nnWeight;
        bool useTraditionalEval;
        float traditionalWeight;
        std::string modelPath;
        
        // Traditional evaluation weights
        float materialWeight;
        float positionalWeight;
        float tacticalWeight;
        float endgameWeight;
        
        EvaluationConfig() : useNeuralNetwork(true), nnWeight(0.7f), useTraditionalEval(true),
                            traditionalWeight(0.3f), modelPath("models/chess_nn.bin"),
                            materialWeight(1.0f), positionalWeight(0.8f), tacticalWeight(0.9f),
                            endgameWeight(1.0f) {}
    };
    
    EnhancedEvaluator(const EvaluationConfig& config = EvaluationConfig{});
    ~EnhancedEvaluator() = default;
    
    // Main evaluation function
    int evaluatePosition(const Board& board);
    
    // Individual evaluation components
    int evaluateMaterial(const Board& board);
    int evaluatePositional(const Board& board);
    int evaluateTactical(const Board& board);
    int evaluateEndgame(const Board& board);
    
    // Neural network evaluation
    float getNeuralNetworkEvaluation(const Board& board);
    
    // Hybrid evaluation
    float getHybridEvaluation(const Board& board);
    
    // Configuration
    void setConfig(const EvaluationConfig& config);
    const EvaluationConfig& getConfig() const;
    
    // Model management
    bool loadNeuralNetwork(const std::string& path);
    bool isNeuralNetworkLoaded() const;
    NeuralNetworkEvaluator* getNeuralNetwork() const;

private:
    EvaluationConfig config;
    std::unique_ptr<NeuralNetworkEvaluator> neuralNetwork;
    
    // Helper functions
    float getGamePhase(const Board& board);
    int interpolateEvaluation(int openingEval, int endgameEval, float phase);
};

// Global enhanced evaluator instance
extern std::unique_ptr<EnhancedEvaluator> g_enhancedEvaluator;

// Function to initialize the enhanced evaluator
void initializeEnhancedEvaluator(const EnhancedEvaluator::EvaluationConfig& config = EnhancedEvaluator::EvaluationConfig{});

// Function to get the global enhanced evaluator
EnhancedEvaluator* getEnhancedEvaluator();

// Enhanced evaluation functions that use the neural network
int evaluateEnhancedPosition(const Board& board);
int evaluateEnhancedMaterial(const Board& board);
int evaluateEnhancedPositional(const Board& board);
int evaluateEnhancedTactical(const Board& board);
int evaluateEnhancedEndgame(const Board& board);

// Neural network specific evaluation
float evaluateWithNeuralNetwork(const Board& board);
float evaluateHybrid(const Board& board, float nnWeight = 0.7f);

#endif // EVALUATION_ENHANCED_H 