#ifndef NEURAL_NETWORK_H
#define NEURAL_NETWORK_H

#include "../core/ChessBoard.h"
#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

class NeuralNetworkEvaluator {
public:
    struct NetworkConfig {
        int inputSize{768};
        int hiddenSize{256};
        int outputSize{1};
        float learningRate{0.001f};
        std::string modelPath{"models/chess_nn.bin"};

        NetworkConfig() = default;
    };

    NeuralNetworkEvaluator(const NetworkConfig& config = NetworkConfig{});
    ~NeuralNetworkEvaluator();
    NeuralNetworkEvaluator(const NeuralNetworkEvaluator&) = delete;
    auto operator=(const NeuralNetworkEvaluator&) -> NeuralNetworkEvaluator& = delete;
    NeuralNetworkEvaluator(NeuralNetworkEvaluator&&) = delete;
    auto operator=(NeuralNetworkEvaluator&&) -> NeuralNetworkEvaluator& = delete;

    auto evaluatePosition(const Board& board) -> float;

    auto encodePosition(const Board& board) -> std::vector<float>;

    void train(const std::vector<std::pair<Board, float>>& trainingData);
    void saveModel(const std::string& path);
    void loadModel(const std::string& path);

    auto hybridEvaluate(const Board& board, float nnWeight = 0.7f) -> float;

private:
    class Impl;
    std::unique_ptr<Impl> m_pImpl;

    auto pieceToVector(const Piece& piece, int square) -> std::vector<float>;
    void updateWeights(const std::vector<float>& gradients);
};

class TrainingDataGenerator {
public:
    struct TrainingExample {
        Board position;
        float targetScore{};
        float gameResult{};
        int gameLength{};
    };

    TrainingDataGenerator();
    ~TrainingDataGenerator() = default;
    TrainingDataGenerator(const TrainingDataGenerator&) = delete;
    auto operator=(const TrainingDataGenerator&) -> TrainingDataGenerator& = delete;
    TrainingDataGenerator(TrainingDataGenerator&&) = delete;
    auto operator=(TrainingDataGenerator&&) -> TrainingDataGenerator& = delete;

    auto generateSelfPlayData(int numGames, int maxMoves = 200) -> std::vector<TrainingExample>;

    auto generateFromPositions(const std::vector<std::string>& fens,
                               const std::vector<float>& scores) -> std::vector<TrainingExample>;

    auto convertToNNFormat(const std::vector<TrainingExample>& examples)
        -> std::vector<std::pair<Board, float>>;

    void saveTrainingData(const std::vector<TrainingExample>& data, const std::string& path);
    auto loadTrainingData(const std::string& path) -> std::vector<TrainingExample>;

private:
    std::mt19937 m_rng;

    auto playGame(int maxMoves) -> std::vector<TrainingExample>;
    auto evaluateGameResult(const Board& board, int gameLength) -> float;

    auto getMaterialScore(const Board& board) -> float;
    auto getPositionalScore(const Board& board) -> float;
};

struct PositionFeatures {
    std::array<std::array<int, 6>, 2> materialCount{};
    float materialBalance{};

    std::array<int, 2> centerControl{};
    std::array<int, 2> mobility{};
    std::array<int, 2> kingSafety{};
    std::array<int, 2> pawnStructure{};

    std::array<int, 2> hangingPieces{};
    std::array<int, 2> pinnedPieces{};
    std::array<int, 2> discoveredAttacks{};

    float gamePhase{};
    std::array<int, 2> passedPawns{};
    int kingDistance{};
};

class FeatureExtractor {
public:
    static auto extractFeatures(const Board& board) -> PositionFeatures;
    static auto featuresToVector(const PositionFeatures& features) -> std::vector<float>;

private:
    static auto calculateMobility(const Board& board, ChessPieceColor color) -> int;
    static auto calculateKingSafety(const Board& board, ChessPieceColor color) -> int;
    static auto calculatePawnStructure(const Board& board, ChessPieceColor color) -> int;
    static auto calculateGamePhase(const Board& board) -> float;
};

class NNTrainer {
public:
    struct TrainingConfig {
        int batchSize{32};
        int epochs{10};
        float validationSplit{0.2f};
        float earlyStoppingPatience{5};
        std::string modelPath{"models/chess_nn.bin"};
        std::string trainingDataPath{"data/training_data.bin"};

        TrainingConfig() = default;
    };

    NNTrainer(NeuralNetworkEvaluator& nn, const TrainingConfig& config = TrainingConfig{});

    void trainOnSelfPlayData(int numGames);
    void trainOnFile(const std::string& dataPath);
    void validateModel(const std::vector<std::pair<Board, float>>& validationData);

    auto evaluateModel(const std::vector<std::pair<Board, float>>& testData) -> float;
    void generateTrainingReport(const std::string& outputPath);

private:
    NeuralNetworkEvaluator& m_neuralNetwork;
    TrainingDataGenerator m_dataGenerator;
    TrainingConfig m_config;

    std::vector<float> m_trainingLosses;
    std::vector<float> m_validationLosses;

    auto splitData(const std::vector<std::pair<Board, float>>& data, float splitRatio,
                   bool takeFirst) -> std::vector<std::pair<Board, float>>;
    auto calculateLoss(const std::vector<std::pair<Board, float>>& data) -> float;
};

#endif