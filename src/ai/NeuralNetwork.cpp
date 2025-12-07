#include "NeuralNetwork.h"
#include "../evaluation/Evaluation.h"
#include "../search/ValidMoves.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>

class NeuralNetworkEvaluator::Impl {
public:
    struct Layer {
        std::vector<std::vector<float>> weights;
        std::vector<float> biases;
        std::vector<float> activations;
        std::vector<float> gradients;
    };

    NetworkConfig config;
    std::vector<Layer> layers;
    std::mt19937 rng;

    Impl(const NetworkConfig& config) : config(config), rng(42) {
        initializeNetwork();
    }

    ~Impl() = default;
    Impl(const Impl&) = delete;
    auto operator=(const Impl&) -> Impl& = delete;
    Impl(Impl&&) = delete;
    auto operator=(Impl&&) -> Impl& = delete;

    void initializeNetwork() {

        std::vector<int> layerSizes = {config.inputSize, config.hiddenSize, config.outputSize};

        for (size_t i = 0; i < layerSizes.size() - 1; ++i) {
            Layer layer;
            int inputSize = layerSizes[i];
            int outputSize = layerSizes[i + 1];

            float scale = std::sqrt(2.0f / static_cast<float>(inputSize + outputSize));
            std::normal_distribution<float> dist(0.0f, scale);

            layer.weights.resize(outputSize);
            for (int j = 0; j < outputSize; ++j) {
                layer.weights[j].resize(inputSize);
                for (int k = 0; k < inputSize; ++k) {
                    layer.weights[j][k] = dist(rng);
                }
            }

            layer.biases.resize(outputSize, 0.0f);
            layer.activations.resize(outputSize);
            layer.gradients.resize(outputSize);

            layers.push_back(std::move(layer));
        }
    }

    auto forwardPass(const std::vector<float>& input) -> float {
        std::vector<float> currentInput = input;

        for (size_t i = 0; i < layers.size(); ++i) {
            Layer& layer = layers[i];

            for (int j = 0; j < layer.weights.size(); ++j) {
                float sum = layer.biases[j];
                for (int k = 0; k < layer.weights[j].size(); ++k) {
                    sum += layer.weights[j][k] * currentInput[k];
                }

                if (i == layers.size() - 1) {

                    layer.activations[j] = std::tanh(sum);
                } else {

                    layer.activations[j] = std::max(0.0f, sum);
                }
            }

            currentInput = layer.activations;
        }

        return layers.back().activations[0];
    }

    void backwardPass(const std::vector<float>& input, float target, float predicted) {

        float lossGradient = predicted - target;

        std::vector<float> nextGradient = {lossGradient};

        for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
            Layer& layer = layers[i];
            std::vector<float> currentGradient(layer.weights.size());

            for (int j = 0; j < layer.weights.size(); ++j) {
                float activationGradient = nextGradient[j];

                if (i == layers.size() - 1) {

                    activationGradient *= (1.0f - (layer.activations[j] * layer.activations[j]));
                } else {

                    if (layer.activations[j] <= 0) {
                        activationGradient = 0.0f;
                    }
                }

                layer.gradients[j] = activationGradient;
                currentGradient[j] = activationGradient;
            }

            if (i > 0) {
                std::vector<float> prevInput = (i == 0) ? input : layers[i - 1].activations;
                nextGradient.resize(prevInput.size(), 0.0f);

                for (int j = 0; j < layer.weights.size(); ++j) {
                    for (int k = 0; k < layer.weights[j].size(); ++k) {
                        nextGradient[k] += layer.weights[j][k] * currentGradient[j];
                    }
                }
            }
        }
    }

    void updateWeights(const std::vector<float>& input, float learningRate) {
        std::vector<float> currentInput = input;

        for (size_t i = 0; i < layers.size(); ++i) {
            Layer& layer = layers[i];

            for (int j = 0; j < layer.weights.size(); ++j) {
                for (int k = 0; k < layer.weights[j].size(); ++k) {
                    layer.weights[j][k] -= learningRate * layer.gradients[j] * currentInput[k];
                }
                layer.biases[j] -= learningRate * layer.gradients[j];
            }

            if (i < layers.size() - 1) {
                currentInput = layer.activations;
            }
        }
    }

    void saveModel(const std::string& path) {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for writing: " << path << '\n';
            return;
        }

        file.write(reinterpret_cast<const char*>(&config.inputSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.hiddenSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.outputSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.learningRate), sizeof(float));

        for (const auto& layer : layers) {

            for (const auto& weightRow : layer.weights) {
                file.write(reinterpret_cast<const char*>(weightRow.data()),
                           static_cast<std::streamsize>(weightRow.size() * sizeof(float)));
            }

            file.write(reinterpret_cast<const char*>(layer.biases.data()),
                       static_cast<std::streamsize>(layer.biases.size() * sizeof(float)));
        }

        file.close();
        std::cout << "Model saved to: " << path << '\n';
    }

    void loadModel(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for reading: " << path << '\n';
            return;
        }

        file.read(reinterpret_cast<char*>(&config.inputSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.hiddenSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.outputSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.learningRate), sizeof(float));

        layers.clear();

        std::vector<int> layerSizes = {config.inputSize, config.hiddenSize, config.outputSize};

        for (size_t i = 0; i < layerSizes.size() - 1; ++i) {
            Layer layer;
            int inputSize = layerSizes[i];
            int outputSize = layerSizes[i + 1];

            layer.weights.resize(outputSize);
            for (int j = 0; j < outputSize; ++j) {
                layer.weights[j].resize(inputSize);
                file.read(reinterpret_cast<char*>(layer.weights[j].data()),
                          static_cast<std::streamsize>(inputSize * sizeof(float)));
            }

            layer.biases.resize(outputSize);
            file.read(reinterpret_cast<char*>(layer.biases.data()),
                      static_cast<std::streamsize>(outputSize * sizeof(float)));

            layer.activations.resize(outputSize);
            layer.gradients.resize(outputSize);

            layers.push_back(std::move(layer));
        }

        file.close();
        std::cout << "Model loaded from: " << path << '\n';
    }
};

NeuralNetworkEvaluator::NeuralNetworkEvaluator(const NetworkConfig& config)
    : m_pImpl(std::make_unique<Impl>(config)) {}

NeuralNetworkEvaluator::~NeuralNetworkEvaluator() = default;

auto NeuralNetworkEvaluator::evaluatePosition(const Board& board) -> float {
    std::vector<float> input = encodePosition(board);
    float evaluation = m_pImpl->forwardPass(input);

    return evaluation * 1000.0f;
}

auto NeuralNetworkEvaluator::encodePosition(const Board& board) -> std::vector<float> {
    std::vector<float> encoding(768, 0.0f);

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int pieceIndex = static_cast<int>(piece.PieceType) - 1;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                pieceIndex += 6;
            }
            encoding[(square * 12) + pieceIndex] = 1.0f;
        }
    }

    return encoding;
}

void NeuralNetworkEvaluator::train(const std::vector<std::pair<Board, float>>& trainingData) {
    if (trainingData.empty()) {
        std::cerr << "Warning: No training data provided" << '\n';
        return;
    }

    std::cout << "Training neural network with " << trainingData.size() << " positions..." << '\n';

    float totalLoss = 0.0f;
    int batchSize = std::min(32, static_cast<int>(trainingData.size()));

    for (size_t i = 0; i < trainingData.size(); i += batchSize) {
        float batchLoss = 0.0f;

        for (int j = 0; j < batchSize && i + j < trainingData.size(); ++j) {
            const auto& [board, target] = trainingData[i + j];

            std::vector<float> input = encodePosition(board);

            float predicted = m_pImpl->forwardPass(input);

            float normalizedTarget = std::tanh(target / 1000.0f);

            m_pImpl->backwardPass(input, normalizedTarget, predicted);

            m_pImpl->updateWeights(input, m_pImpl->config.learningRate);

            batchLoss += (predicted - normalizedTarget) * (predicted - normalizedTarget);
        }

        totalLoss += batchLoss / static_cast<float>(batchSize);

        if ((i / batchSize) % 100 == 0) {
            std::cout << "Batch " << (i / batchSize)
                      << ", Loss: " << (batchLoss / static_cast<float>(batchSize)) << '\n';
        }
    }

    std::cout << "Training completed. Average loss: "
              << (totalLoss /
                  static_cast<float>(trainingData.size() / static_cast<size_t>(batchSize)))
              << '\n';
}

void NeuralNetworkEvaluator::saveModel(const std::string& path) {
    m_pImpl->saveModel(path);
}

void NeuralNetworkEvaluator::loadModel(const std::string& path) {
    m_pImpl->loadModel(path);
}

auto NeuralNetworkEvaluator::hybridEvaluate(const Board& board, float nnWeight) -> float {

    float nnEval = evaluatePosition(board);

    float traditionalEval = 0.0f;
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            float value = static_cast<float>(piece.PieceValue);
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                value = -value;
            }
            traditionalEval += value;
        }
    }

    return (nnWeight * nnEval) + ((1.0f - nnWeight) * traditionalEval);
}

auto NeuralNetworkEvaluator::pieceToVector(const Piece& piece, int square) -> std::vector<float> {
    (void)square;
    std::vector<float> vec(12, 0.0f);
    if (piece.PieceType != ChessPieceType::NONE) {
        int index = static_cast<int>(piece.PieceType) - 1;
        if (piece.PieceColor == ChessPieceColor::BLACK) {
            index += 6;
        }
        vec[index] = 1.0f;
    }
    return vec;
}

void NeuralNetworkEvaluator::updateWeights(const std::vector<float>& gradients) {
    (void)gradients;
}

auto FeatureExtractor::extractFeatures(const Board& board) -> PositionFeatures {
    PositionFeatures features = {};

    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            features.materialCount[color][piece] = 0;
        }
    }

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int color = (piece.PieceColor == ChessPieceColor::WHITE) ? 0 : 1;
            int pieceType = static_cast<int>(piece.PieceType) - 1;
            if (pieceType >= 0 && pieceType < 6) {
                features.materialCount[color][pieceType]++;
            }
        }
    }

    int whiteMaterial = 0, blackMaterial = 0;
    for (int piece = 0; piece < 6; ++piece) {
        whiteMaterial += features.materialCount[0][piece] *
                         Piece::getPieceValue(static_cast<ChessPieceType>(piece + 1));
        blackMaterial += features.materialCount[1][piece] *
                         Piece::getPieceValue(static_cast<ChessPieceType>(piece + 1));
    }
    features.materialBalance = static_cast<float>(whiteMaterial - blackMaterial);

    features.centerControl[0] = evaluateCenterControl(board);
    features.centerControl[1] = evaluateCenterControl(board);
    features.mobility[0] = calculateMobility(board, ChessPieceColor::WHITE);
    features.mobility[1] = calculateMobility(board, ChessPieceColor::BLACK);
    features.kingSafety[0] = calculateKingSafety(board, ChessPieceColor::WHITE);
    features.kingSafety[1] = calculateKingSafety(board, ChessPieceColor::BLACK);
    features.pawnStructure[0] = calculatePawnStructure(board, ChessPieceColor::WHITE);
    features.pawnStructure[1] = calculatePawnStructure(board, ChessPieceColor::BLACK);

    features.hangingPieces[0] = evaluateHangingPieces(board);
    features.hangingPieces[1] = evaluateHangingPieces(board);
    features.pinnedPieces[0] = 0;
    features.pinnedPieces[1] = 0;
    features.discoveredAttacks[0] = 0;
    features.discoveredAttacks[1] = 0;

    features.gamePhase = calculateGamePhase(board);
    features.passedPawns[0] = evaluatePassedPawns(board);
    features.passedPawns[1] = evaluatePassedPawns(board);
    features.kingDistance = 0;

    return features;
}

auto FeatureExtractor::featuresToVector(const PositionFeatures& features) -> std::vector<float> {
    std::vector<float> vec;

    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            vec.push_back(static_cast<float>(features.materialCount[color][piece]));
        }
    }

    vec.push_back(features.materialBalance);
    vec.push_back(static_cast<float>(features.centerControl[0]));
    vec.push_back(static_cast<float>(features.centerControl[1]));
    vec.push_back(static_cast<float>(features.mobility[0]));
    vec.push_back(static_cast<float>(features.mobility[1]));
    vec.push_back(static_cast<float>(features.kingSafety[0]));
    vec.push_back(static_cast<float>(features.kingSafety[1]));
    vec.push_back(static_cast<float>(features.pawnStructure[0]));
    vec.push_back(static_cast<float>(features.pawnStructure[1]));
    vec.push_back(static_cast<float>(features.hangingPieces[0]));
    vec.push_back(static_cast<float>(features.hangingPieces[1]));
    vec.push_back(static_cast<float>(features.pinnedPieces[0]));
    vec.push_back(static_cast<float>(features.pinnedPieces[1]));
    vec.push_back(static_cast<float>(features.discoveredAttacks[0]));
    vec.push_back(static_cast<float>(features.discoveredAttacks[1]));
    vec.push_back(features.gamePhase);
    vec.push_back(static_cast<float>(features.passedPawns[0]));
    vec.push_back(static_cast<float>(features.passedPawns[1]));
    vec.push_back(static_cast<float>(features.kingDistance));

    return vec;
}

auto FeatureExtractor::calculateMobility(const Board& board, ChessPieceColor color) -> int {

    int mobility = 0;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceColor == color) {

            mobility += piece.PieceValue / 100;
        }
    }

    return mobility;
}

auto FeatureExtractor::calculateKingSafety(const Board& board, ChessPieceColor color) -> int {

    int kingSquare = -1;
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::KING && piece.PieceColor == color) {
            kingSquare = square;
            break;
        }
    }

    if (kingSquare == -1)
        return 0;

    int row = kingSquare / 8;
    int col = kingSquare % 8;

    if (col >= 2 && col <= 5 && row >= 1 && row <= 5) {
        return -100;
    }

    return 0;
}

auto FeatureExtractor::calculatePawnStructure(const Board& board, ChessPieceColor color) -> int {
    int score = 0;

    std::vector<int> pawnsInFile(8, 0);

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
            int file = square % 8;
            pawnsInFile[file]++;
        }
    }

    for (int file = 0; file < 8; ++file) {
        if (pawnsInFile[file] > 1) {
            score -= 50 * (pawnsInFile[file] - 1);
        }
    }

    return score;
}

auto FeatureExtractor::calculateGamePhase(const Board& board) -> float {
    int totalPieces = 0;
    int pawns = 0;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            totalPieces++;
            if (piece.PieceType == ChessPieceType::PAWN) {
                pawns++;
            }
        }
    }

    float phase = 1.0f - (static_cast<float>(totalPieces) / 32.0f);
    phase = std::max(0.0f, std::min(1.0f, phase));

    return phase;
}

TrainingDataGenerator::TrainingDataGenerator() : m_rng(42) {}

auto TrainingDataGenerator::generateSelfPlayData(int numGames, int maxMoves)
    -> std::vector<TrainingDataGenerator::TrainingExample> {
    std::vector<TrainingExample> allExamples;

    std::cout << "Generating " << numGames << " self-play games..." << '\n';

    for (int game = 0; game < numGames; ++game) {
        if (game % 10 == 0) {
            std::cout << "Playing game " << game + 1 << "/" << numGames << '\n';
        }

        auto gameExamples = playGame(maxMoves);
        allExamples.insert(allExamples.end(), gameExamples.begin(), gameExamples.end());
    }

    std::cout << "Generated " << allExamples.size() << " training examples" << '\n';
    return allExamples;
}

auto TrainingDataGenerator::playGame(int maxMoves)
    -> std::vector<TrainingDataGenerator::TrainingExample> {
    std::vector<TrainingExample> examples;

    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int moveCount = 0;
    std::vector<Board> gameHistory;

    while (moveCount < maxMoves) {

        TrainingExample example;
        example.position = board;
        example.gameLength = moveCount;

        std::vector<std::pair<int, int>> legalMoves;
        for (int src = 0; src < 64; ++src) {
            for (int dest = 0; dest < 64; ++dest) {
                if (IsMoveLegal(board, src, dest)) {
                    legalMoves.emplace_back(src, dest);
                }
            }
        }

        if (legalMoves.empty()) {

            example.targetScore = evaluateGameResult(board, moveCount);
            example.gameResult = example.targetScore;
            examples.push_back(example);
            break;
        }

        std::pair<int, int> bestMove = legalMoves[0];
        float bestScore = -10000.0f;

        for (const auto& move : legalMoves) {
            float score = 0.0f;

            if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
                score += static_cast<float>(board.squares[move.second].piece.PieceValue);
            }

            int destRow = move.second / 8;
            int destCol = move.second % 8;
            if (destRow >= 2 && destRow <= 5 && destCol >= 2 && destCol <= 5) {
                score += 50;
            }

            std::uniform_real_distribution<float> noise(-100.0f, 100.0f);
            score += noise(m_rng);

            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }

        board.movePiece(bestMove.first, bestMove.second);
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;
        board.updateBitboards();

        example.targetScore = getMaterialScore(board) + getPositionalScore(board);
        example.gameResult = 0.5f;
        examples.push_back(example);

        gameHistory.push_back(board);
        moveCount++;
    }

    float finalResult = evaluateGameResult(board, moveCount);
    for (auto& example : examples) {
        example.gameResult = finalResult;
    }

    return examples;
}

auto TrainingDataGenerator::evaluateGameResult(const Board& board, int gameLength) -> float {

    bool whiteInCheck = false, blackInCheck = false;
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whiteInCheck = IsKingInCheck(board, ChessPieceColor::WHITE);
            } else {
                blackInCheck = IsKingInCheck(board, ChessPieceColor::BLACK);
            }
        }
    }

    if (whiteInCheck && board.turn == ChessPieceColor::WHITE) {
        return 0.0f;
    } else if (blackInCheck && board.turn == ChessPieceColor::BLACK) {
        return 1.0f;
    }

    if (gameLength > 150) {
        return 0.5f;
    }

    float materialScore = getMaterialScore(board);
    float positionalScore = getPositionalScore(board);
    float totalScore = materialScore + positionalScore;

    return 1.0f / (1.0f + std::exp(-totalScore / 1000.0f));
}

auto TrainingDataGenerator::getMaterialScore(const Board& board) -> float {
    float score = 0.0f;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            float value = static_cast<float>(piece.PieceValue);
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                value = -value;
            }
            score += value;
        }
    }

    return score;
}

auto TrainingDataGenerator::getPositionalScore(const Board& board) -> float {
    float score = 0.0f;

    constexpr std::array<int, 4> centerSquares = {27, 28, 35, 36};
    for (int square : centerSquares) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            float bonus = static_cast<float>(piece.PieceValue) * 0.1f;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                bonus = -bonus;
            }
            score += bonus;
        }
    }

    return score;
}

auto TrainingDataGenerator::convertToNNFormat(const std::vector<TrainingExample>& examples)
    -> std::vector<std::pair<Board, float>> {
    std::vector<std::pair<Board, float>> nnData;

    for (const auto& example : examples) {

        float score = (example.gameResult - 0.5f) * 2000.0f;
        nnData.emplace_back(example.position, score);
    }

    return nnData;
}

void TrainingDataGenerator::saveTrainingData(const std::vector<TrainingExample>& data,
                                             const std::string& path) {

    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path << '\n';
        return;
    }

    size_t numExamples = data.size();
    file.write(reinterpret_cast<const char*>(&numExamples), sizeof(size_t));

    for (const auto& example : data) {

        std::string fen = example.position.toFEN();
        size_t fenLength = fen.length();
        file.write(reinterpret_cast<const char*>(&fenLength), sizeof(size_t));
        file.write(fen.c_str(), static_cast<std::streamsize>(fenLength));

        file.write(reinterpret_cast<const char*>(&example.targetScore), sizeof(float));
        file.write(reinterpret_cast<const char*>(&example.gameResult), sizeof(float));
        file.write(reinterpret_cast<const char*>(&example.gameLength), sizeof(int));
    }

    file.close();
    std::cout << "Training data saved to: " << path << '\n';
}

auto TrainingDataGenerator::loadTrainingData(const std::string& path)
    -> std::vector<TrainingDataGenerator::TrainingExample> {
    std::vector<TrainingExample> data;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << path << '\n';
        return data;
    }

    size_t numExamples = 0;
    file.read(reinterpret_cast<char*>(&numExamples), sizeof(size_t));

    data.reserve(numExamples);

    for (size_t i = 0; i < numExamples; ++i) {
        TrainingExample example;

        size_t fenLength = 0;
        file.read(reinterpret_cast<char*>(&fenLength), sizeof(size_t));

        std::string fen(fenLength, '\0');
        file.read(fen.data(), static_cast<std::streamsize>(fenLength));

        example.position.InitializeFromFEN(fen);

        file.read(reinterpret_cast<char*>(&example.targetScore), sizeof(float));
        file.read(reinterpret_cast<char*>(&example.gameResult), sizeof(float));
        file.read(reinterpret_cast<char*>(&example.gameLength), sizeof(int));

        data.push_back(example);
    }

    file.close();
    std::cout << "Training data loaded from: " << path << " (" << data.size() << " examples)"
              << '\n';

    return data;
}

NNTrainer::NNTrainer(NeuralNetworkEvaluator& nn, const TrainingConfig& config)
    : m_neuralNetwork(nn), m_config(config) {}

void NNTrainer::trainOnSelfPlayData(int numGames) {
    std::cout << "Starting self-play training with " << numGames << " games..." << '\n';

    auto trainingExamples = m_dataGenerator.generateSelfPlayData(numGames);
    auto nnData = m_dataGenerator.convertToNNFormat(trainingExamples);

    auto trainingData = splitData(nnData, 1.0f - m_config.validationSplit, true);
    auto validationData = splitData(nnData, m_config.validationSplit, false);

    std::cout << "Training set: " << trainingData.size() << " examples" << '\n';
    std::cout << "Validation set: " << validationData.size() << " examples" << '\n';

    float bestValidationLoss = std::numeric_limits<float>::max();
    int patienceCounter = 0;

    for (int epoch = 0; epoch < m_config.epochs; ++epoch) {
        std::cout << "Epoch " << epoch + 1 << "/" << m_config.epochs << '\n';

        m_neuralNetwork.train(trainingData);

        float trainLoss = calculateLoss(trainingData);
        float valLoss = calculateLoss(validationData);

        m_trainingLosses.push_back(trainLoss);
        m_validationLosses.push_back(valLoss);

        std::cout << "Training loss: " << trainLoss << ", Validation loss: " << valLoss << '\n';

        if (valLoss < bestValidationLoss) {
            bestValidationLoss = valLoss;
            patienceCounter = 0;

            m_neuralNetwork.saveModel(m_config.modelPath);
        } else {
            patienceCounter++;
            if (patienceCounter >= static_cast<int>(m_config.earlyStoppingPatience)) {
                std::cout << "Early stopping triggered after " << epoch + 1 << " epochs" << '\n';
                break;
            }
        }
    }

    std::cout << "Training completed. Best validation loss: " << bestValidationLoss << '\n';
}

void NNTrainer::trainOnFile(const std::string& dataPath) {
    std::cout << "Training on data from: " << dataPath << '\n';

    auto trainingExamples = m_dataGenerator.loadTrainingData(dataPath);
    auto nnData = m_dataGenerator.convertToNNFormat(trainingExamples);

    auto trainingData = splitData(nnData, 1.0f - m_config.validationSplit, true);
    auto validationData = splitData(nnData, m_config.validationSplit, false);

    std::cout << "Training set: " << trainingData.size() << " examples" << '\n';
    std::cout << "Validation set: " << validationData.size() << " examples" << '\n';

    m_neuralNetwork.train(trainingData);

    validateModel(validationData);
}

void NNTrainer::validateModel(const std::vector<std::pair<Board, float>>& validationData) {
    float loss = calculateLoss(validationData);
    std::cout << "Validation loss: " << loss << '\n';
}

auto NNTrainer::evaluateModel(const std::vector<std::pair<Board, float>>& testData) -> float {
    float totalLoss = 0.0f;
    int correctPredictions = 0;

    for (const auto& [board, target] : testData) {
        float predicted = m_neuralNetwork.evaluatePosition(board);
        float error = predicted - target;
        totalLoss += error * error;

        if ((predicted > 0 && target > 0) || (predicted < 0 && target < 0)) {
            correctPredictions++;
        }
    }

    float avgLoss = totalLoss / static_cast<float>(testData.size());
    float accuracy = static_cast<float>(correctPredictions) / static_cast<float>(testData.size());

    std::cout << "Test loss: " << avgLoss << '\n';
    std::cout << "Prediction accuracy: " << (accuracy * 100.0f) << "%" << '\n';

    return avgLoss;
}

void NNTrainer::generateTrainingReport(const std::string& outputPath) {
    std::ofstream report(outputPath);
    if (!report.is_open()) {
        std::cerr << "Error: Could not open report file: " << outputPath << '\n';
        return;
    }

    report << "Neural Network Training Report\n";
    report << "==============================\n\n";

    report << "Training Configuration:\n";
    report << "- Batch size: " << m_config.batchSize << "\n";
    report << "- Epochs: " << m_config.epochs << "\n";
    report << "- Validation split: " << (m_config.validationSplit * 100.0f) << "%\n";
    report << "- Early stopping patience: " << m_config.earlyStoppingPatience << "\n\n";

    report << "Training History:\n";
    report << "Epoch\tTraining Loss\tValidation Loss\n";
    for (size_t i = 0; i < m_trainingLosses.size(); ++i) {
        report << (i + 1) << "\t" << m_trainingLosses[i] << "\t" << m_validationLosses[i] << "\n";
    }

    if (!m_trainingLosses.empty()) {
        report << "\nFinal Results:\n";
        report << "- Final training loss: " << m_trainingLosses.back() << "\n";
        report << "- Final validation loss: " << m_validationLosses.back() << "\n";

        auto minValLoss = std::ranges::min_element(m_validationLosses);
        report << "- Best validation loss: " << *minValLoss << "\n";
    }

    report.close();
    std::cout << "Training report saved to: " << outputPath << '\n';
}

auto NNTrainer::splitData(const std::vector<std::pair<Board, float>>& data, float splitRatio,
                          bool takeFirst) -> std::vector<std::pair<Board, float>> {
    size_t splitIndex = static_cast<size_t>(static_cast<float>(data.size()) * splitRatio);

    if (takeFirst) {
        return std::vector<std::pair<Board, float>>(
            data.begin(), data.begin() + static_cast<std::ptrdiff_t>(splitIndex));
    } else {
        return std::vector<std::pair<Board, float>>(
            data.begin() + static_cast<std::ptrdiff_t>(splitIndex), data.end());
    }
}

auto NNTrainer::calculateLoss(const std::vector<std::pair<Board, float>>& data) -> float {
    float totalLoss = 0.0f;

    for (const auto& [board, target] : data) {
        float predicted = m_neuralNetwork.evaluatePosition(board);
        float error = predicted - target;
        totalLoss += error * error;
    }

    return totalLoss / static_cast<float>(data.size());
}