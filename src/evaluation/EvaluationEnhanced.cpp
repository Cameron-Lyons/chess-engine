#include "EvaluationEnhanced.h"
#include "Evaluation.h"
#include <filesystem>
#include <iostream>

std::unique_ptr<EnhancedEvaluator> g_enhancedEvaluator;

EnhancedEvaluator::EnhancedEvaluator(const EvaluationConfig& config) : config(config) {
    if (config.useNeuralNetwork) {
        neuralNetwork = std::make_unique<NeuralNetworkEvaluator>();

        if (std::filesystem::exists(config.modelPath)) {
            if (loadNeuralNetwork(config.modelPath)) {
                std::cout << "Neural network model loaded from: " << config.modelPath << std::endl;
            } else {
                std::cout << "Warning: Could not load neural network model, using untrained network"
                          << std::endl;
            }
        } else {
            std::cout << "No neural network model found, using untrained network" << std::endl;
        }
    }
}

int EnhancedEvaluator::evaluatePosition(const Board& board) {
    if (config.useNeuralNetwork && neuralNetwork) {
        return static_cast<int>(getHybridEvaluation(board));
    } else {

        return evaluateMaterial(board) + evaluatePositional(board) + evaluateTactical(board) +
               evaluateEndgame(board);
    }
}

int EnhancedEvaluator::evaluateMaterial(const Board& board) {
    int materialScore = 0;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int value = piece.PieceValue;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                value = -value;
            }
            materialScore += value;
        }
    }

    return static_cast<int>(materialScore * config.materialWeight);
}

int EnhancedEvaluator::evaluatePositional(const Board& board) {
    int positionalScore = 0;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int pstValue = getPieceSquareValue(piece.PieceType, square, piece.PieceColor);
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                pstValue = -pstValue;
            }
            positionalScore += pstValue;
        }
    }

    positionalScore += evaluatePawnStructure(board);
    positionalScore += evaluateMobility(board);
    positionalScore += evaluateCenterControl(board);
    positionalScore += evaluateKingSafety(board, ChessPieceColor::WHITE);
    positionalScore -= evaluateKingSafety(board, ChessPieceColor::BLACK);

    return static_cast<int>(positionalScore * config.positionalWeight);
}

int EnhancedEvaluator::evaluateTactical(const Board& board) {
    int tacticalScore = 0;

    tacticalScore += evaluateHangingPieces(board);
    tacticalScore += evaluateQueenTrapDanger(board);
    tacticalScore += evaluateTacticalSafety(board);

    return static_cast<int>(tacticalScore * config.tacticalWeight);
}

int EnhancedEvaluator::evaluateEndgame(const Board& board) {
    float phase = getGamePhase(board);

    if (phase > 0.8f) {

        int endgameScore = 0;
        endgameScore += evaluatePassedPawns(board);
        endgameScore += evaluateBishopPair(board);
        endgameScore += evaluateRooksOnOpenFiles(board);
        endgameScore += evaluateEndgame(board);

        return static_cast<int>(endgameScore * config.endgameWeight);
    }

    return 0;
}

float EnhancedEvaluator::getNeuralNetworkEvaluation(const Board& board) {
    if (!neuralNetwork) {
        return 0.0f;
    }

    return neuralNetwork->evaluatePosition(board);
}

float EnhancedEvaluator::getHybridEvaluation(const Board& board) {
    if (!neuralNetwork || !config.useNeuralNetwork) {

        return static_cast<float>(evaluateMaterial(board) + evaluatePositional(board) +
                                  evaluateTactical(board) + evaluateEndgame(board));
    }

    float nnEval = getNeuralNetworkEvaluation(board);

    if (!config.useTraditionalEval) {
        return nnEval;
    }

    float traditionalEval = static_cast<float>(evaluateMaterial(board) + evaluatePositional(board) +
                                               evaluateTactical(board) + evaluateEndgame(board));

    return config.nnWeight * nnEval + config.traditionalWeight * traditionalEval;
}

void EnhancedEvaluator::setConfig(const EvaluationConfig& newConfig) {
    config = newConfig;

    if (config.useNeuralNetwork && !neuralNetwork) {
        neuralNetwork = std::make_unique<NeuralNetworkEvaluator>();
        if (std::filesystem::exists(config.modelPath)) {
            loadNeuralNetwork(config.modelPath);
        }
    }
}

const EnhancedEvaluator::EvaluationConfig& EnhancedEvaluator::getConfig() const {
    return config;
}

bool EnhancedEvaluator::loadNeuralNetwork(const std::string& path) {
    if (!neuralNetwork) {
        neuralNetwork = std::make_unique<NeuralNetworkEvaluator>();
    }

    try {
        neuralNetwork->loadModel(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading neural network: " << e.what() << std::endl;
        return false;
    }
}

bool EnhancedEvaluator::isNeuralNetworkLoaded() const {
    return neuralNetwork != nullptr;
}

NeuralNetworkEvaluator* EnhancedEvaluator::getNeuralNetwork() const {
    return neuralNetwork.get();
}

float EnhancedEvaluator::getGamePhase(const Board& board) {
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
    return std::max(0.0f, std::min(1.0f, phase));
}

int EnhancedEvaluator::interpolateEvaluation(int openingEval, int endgameEval, float phase) {
    return static_cast<int>((1.0f - phase) * openingEval + phase * endgameEval);
}

void initializeEnhancedEvaluator(const EnhancedEvaluator::EvaluationConfig& config) {
    g_enhancedEvaluator = std::make_unique<EnhancedEvaluator>(config);
}

EnhancedEvaluator* getEnhancedEvaluator() {
    return g_enhancedEvaluator.get();
}

int evaluateEnhancedPosition(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->evaluatePosition(board);
    } else {

        return evaluatePosition(board);
    }
}

int evaluateEnhancedMaterial(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->evaluateMaterial(board);
    } else {

        return evaluatePosition(board);
    }
}

int evaluateEnhancedPositional(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->evaluatePositional(board);
    } else {

        return evaluatePosition(board);
    }
}

int evaluateEnhancedTactical(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->evaluateTactical(board);
    } else {

        return evaluatePosition(board);
    }
}

int evaluateEnhancedEndgame(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->evaluateEndgame(board);
    } else {

        return evaluatePosition(board);
    }
}

float evaluateWithNeuralNetwork(const Board& board) {
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->getNeuralNetworkEvaluation(board);
    } else {
        return 0.0f;
    }
}

float evaluateHybrid(const Board& board, float nnWeight) {
    (void)nnWeight;
    if (g_enhancedEvaluator) {
        return g_enhancedEvaluator->getHybridEvaluation(board);
    } else {

        return static_cast<float>(evaluatePosition(board));
    }
}