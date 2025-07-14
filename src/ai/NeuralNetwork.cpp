#include "NeuralNetwork.h"
#include "../search/ValidMoves.h"
#include "../evaluation/Evaluation.h"
#include <random>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <filesystem>

// Neural network implementation using PIMPL pattern
class NeuralNetworkEvaluator::Impl {
public:
    struct Layer {
        std::vector<std::vector<float>> weights;  // [output_size][input_size]
        std::vector<float> biases;                 // [output_size]
        std::vector<float> activations;            // [output_size]
        std::vector<float> gradients;              // [output_size]
    };
    
    NetworkConfig config;
    std::vector<Layer> layers;
    std::mt19937 rng;
    
    Impl(const NetworkConfig& config) : config(config), rng(42) {
        initializeNetwork();
    }
    
    ~Impl() = default;
    
    void initializeNetwork() {
        // Create layers: input -> hidden -> output
        std::vector<int> layerSizes = {config.inputSize, config.hiddenSize, config.outputSize};
        
        for (size_t i = 0; i < layerSizes.size() - 1; ++i) {
            Layer layer;
            int inputSize = layerSizes[i];
            int outputSize = layerSizes[i + 1];
            
            // Initialize weights with Xavier/Glorot initialization
            float scale = std::sqrt(2.0f / (inputSize + outputSize));
            std::normal_distribution<float> dist(0.0f, scale);
            
            layer.weights.resize(outputSize);
            for (int j = 0; j < outputSize; ++j) {
                layer.weights[j].resize(inputSize);
                for (int k = 0; k < inputSize; ++k) {
                    layer.weights[j][k] = dist(rng);
                }
            }
            
            // Initialize biases to zero
            layer.biases.resize(outputSize, 0.0f);
            layer.activations.resize(outputSize);
            layer.gradients.resize(outputSize);
            
            layers.push_back(std::move(layer));
        }
    }
    
    float forwardPass(const std::vector<float>& input) {
        std::vector<float> currentInput = input;
        
        for (size_t i = 0; i < layers.size(); ++i) {
            Layer& layer = layers[i];
            
            // Linear transformation: output = weights * input + bias
            for (int j = 0; j < layer.weights.size(); ++j) {
                float sum = layer.biases[j];
                for (int k = 0; k < layer.weights[j].size(); ++k) {
                    sum += layer.weights[j][k] * currentInput[k];
                }
                
                // Apply activation function
                if (i == layers.size() - 1) {
                    // Output layer: tanh for evaluation scores
                    layer.activations[j] = std::tanh(sum);
                } else {
                    // Hidden layers: ReLU
                    layer.activations[j] = std::max(0.0f, sum);
                }
            }
            
            currentInput = layer.activations;
        }
        
        return layers.back().activations[0];
    }
    
    void backwardPass(const std::vector<float>& input, float target, float predicted) {
        // Compute loss gradient
        float lossGradient = predicted - target;  // MSE derivative
        
        // Backpropagate through layers
        std::vector<float> nextGradient = {lossGradient};
        
        for (int i = layers.size() - 1; i >= 0; --i) {
            Layer& layer = layers[i];
            std::vector<float> currentGradient(layer.weights.size());
            
            // Compute gradients for this layer
            for (int j = 0; j < layer.weights.size(); ++j) {
                float activationGradient = nextGradient[j];
                
                // Apply activation derivative
                if (i == layers.size() - 1) {
                    // Output layer: tanh derivative
                    activationGradient *= (1.0f - layer.activations[j] * layer.activations[j]);
                } else {
                    // Hidden layers: ReLU derivative
                    if (layer.activations[j] <= 0) {
                        activationGradient = 0.0f;
                    }
                }
                
                layer.gradients[j] = activationGradient;
                currentGradient[j] = activationGradient;
            }
            
            // Compute gradients for previous layer
            if (i > 0) {
                std::vector<float> prevInput = (i == 0) ? input : layers[i-1].activations;
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
            
            // Update weights and biases
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
            std::cerr << "Error: Could not open file for writing: " << path << std::endl;
            return;
        }
        
        // Write config
        file.write(reinterpret_cast<const char*>(&config.inputSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.hiddenSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.outputSize), sizeof(int));
        file.write(reinterpret_cast<const char*>(&config.learningRate), sizeof(float));
        
        // Write layers
        for (const auto& layer : layers) {
            // Write weights
            for (const auto& weightRow : layer.weights) {
                file.write(reinterpret_cast<const char*>(weightRow.data()), 
                          weightRow.size() * sizeof(float));
            }
            
            // Write biases
            file.write(reinterpret_cast<const char*>(layer.biases.data()), 
                      layer.biases.size() * sizeof(float));
        }
        
        file.close();
        std::cout << "Model saved to: " << path << std::endl;
    }
    
    void loadModel(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file for reading: " << path << std::endl;
            return;
        }
        
        // Read config
        file.read(reinterpret_cast<char*>(&config.inputSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.hiddenSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.outputSize), sizeof(int));
        file.read(reinterpret_cast<char*>(&config.learningRate), sizeof(float));
        
        // Clear existing layers
        layers.clear();
        
        // Read layers
        std::vector<int> layerSizes = {config.inputSize, config.hiddenSize, config.outputSize};
        
        for (size_t i = 0; i < layerSizes.size() - 1; ++i) {
            Layer layer;
            int inputSize = layerSizes[i];
            int outputSize = layerSizes[i + 1];
            
            // Read weights
            layer.weights.resize(outputSize);
            for (int j = 0; j < outputSize; ++j) {
                layer.weights[j].resize(inputSize);
                file.read(reinterpret_cast<char*>(layer.weights[j].data()), 
                         inputSize * sizeof(float));
            }
            
            // Read biases
            layer.biases.resize(outputSize);
            file.read(reinterpret_cast<char*>(layer.biases.data()), 
                     outputSize * sizeof(float));
            
            layer.activations.resize(outputSize);
            layer.gradients.resize(outputSize);
            
            layers.push_back(std::move(layer));
        }
        
        file.close();
        std::cout << "Model loaded from: " << path << std::endl;
    }
};

NeuralNetworkEvaluator::NeuralNetworkEvaluator(const NetworkConfig& config) 
    : pImpl(std::make_unique<Impl>(config)) {
}

NeuralNetworkEvaluator::~NeuralNetworkEvaluator() = default;

float NeuralNetworkEvaluator::evaluatePosition(const Board& board) {
    std::vector<float> input = encodePosition(board);
    float evaluation = pImpl->forwardPass(input);
    
    // Convert from [-1, 1] range to centipawns
    return evaluation * 1000.0f;
}

std::vector<float> NeuralNetworkEvaluator::encodePosition(const Board& board) {
    std::vector<float> encoding(768, 0.0f);
    
    // One-hot encoding: 64 squares * 12 piece types = 768 features
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int pieceIndex = static_cast<int>(piece.PieceType) - 1;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                pieceIndex += 6;
            }
            encoding[square * 12 + pieceIndex] = 1.0f;
        }
    }
    
    return encoding;
}

void NeuralNetworkEvaluator::train(const std::vector<std::pair<Board, float>>& trainingData) {
    if (trainingData.empty()) {
        std::cerr << "Warning: No training data provided" << std::endl;
        return;
    }
    
    std::cout << "Training neural network with " << trainingData.size() << " positions..." << std::endl;
    
    float totalLoss = 0.0f;
    int batchSize = std::min(32, static_cast<int>(trainingData.size()));
    
    for (size_t i = 0; i < trainingData.size(); i += batchSize) {
        float batchLoss = 0.0f;
        
        for (int j = 0; j < batchSize && i + j < trainingData.size(); ++j) {
            const auto& [board, target] = trainingData[i + j];
            
            // Encode position
            std::vector<float> input = encodePosition(board);
            
            // Forward pass
            float predicted = pImpl->forwardPass(input);
            
            // Normalize target to [-1, 1] range
            float normalizedTarget = std::tanh(target / 1000.0f);
            
            // Backward pass
            pImpl->backwardPass(input, normalizedTarget, predicted);
            
            // Update weights
            pImpl->updateWeights(input, pImpl->config.learningRate);
            
            batchLoss += (predicted - normalizedTarget) * (predicted - normalizedTarget);
        }
        
        totalLoss += batchLoss / batchSize;
        
        if ((i / batchSize) % 100 == 0) {
            std::cout << "Batch " << (i / batchSize) << ", Loss: " << (batchLoss / batchSize) << std::endl;
        }
    }
    
    std::cout << "Training completed. Average loss: " << (totalLoss / (trainingData.size() / batchSize)) << std::endl;
}

void NeuralNetworkEvaluator::saveModel(const std::string& path) {
    pImpl->saveModel(path);
}

void NeuralNetworkEvaluator::loadModel(const std::string& path) {
    pImpl->loadModel(path);
}

float NeuralNetworkEvaluator::hybridEvaluate(const Board& board, float nnWeight) {
    // Get neural network evaluation
    float nnEval = evaluatePosition(board);
    
    // Get traditional evaluation (you'll need to implement this)
    // For now, we'll use a simple material count
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
    
    // Combine evaluations
    return nnWeight * nnEval + (1.0f - nnWeight) * traditionalEval;
}

std::vector<float> NeuralNetworkEvaluator::pieceToVector(const Piece& piece, int square) {
    (void)square; // Suppress unused parameter warning
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
    (void)gradients; // This is now handled internally in the Impl class
}

// Feature extraction implementation
PositionFeatures FeatureExtractor::extractFeatures(const Board& board) {
    PositionFeatures features = {};
    
    // Initialize material counts
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            features.materialCount[color][piece] = 0;
        }
    }
    
    // Count pieces and calculate material balance
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
    
    // Calculate material balance
    int whiteMaterial = 0, blackMaterial = 0;
    for (int piece = 0; piece < 6; ++piece) {
        whiteMaterial += features.materialCount[0][piece] * Piece::getPieceValue(static_cast<ChessPieceType>(piece + 1));
        blackMaterial += features.materialCount[1][piece] * Piece::getPieceValue(static_cast<ChessPieceType>(piece + 1));
    }
    features.materialBalance = static_cast<float>(whiteMaterial - blackMaterial);
    
    // Calculate positional features
    features.centerControl[0] = evaluateCenterControl(board);
    features.centerControl[1] = evaluateCenterControl(board);
    features.mobility[0] = calculateMobility(board, ChessPieceColor::WHITE);
    features.mobility[1] = calculateMobility(board, ChessPieceColor::BLACK);
    features.kingSafety[0] = calculateKingSafety(board, ChessPieceColor::WHITE);
    features.kingSafety[1] = calculateKingSafety(board, ChessPieceColor::BLACK);
    features.pawnStructure[0] = calculatePawnStructure(board, ChessPieceColor::WHITE);
    features.pawnStructure[1] = calculatePawnStructure(board, ChessPieceColor::BLACK);
    
    // Calculate tactical features
    features.hangingPieces[0] = evaluateHangingPieces(board);
    features.hangingPieces[1] = evaluateHangingPieces(board);
    features.pinnedPieces[0] = 0; // Simplified - would need complex pin detection
    features.pinnedPieces[1] = 0; // Simplified - would need complex pin detection
    features.discoveredAttacks[0] = 0; // Simplified - would need complex discovered attack detection
    features.discoveredAttacks[1] = 0; // Simplified - would need complex discovered attack detection
    
    // Calculate endgame features
    features.gamePhase = calculateGamePhase(board);
    features.passedPawns[0] = evaluatePassedPawns(board);
    features.passedPawns[1] = evaluatePassedPawns(board);
    features.kingDistance = 0; // Simplified - would need king distance calculation
    
    return features;
}

std::vector<float> FeatureExtractor::featuresToVector(const PositionFeatures& features) {
    std::vector<float> vec;
    
    // Add material features
    for (int color = 0; color < 2; ++color) {
        for (int piece = 0; piece < 6; ++piece) {
            vec.push_back(static_cast<float>(features.materialCount[color][piece]));
        }
    }
    
    // Add other features
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



int FeatureExtractor::calculateMobility(const Board& board, ChessPieceColor color) {
    // This is a simplified mobility calculation
    // In a full implementation, you'd generate all legal moves
    int mobility = 0;
    
    // Count pieces that can potentially move
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceColor == color) {
            // Simple heuristic: more valuable pieces get more mobility weight
            mobility += piece.PieceValue / 100;
        }
    }
    
    return mobility;
}

int FeatureExtractor::calculateKingSafety(const Board& board, ChessPieceColor color) {
    // Find king position
    int kingSquare = -1;
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::KING && piece.PieceColor == color) {
            kingSquare = square;
            break;
        }
    }
    
    if (kingSquare == -1) return 0;
    
    // Simple king safety: penalize king in center during middlegame
    int row = kingSquare / 8;
    int col = kingSquare % 8;
    
    // Penalize king in center (files c-f, ranks 2-6)
    if (col >= 2 && col <= 5 && row >= 1 && row <= 5) {
        return -100;
    }
    
    return 0;
}

int FeatureExtractor::calculatePawnStructure(const Board& board, ChessPieceColor color) {
    int score = 0;
    
    // Count pawns and check for doubled pawns
    std::vector<int> pawnsInFile(8, 0);
    
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
            int file = square % 8;
            pawnsInFile[file]++;
        }
    }
    
    // Penalize doubled pawns
    for (int file = 0; file < 8; ++file) {
        if (pawnsInFile[file] > 1) {
            score -= 50 * (pawnsInFile[file] - 1);
        }
    }
    
    return score;
}



float FeatureExtractor::calculateGamePhase(const Board& board) {
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
    
    // Game phase: 0.0 = opening, 1.0 = endgame
    // Based on piece count and pawn count
    float phase = 1.0f - (static_cast<float>(totalPieces) / 32.0f);
    phase = std::max(0.0f, std::min(1.0f, phase));
    
    return phase;
}

 

// Training data generator implementation
TrainingDataGenerator::TrainingDataGenerator() : rng(42) {
}

std::vector<TrainingDataGenerator::TrainingExample> TrainingDataGenerator::generateSelfPlayData(int numGames, int maxMoves) {
    std::vector<TrainingExample> allExamples;
    
    std::cout << "Generating " << numGames << " self-play games..." << std::endl;
    
    for (int game = 0; game < numGames; ++game) {
        if (game % 10 == 0) {
            std::cout << "Playing game " << game + 1 << "/" << numGames << std::endl;
        }
        
        auto gameExamples = playGame(maxMoves);
        allExamples.insert(allExamples.end(), gameExamples.begin(), gameExamples.end());
    }
    
    std::cout << "Generated " << allExamples.size() << " training examples" << std::endl;
    return allExamples;
}

std::vector<TrainingDataGenerator::TrainingExample> TrainingDataGenerator::playGame(int maxMoves) {
    std::vector<TrainingExample> examples;
    
    // Start with initial position
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    int moveCount = 0;
    std::vector<Board> gameHistory;
    
    while (moveCount < maxMoves) {
        // Store position for training
        TrainingExample example;
        example.position = board;
        example.gameLength = moveCount;
        
        // Generate legal moves
        std::vector<std::pair<int, int>> legalMoves;
        for (int src = 0; src < 64; ++src) {
            for (int dest = 0; dest < 64; ++dest) {
                if (IsMoveLegal(board, src, dest)) {
                    legalMoves.emplace_back(src, dest);
                }
            }
        }
        
        if (legalMoves.empty()) {
            // Game over
            example.targetScore = evaluateGameResult(board, moveCount);
            example.gameResult = example.targetScore;
            examples.push_back(example);
            break;
        }
        
        // Simple move selection: prefer captures and center moves
        std::pair<int, int> bestMove = legalMoves[0];
        float bestScore = -10000.0f;
        
        for (const auto& move : legalMoves) {
            float score = 0.0f;
            
            // Prefer captures
            if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
                score += board.squares[move.second].piece.PieceValue;
            }
            
            // Prefer center moves
            int destRow = move.second / 8;
            int destCol = move.second % 8;
            if (destRow >= 2 && destRow <= 5 && destCol >= 2 && destCol <= 5) {
                score += 50;
            }
            
            // Add some randomness
            std::uniform_real_distribution<float> noise(-100.0f, 100.0f);
            score += noise(rng);
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
        
        // Make the move
        board.movePiece(bestMove.first, bestMove.second);
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        board.updateBitboards();
        
        // Evaluate position for training
        example.targetScore = getMaterialScore(board) + getPositionalScore(board);
        example.gameResult = 0.5f; // Will be updated at game end
        examples.push_back(example);
        
        gameHistory.push_back(board);
        moveCount++;
    }
    
    // Update game results for all positions
    float finalResult = evaluateGameResult(board, moveCount);
    for (auto& example : examples) {
        example.gameResult = finalResult;
    }
    
    return examples;
}

float TrainingDataGenerator::evaluateGameResult(const Board& board, int gameLength) {
    // Check for checkmate
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
        return 0.0f; // Black wins
    } else if (blackInCheck && board.turn == ChessPieceColor::BLACK) {
        return 1.0f; // White wins
    }
    
    // Check for stalemate or draw
    if (gameLength > 150) {
        return 0.5f; // Draw by move count
    }
    
    // Evaluate material and position
    float materialScore = getMaterialScore(board);
    float positionalScore = getPositionalScore(board);
    float totalScore = materialScore + positionalScore;
    
    // Convert to win probability
    return 1.0f / (1.0f + std::exp(-totalScore / 1000.0f));
}

float TrainingDataGenerator::getMaterialScore(const Board& board) {
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

float TrainingDataGenerator::getPositionalScore(const Board& board) {
    float score = 0.0f;
    
    // Center control bonus
    int centerSquares[] = {27, 28, 35, 36}; // d4, d5, e4, e5
    for (int square : centerSquares) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            float bonus = piece.PieceValue * 0.1f;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                bonus = -bonus;
            }
            score += bonus;
        }
    }
    
    return score;
}

std::vector<std::pair<Board, float>> TrainingDataGenerator::convertToNNFormat(const std::vector<TrainingExample>& examples) {
    std::vector<std::pair<Board, float>> nnData;
    
    for (const auto& example : examples) {
        // Convert game result to centipawn score
        float score = (example.gameResult - 0.5f) * 2000.0f; // Range: -1000 to +1000 centipawns
        nnData.emplace_back(example.position, score);
    }
    
    return nnData;
}

void TrainingDataGenerator::saveTrainingData(const std::vector<TrainingExample>& data, const std::string& path) {
    // Create directory if it doesn't exist
    std::filesystem::path filePath(path);
    std::filesystem::create_directories(filePath.parent_path());
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << path << std::endl;
        return;
    }
    
    size_t numExamples = data.size();
    file.write(reinterpret_cast<const char*>(&numExamples), sizeof(size_t));
    
    for (const auto& example : data) {
        // Save FEN string
        std::string fen = example.position.toFEN();
        size_t fenLength = fen.length();
        file.write(reinterpret_cast<const char*>(&fenLength), sizeof(size_t));
        file.write(fen.c_str(), fenLength);
        
        // Save scores
        file.write(reinterpret_cast<const char*>(&example.targetScore), sizeof(float));
        file.write(reinterpret_cast<const char*>(&example.gameResult), sizeof(float));
        file.write(reinterpret_cast<const char*>(&example.gameLength), sizeof(int));
    }
    
    file.close();
    std::cout << "Training data saved to: " << path << std::endl;
}

std::vector<TrainingDataGenerator::TrainingExample> TrainingDataGenerator::loadTrainingData(const std::string& path) {
    std::vector<TrainingExample> data;
    
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << path << std::endl;
        return data;
    }
    
    size_t numExamples;
    file.read(reinterpret_cast<char*>(&numExamples), sizeof(size_t));
    
    data.reserve(numExamples);
    
    for (size_t i = 0; i < numExamples; ++i) {
        TrainingExample example;
        
        // Read FEN string
        size_t fenLength;
        file.read(reinterpret_cast<char*>(&fenLength), sizeof(size_t));
        
        std::string fen(fenLength, '\0');
        file.read(&fen[0], fenLength);
        
        example.position.InitializeFromFEN(fen);
        
        // Read scores
        file.read(reinterpret_cast<char*>(&example.targetScore), sizeof(float));
        file.read(reinterpret_cast<char*>(&example.gameResult), sizeof(float));
        file.read(reinterpret_cast<char*>(&example.gameLength), sizeof(int));
        
        data.push_back(example);
    }
    
    file.close();
    std::cout << "Training data loaded from: " << path << " (" << data.size() << " examples)" << std::endl;
    
    return data;
}

// Neural network trainer implementation
NNTrainer::NNTrainer(NeuralNetworkEvaluator& nn, const TrainingConfig& config) 
    : neuralNetwork(nn), config(config) {
}

void NNTrainer::trainOnSelfPlayData(int numGames) {
    std::cout << "Starting self-play training with " << numGames << " games..." << std::endl;
    
    // Generate training data
    auto trainingExamples = dataGenerator.generateSelfPlayData(numGames);
    auto nnData = dataGenerator.convertToNNFormat(trainingExamples);
    
    // Split into training and validation sets
    auto trainingData = splitData(nnData, 1.0f - config.validationSplit, true);
    auto validationData = splitData(nnData, config.validationSplit, false);
    
    std::cout << "Training set: " << trainingData.size() << " examples" << std::endl;
    std::cout << "Validation set: " << validationData.size() << " examples" << std::endl;
    
    // Training loop
    float bestValidationLoss = std::numeric_limits<float>::max();
    int patienceCounter = 0;
    
    for (int epoch = 0; epoch < config.epochs; ++epoch) {
        std::cout << "Epoch " << epoch + 1 << "/" << config.epochs << std::endl;
        
        // Train on training data
        neuralNetwork.train(trainingData);
        
        // Calculate losses
        float trainLoss = calculateLoss(trainingData);
        float valLoss = calculateLoss(validationData);
        
        trainingLosses.push_back(trainLoss);
        validationLosses.push_back(valLoss);
        
        std::cout << "Training loss: " << trainLoss << ", Validation loss: " << valLoss << std::endl;
        
        // Early stopping
        if (valLoss < bestValidationLoss) {
            bestValidationLoss = valLoss;
            patienceCounter = 0;
            
            // Save best model
            neuralNetwork.saveModel(config.modelPath);
        } else {
            patienceCounter++;
            if (patienceCounter >= config.earlyStoppingPatience) {
                std::cout << "Early stopping triggered after " << epoch + 1 << " epochs" << std::endl;
                break;
            }
        }
    }
    
    std::cout << "Training completed. Best validation loss: " << bestValidationLoss << std::endl;
}

void NNTrainer::trainOnFile(const std::string& dataPath) {
    std::cout << "Training on data from: " << dataPath << std::endl;
    
    // Load training data
    auto trainingExamples = dataGenerator.loadTrainingData(dataPath);
    auto nnData = dataGenerator.convertToNNFormat(trainingExamples);
    
    // Split into training and validation sets
    auto trainingData = splitData(nnData, 1.0f - config.validationSplit, true);
    auto validationData = splitData(nnData, config.validationSplit, false);
    
    std::cout << "Training set: " << trainingData.size() << " examples" << std::endl;
    std::cout << "Validation set: " << validationData.size() << " examples" << std::endl;
    
    // Train the model
    neuralNetwork.train(trainingData);
    
    // Validate
    validateModel(validationData);
}

void NNTrainer::validateModel(const std::vector<std::pair<Board, float>>& validationData) {
    float loss = calculateLoss(validationData);
    std::cout << "Validation loss: " << loss << std::endl;
}

float NNTrainer::evaluateModel(const std::vector<std::pair<Board, float>>& testData) {
    float totalLoss = 0.0f;
    int correctPredictions = 0;
    
    for (const auto& [board, target] : testData) {
        float predicted = neuralNetwork.evaluatePosition(board);
        float error = predicted - target;
        totalLoss += error * error;
        
        // Count predictions with correct sign
        if ((predicted > 0 && target > 0) || (predicted < 0 && target < 0)) {
            correctPredictions++;
        }
    }
    
    float avgLoss = totalLoss / testData.size();
    float accuracy = static_cast<float>(correctPredictions) / testData.size();
    
    std::cout << "Test loss: " << avgLoss << std::endl;
    std::cout << "Prediction accuracy: " << (accuracy * 100.0f) << "%" << std::endl;
    
    return avgLoss;
}

void NNTrainer::generateTrainingReport(const std::string& outputPath) {
    std::ofstream report(outputPath);
    if (!report.is_open()) {
        std::cerr << "Error: Could not open report file: " << outputPath << std::endl;
        return;
    }
    
    report << "Neural Network Training Report\n";
    report << "==============================\n\n";
    
    report << "Training Configuration:\n";
    report << "- Batch size: " << config.batchSize << "\n";
    report << "- Epochs: " << config.epochs << "\n";
    report << "- Validation split: " << (config.validationSplit * 100.0f) << "%\n";
    report << "- Early stopping patience: " << config.earlyStoppingPatience << "\n\n";
    
    report << "Training History:\n";
    report << "Epoch\tTraining Loss\tValidation Loss\n";
    for (size_t i = 0; i < trainingLosses.size(); ++i) {
        report << (i + 1) << "\t" << trainingLosses[i] << "\t" << validationLosses[i] << "\n";
    }
    
    if (!trainingLosses.empty()) {
        report << "\nFinal Results:\n";
        report << "- Final training loss: " << trainingLosses.back() << "\n";
        report << "- Final validation loss: " << validationLosses.back() << "\n";
        
        auto minValLoss = std::min_element(validationLosses.begin(), validationLosses.end());
        report << "- Best validation loss: " << *minValLoss << "\n";
    }
    
    report.close();
    std::cout << "Training report saved to: " << outputPath << std::endl;
}

std::vector<std::pair<Board, float>> NNTrainer::splitData(const std::vector<std::pair<Board, float>>& data, 
                                                          float splitRatio, bool takeFirst) {
    size_t splitIndex = static_cast<size_t>(data.size() * splitRatio);
    
    if (takeFirst) {
        return std::vector<std::pair<Board, float>>(data.begin(), data.begin() + splitIndex);
    } else {
        return std::vector<std::pair<Board, float>>(data.begin() + splitIndex, data.end());
    }
}

float NNTrainer::calculateLoss(const std::vector<std::pair<Board, float>>& data) {
    float totalLoss = 0.0f;
    
    for (const auto& [board, target] : data) {
        float predicted = neuralNetwork.evaluatePosition(board);
        float error = predicted - target;
        totalLoss += error * error;
    }
    
    return totalLoss / data.size();
} 