#include "HybridEvaluator.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "Evaluation.h"
#include "ai/NeuralNetwork.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {
constexpr int kBoardSquareCount = 64;
constexpr float kEndgamePhaseThreshold = 0.8F;
constexpr int kNoEvalScore = 0;
constexpr float kNoEvalFloat = 0.0F;
constexpr int kStartingPieceCount = 32;
constexpr float kFullPhase = 1.0F;
constexpr int kBlackScoreMultiplier = -1;
} // namespace

std::unique_ptr<HybridEvaluator> g_hybridEvaluator;

HybridEvaluator::HybridEvaluator(const EvaluationConfig& config) : config(config) {
    if (config.useNeuralNetwork) {
        neuralNetwork = std::make_unique<NeuralNetworkEvaluator>();

        if (std::filesystem::exists(config.modelPath)) {
            if (loadNeuralNetwork(config.modelPath)) {
                std::cout << "Neural network model loaded from: " << config.modelPath << "\n";
            } else {
                std::cout
                    << "Warning: Could not load neural network model, using untrained network\n";
            }
        } else {
            std::cout << "No neural network model found, using untrained network\n";
        }
    }
}

int HybridEvaluator::evaluatePosition(const Board& board) {
    if (config.useNeuralNetwork && neuralNetwork) {
        return static_cast<int>(getHybridEvaluation(board));
    } else {

        return evaluateMaterial(board) + evaluatePositional(board) + evaluateTactical(board) +
               evaluateEndgame(board);
    }
}

int HybridEvaluator::evaluateMaterial(const Board& board) const {
    int materialScore = 0;

    for (int square = 0; square < kBoardSquareCount; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int value = piece.PieceValue;
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                value *= kBlackScoreMultiplier;
            }
            materialScore += value;
        }
    }

    return static_cast<int>(static_cast<float>(materialScore) * config.materialWeight);
}

int HybridEvaluator::evaluatePositional(const Board& board) const {
    int positionalScore = 0;

    for (int square = 0; square < kBoardSquareCount; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int pstValue = getPieceSquareValue(piece.PieceType, square, piece.PieceColor);
            if (piece.PieceColor == ChessPieceColor::BLACK) {
                pstValue *= kBlackScoreMultiplier;
            }
            positionalScore += pstValue;
        }
    }

    positionalScore += evaluatePawnStructure(board);
    positionalScore += evaluateMobility(board);
    positionalScore += evaluateCenterControl(board);
    positionalScore += evaluateKingSafety(board, ChessPieceColor::WHITE);
    positionalScore -= evaluateKingSafety(board, ChessPieceColor::BLACK);
    return static_cast<int>(static_cast<float>(positionalScore) * config.positionalWeight);
}

int HybridEvaluator::evaluateTactical(const Board& board) const {
    int tacticalScore = 0;
    tacticalScore += evaluateHangingPieces(board);
    tacticalScore += evaluateQueenTrapDanger(board);
    tacticalScore += evaluateTacticalSafety(board);
    return static_cast<int>(static_cast<float>(tacticalScore) * config.tacticalWeight);
}

int HybridEvaluator::evaluateEndgame(const Board& board) {
    float phase = getGamePhase(board);

    if (phase > kEndgamePhaseThreshold) {

        int endgameScore = 0;
        endgameScore += evaluatePassedPawns(board);
        endgameScore += evaluateBishopPair(board);
        endgameScore += evaluateRooksOnOpenFiles(board);
        endgameScore += ::evaluateEndgame(board);
        return static_cast<int>(static_cast<float>(endgameScore) * config.endgameWeight);
    }

    return kNoEvalScore;
}

float HybridEvaluator::getNeuralNetworkEvaluation(const Board& board) {
    if (!neuralNetwork) {
        return kNoEvalFloat;
    }

    return neuralNetwork->evaluatePosition(board);
}

float HybridEvaluator::getHybridEvaluation(const Board& board) {
    if (!neuralNetwork || !config.useNeuralNetwork) {

        return static_cast<float>(evaluateMaterial(board) + evaluatePositional(board) +
                                  evaluateTactical(board) + evaluateEndgame(board));
    }

    float nnEval = getNeuralNetworkEvaluation(board);

    if (!config.useTraditionalEval) {
        return nnEval;
    }

    auto traditionalEval = static_cast<float>(evaluateMaterial(board) + evaluatePositional(board) +
                                              evaluateTactical(board) + evaluateEndgame(board));

    return (config.nnWeight * nnEval) + (config.traditionalWeight * traditionalEval);
}

bool HybridEvaluator::loadNeuralNetwork(const std::string& path) {
    if (!neuralNetwork) {
        neuralNetwork = std::make_unique<NeuralNetworkEvaluator>();
    }

    try {
        neuralNetwork->loadModel(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading neural network: " << e.what() << "\n";
        return false;
    }
}

NeuralNetworkEvaluator* HybridEvaluator::getNeuralNetwork() const {
    return neuralNetwork.get();
}

float HybridEvaluator::getGamePhase(const Board& board) {
    int totalPieces = 0;

    for (int square = 0; square < kBoardSquareCount; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            totalPieces++;
        }
    }

    float phase =
        kFullPhase - (static_cast<float>(totalPieces) / static_cast<float>(kStartingPieceCount));
    return std::max(kNoEvalFloat, std::min(kFullPhase, phase));
}

void initializeHybridEvaluator(const HybridEvaluator::EvaluationConfig& config) {
    g_hybridEvaluator = std::make_unique<HybridEvaluator>(config);
}

HybridEvaluator* getHybridEvaluator() {
    return g_hybridEvaluator.get();
}

float evaluateWithNeuralNetwork(const Board& board) {
    if (g_hybridEvaluator) {
        return g_hybridEvaluator->getNeuralNetworkEvaluation(board);
    } else {
        return kNoEvalFloat;
    }
}

float evaluateHybrid(const Board& board, float nnWeight) {
    (void)nnWeight;
    if (g_hybridEvaluator) {
        return g_hybridEvaluator->getHybridEvaluation(board);
    } else {

        return static_cast<float>(evaluatePosition(board));
    }
}
