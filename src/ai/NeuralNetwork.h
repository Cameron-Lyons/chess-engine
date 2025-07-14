#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

#include "../core/ChessBoard.h"
#include <vector>
#include <memory>
#include <string>
#include <random>

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

// Training data generator for self-play
class TrainingDataGenerator {
public:
    struct TrainingExample {
        Board position;
        float targetScore;
        float gameResult;  // 1.0 = white win, 0.5 = draw, 0.0 = black win
        int gameLength;
    };
    
    TrainingDataGenerator();
    ~TrainingDataGenerator() = default;
    
    // Generate training data from self-play games
    std::vector<TrainingExample> generateSelfPlayData(int numGames, int maxMoves = 200);
    
    // Generate training data from known positions
    std::vector<TrainingExample> generateFromPositions(const std::vector<std::string>& fens, 
                                                       const std::vector<float>& scores);
    
    // Convert training examples to neural network format
    std::vector<std::pair<Board, float>> convertToNNFormat(const std::vector<TrainingExample>& examples);
    
    // Save/load training data
    void saveTrainingData(const std::vector<TrainingExample>& data, const std::string& path);
    std::vector<TrainingExample> loadTrainingData(const std::string& path);

private:
    std::mt19937 rng;
    
    // Self-play game generation
    std::vector<TrainingExample> playGame(int maxMoves);
    float evaluateGameResult(const Board& board, int gameLength);
    
    // Position evaluation helpers
    float getMaterialScore(const Board& board);
    float getPositionalScore(const Board& board);
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

// Neural network training utilities
class NNTrainer {
public:
    struct TrainingConfig {
        int batchSize;
        int epochs;
        float validationSplit;
        float earlyStoppingPatience;
        std::string modelPath;
        std::string trainingDataPath;
        
        TrainingConfig() : batchSize(32), epochs(10), validationSplit(0.2f), 
                          earlyStoppingPatience(5), modelPath("models/chess_nn.bin"),
                          trainingDataPath("data/training_data.bin") {}
    };
    
    NNTrainer(NeuralNetworkEvaluator& nn, const TrainingConfig& config = TrainingConfig{});
    
    // Training functions
    void trainOnSelfPlayData(int numGames);
    void trainOnFile(const std::string& dataPath);
    void validateModel(const std::vector<std::pair<Board, float>>& validationData);
    
    // Model evaluation
    float evaluateModel(const std::vector<std::pair<Board, float>>& testData);
    void generateTrainingReport(const std::string& outputPath);

private:
    NeuralNetworkEvaluator& neuralNetwork;
    TrainingDataGenerator dataGenerator;
    TrainingConfig config;
    
    // Training history
    std::vector<float> trainingLosses;
    std::vector<float> validationLosses;
    
    // Helper functions
    std::vector<std::pair<Board, float>> splitData(const std::vector<std::pair<Board, float>>& data, 
                                                   float splitRatio, bool takeFirst);
    float calculateLoss(const std::vector<std::pair<Board, float>>& data);
};

#endif // NEURAL_NETWORK_H 