#ifndef POSITION_ANALYSIS_H
#define POSITION_ANALYSIS_H

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include <string>
#include <vector>

struct PositionAnalysis {
    // Material balance
    int materialBalance;
    int whiteMaterial;
    int blackMaterial;

    // Piece counts
    int whitePieces;
    int blackPieces;
    int whitePawns;
    int blackPawns;
    int whiteKnights;
    int blackKnights;
    int whiteBishops;
    int blackBishops;
    int whiteRooks;
    int blackRooks;
    int whiteQueens;
    int blackQueens;

    // Positional factors
    int centerControl;
    int mobility;
    int kingSafety;
    int pawnStructure;
    int pieceActivity;

    // Tactical factors
    int hangingPieces;
    int pins;
    int forks;
    int discoveredAttacks;

    // Game phase
    enum GamePhase {
        OPENING,
        MIDDLEGAME,
        ENDGAME
    } gamePhase;

    // Evaluation breakdown
    int staticEvaluation;
    int materialScore;
    int positionalScore;
    int tacticalScore;
    int endgameScore;

    // Threats and opportunities
    std::vector<std::string> threats;
    std::vector<std::string> opportunities;
    std::vector<std::string> weaknesses;

    // Move quality indicators
    int bestMoveScore;
    int secondBestMoveScore;
    int moveQualityGap;
};

class PositionAnalyzer {
public:
    static PositionAnalysis analyzePosition(const Board& board);

    static std::string formatAnalysis(const PositionAnalysis& analysis);

    static void printDetailedAnalysis(const PositionAnalysis& analysis);

private:
    static PositionAnalysis::GamePhase determineGamePhase(const Board& board);
    static int countMaterial(const Board& board, ChessPieceColor color);
    static int evaluateCenterControl(const Board& board);
    static int evaluateMobility(const Board& board);
    static int evaluateKingSafety(const Board& board);
    static int evaluatePawnStructure(const Board& board);
    static int evaluatePieceActivity(const Board& board);
    static int findHangingPieces(const Board& board);
    static int findPins(const Board& board);
    static int findForks(const Board& board);
    static int findDiscoveredAttacks(const Board& board);
    static std::vector<std::string> identifyThreats(const Board& board);
    static std::vector<std::string> identifyOpportunities(const Board& board);
    static std::vector<std::string> identifyWeaknesses(const Board& board);
};

#endif

