#ifndef EVALUATION_ENHANCED_H
#define EVALUATION_ENHANCED_H

#include "../core/ChessBoard.h"
#include "Evaluation.h"
#include <vector>
#include <array>

// Enhanced evaluation with sophisticated positional understanding
class EnhancedEvaluation {
public:
    // Main enhanced evaluation function
    static int evaluatePosition(const Board& board);
    
    // Pawn structure evaluation
    static int evaluatePawnStructure(const Board& board);
    static int evaluateDoubledPawns(const Board& board);
    static int evaluateIsolatedPawns(const Board& board);
    static int evaluateBackwardPawns(const Board& board);
    static int evaluatePassedPawns(const Board& board);
    static int evaluatePawnChains(const Board& board);
    static int evaluatePawnIslands(const Board& board);
    
    // Piece coordination and activity
    static int evaluatePieceCoordination(const Board& board);
    static int evaluateRookCoordination(const Board& board);
    static int evaluateBishopCoordination(const Board& board);
    static int evaluateKnightCoordination(const Board& board);
    static int evaluateQueenActivity(const Board& board);
    
    // King safety evaluation
    static int evaluateKingSafety(const Board& board);
    static int evaluatePawnShield(const Board& board, ChessPieceColor color);
    static int evaluateKingAttack(const Board& board, ChessPieceColor color);
    static int evaluateKingMobility(const Board& board, ChessPieceColor color);
    static int evaluateKingDistance(const Board& board);
    
    // Space and control evaluation
    static int evaluateSpace(const Board& board);
    static int evaluateCenterControl(const Board& board);
    static int evaluateFileControl(const Board& board);
    static int evaluateDiagonalControl(const Board& board);
    static int evaluateOutpostSquares(const Board& board);
    
    // Tactical evaluation
    static int evaluateTacticalPosition(const Board& board);
    static int evaluateHangingPieces(const Board& board);
    static int evaluatePinnedPieces(const Board& board);
    static int evaluateDiscoveredAttacks(const Board& board);
    static int evaluateForkOpportunities(const Board& board);
    static int evaluateSkewerOpportunities(const Board& board);
    
    // Endgame evaluation
    static int evaluateEndgame(const Board& board);
    static int evaluateKPK(const Board& board);
    static int evaluateKRK(const Board& board);
    static int evaluateKQK(const Board& board);
    static int evaluateKBNK(const Board& board);
    static int evaluateKBBK(const Board& board);
    static int evaluateKRPKR(const Board& board);
    static int evaluateKRPPKRP(const Board& board);
    
    // Mobility evaluation
    static int evaluateMobility(const Board& board);
    static int evaluatePieceMobility(const Board& board, ChessPieceType pieceType, ChessPieceColor color);
    static int evaluateAttackMobility(const Board& board, ChessPieceColor color);
    static int evaluateDefenseMobility(const Board& board, ChessPieceColor color);
    
    // Development and tempo
    static int evaluateDevelopment(const Board& board);
    static int evaluateTempo(const Board& board);
    static int evaluateCastling(const Board& board);
    static int evaluateCenterOccupation(const Board& board);
    
    // Material imbalance
    static int evaluateMaterialImbalance(const Board& board);
    static int evaluateBishopPair(const Board& board);
    static int evaluateRookPair(const Board& board);
    static int evaluateMinorPieceImbalance(const Board& board);
    
    // Positional patterns
    static int evaluatePositionalPatterns(const Board& board);
    static int evaluateBadBishop(const Board& board);
    static int evaluateGoodBishop(const Board& board);
    static int evaluateRookOnSeventh(const Board& board);
    static int evaluateConnectedRooks(const Board& board);
    static int evaluateRookBehindPassedPawn(const Board& board);
    
private:
    // Helper functions
    static bool isDoubledPawn(const Board& board, int file, ChessPieceColor color);
    static bool isIsolatedPawn(const Board& board, int file, ChessPieceColor color);
    static bool isBackwardPawn(const Board& board, int square, ChessPieceColor color);
    static bool isPassedPawn(const Board& board, int square, ChessPieceColor color);
    static int getPawnAdvancement(const Board& board, int square, ChessPieceColor color);
    static int getKingDistance(int king1, int king2);
    static bool isOutpost(const Board& board, int square, ChessPieceColor color);
    static bool isHanging(const Board& board, int square);
    static bool isPinned(const Board& board, int square, ChessPieceColor color);
    static int getMobilityCount(const Board& board, int square, ChessPieceType pieceType);
    static float getGamePhase(const Board& board);
    static int getMaterialCount(const Board& board, ChessPieceColor color);
    static int getPieceCount(const Board& board, ChessPieceType pieceType, ChessPieceColor color);
};

// Piece-square table with game phase interpolation
class EnhancedPieceSquareTables {
public:
    // Get piece-square value with game phase interpolation
    static int getPieceSquareValue(ChessPieceType pieceType, int square, ChessPieceColor color, float gamePhase);
    
    // Get opening piece-square value
    static int getOpeningValue(ChessPieceType pieceType, int square, ChessPieceColor color);
    
    // Get endgame piece-square value
    static int getEndgameValue(ChessPieceType pieceType, int square, ChessPieceColor color);
    
    // Interpolate between opening and endgame values
    static int interpolate(int openingValue, int endgameValue, float gamePhase);
    
private:
    // Enhanced piece-square tables
    static const std::array<int, 64> PAWN_OPENING;
    static const std::array<int, 64> PAWN_ENDGAME;
    static const std::array<int, 64> KNIGHT_OPENING;
    static const std::array<int, 64> KNIGHT_ENDGAME;
    static const std::array<int, 64> BISHOP_OPENING;
    static const std::array<int, 64> BISHOP_ENDGAME;
    static const std::array<int, 64> ROOK_OPENING;
    static const std::array<int, 64> ROOK_ENDGAME;
    static const std::array<int, 64> QUEEN_OPENING;
    static const std::array<int, 64> QUEEN_ENDGAME;
    static const std::array<int, 64> KING_OPENING;
    static const std::array<int, 64> KING_ENDGAME;
};

// Evaluation weights for tuning
class EvaluationWeights {
public:
    // Material weights
    static const int PAWN_VALUE = 100;
    static const int KNIGHT_VALUE = 320;
    static const int BISHOP_VALUE = 330;
    static const int ROOK_VALUE = 500;
    static const int QUEEN_VALUE = 900;
    static const int KING_VALUE = 20000;
    
    // Positional weights
    static const int PAWN_STRUCTURE_WEIGHT = 10;
    static const int MOBILITY_WEIGHT = 5;
    static const int KING_SAFETY_WEIGHT = 15;
    static const int CENTER_CONTROL_WEIGHT = 8;
    static const int DEVELOPMENT_WEIGHT = 12;
    static const int TEMPO_WEIGHT = 20;
    static const int MATERIAL_IMBALANCE_WEIGHT = 25;
    static const int TACTICAL_WEIGHT = 30;
    static const int ENDGAME_WEIGHT = 40;
    
    // Pawn structure weights
    static const int DOUBLED_PAWN_PENALTY = -10;
    static const int ISOLATED_PAWN_PENALTY = -15;
    static const int BACKWARD_PAWN_PENALTY = -8;
    static const int PASSED_PAWN_BONUS = 20;
    static const int PAWN_CHAIN_BONUS = 5;
    
    // King safety weights
    static const int PAWN_SHIELD_BONUS = 10;
    static const int OPEN_FILE_PENALTY = -20;
    static const int SEMI_OPEN_FILE_PENALTY = -10;
    static const int KING_ATTACK_WEIGHT = 5;
    
    // Mobility weights
    static const int KNIGHT_MOBILITY_WEIGHT = 3;
    static const int BISHOP_MOBILITY_WEIGHT = 4;
    static const int ROOK_MOBILITY_WEIGHT = 2;
    static const int QUEEN_MOBILITY_WEIGHT = 1;
    
    // Endgame weights
    static const int KING_ACTIVITY_WEIGHT = 10;
    static const int PASSED_PAWN_ADVANCE_WEIGHT = 15;
    static const int ROOK_ON_SEVENTH_BONUS = 20;
    static const int CONNECTED_ROOKS_BONUS = 10;
};

#endif // EVALUATION_ENHANCED_H 