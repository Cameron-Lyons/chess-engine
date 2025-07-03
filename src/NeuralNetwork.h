#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

#include "ChessBoard.h"
#include <vector>
#include <memory>
#include <string>

// Neural network evaluator for chess positions
class NeuralNetworkEvaluator {
public:
    struct NetworkConfig {
        int inputSize;
        int hiddenSize;
        int outputSize;
        float learningRate;
        std::string modelPath;
        
        NetworkConfig() : inputSize(768), hiddenSize(256), outputSize(1), 
                         learningRate(0.001f), modelPath("models/chess_nn.bin") {}
    };

    NeuralNetworkEvaluator(const NetworkConfig& config = NetworkConfig{});
    ~NeuralNetworkEvaluator();

    // Main evaluation function
    float evaluatePosition(const Board& board);
    
    // Position encoding for neural network input
    std::vector<float> encodePosition(const Board& board);
    
    // Training functions
    void train(const std::vector<std::pair<Board, float>>& trainingData);
    void saveModel(const std::string& path);
    void loadModel(const std::string& path);
    
    // Hybrid evaluation (combine NN with traditional eval)
    float hybridEvaluate(const Board& board, float nnWeight = 0.7f);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Helper functions
    std::vector<float> pieceToVector(const Piece& piece, int square);
    void updateWeights(const std::vector<float>& gradients);
};

// Position features for enhanced evaluation
struct PositionFeatures {
    // Material features
    int materialCount[2][6];  // [color][piece_type]
    float materialBalance;
    
    // Positional features
    int centerControl[2];
    int mobility[2];
    int kingSafety[2];
    int pawnStructure[2];
    
    // Tactical features
    int hangingPieces[2];
    int pinnedPieces[2];
    int discoveredAttacks[2];
    
    // Endgame features
    float gamePhase;  // 0.0 = opening, 1.0 = endgame
    int passedPawns[2];
    int kingDistance;
};

// Feature extraction for neural network
class FeatureExtractor {
public:
    static PositionFeatures extractFeatures(const Board& board);
    static std::vector<float> featuresToVector(const PositionFeatures& features);
    
private:
    static int calculateMobility(const Board& board, ChessPieceColor color);
    static int calculateKingSafety(const Board& board, ChessPieceColor color);
    static int calculatePawnStructure(const Board& board, ChessPieceColor color);
    static float calculateGamePhase(const Board& board);
};

#endif // NEURAL_NETWORK_H 