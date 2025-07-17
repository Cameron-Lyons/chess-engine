#ifndef LMR_ENHANCED_H
#define LMR_ENHANCED_H

#include <cmath>
#include <algorithm>
#include "../core/ChessBoard.h"
#include "search.h"

namespace LMREnhanced {

// LMR reduction table - precomputed for efficiency
class ReductionTable {
private:
    // Table indexed by [depth][moveNumber]
    static constexpr int MAX_DEPTH = 64;
    static constexpr int MAX_MOVES = 64;
    int table[MAX_DEPTH][MAX_MOVES];
    
public:
    ReductionTable() {
        // Initialize reduction table with logarithmic formula
        for (int depth = 0; depth < MAX_DEPTH; ++depth) {
            for (int moves = 0; moves < MAX_MOVES; ++moves) {
                if (depth < 3 || moves < 2) {
                    table[depth][moves] = 0;
                } else {
                    // More aggressive logarithmic reduction formula
                    double base = 0.7 + std::log(depth) * std::log(moves + 1) / 2.25;
                    table[depth][moves] = static_cast<int>(base);
                    
                    // Cap maximum reduction based on remaining depth
                    table[depth][moves] = std::min(table[depth][moves], depth - 2);
                }
            }
        }
    }
    
    int getReduction(int depth, int moveNumber) const {
        return table[std::min(depth, MAX_DEPTH - 1)][std::min(moveNumber, MAX_MOVES - 1)];
    }
};

// Global reduction table instance
extern ReductionTable reductionTable;

// Enhanced LMR parameters
struct LMRParams {
    // Base conditions
    static constexpr int MIN_DEPTH = 2;        // Minimum depth for LMR
    static constexpr int MIN_MOVE_COUNT = 2;   // First N moves get full search
    static constexpr int MAX_REDUCTION = 6;    // Maximum reduction depth
    
    // Move type factors
    static constexpr double CAPTURE_FACTOR = 0.5;      // Reduce captures less
    static constexpr double CHECK_FACTOR = 0.3;        // Reduce checks much less
    static constexpr double KILLER_FACTOR = 0.6;       // Reduce killers less
    static constexpr double COUNTER_FACTOR = 0.7;      // Reduce counter moves less
    static constexpr double HISTORY_FACTOR = 0.8;      // History heuristic factor
    
    // Position factors
    static constexpr double ENDGAME_FACTOR = 0.8;      // Less reduction in endgame
    static constexpr double TACTICAL_FACTOR = 0.5;     // Less reduction in tactical positions
    static constexpr double PV_FACTOR = 0.7;           // Less reduction near PV
    
    // Dynamic adjustment factors
    static constexpr double IMPROVING_FACTOR = 0.8;    // Less reduction if position improving
    static constexpr double SINGULAR_FACTOR = 0.6;     // Less reduction for singular moves
};

// Move classification for LMR decisions
struct MoveClassification {
    bool isCapture;
    bool givesCheck;
    bool isKiller;
    bool isCounter;
    bool isHashMove;
    bool isPromotion;
    bool isCastling;
    bool isPassed;      // Passed pawn push
    bool isTactical;    // Includes forks, pins, etc.
    int historyScore;
    int moveNumber;
    
    MoveClassification() : isCapture(false), givesCheck(false), isKiller(false),
                          isCounter(false), isHashMove(false), isPromotion(false),
                          isCastling(false), isPassed(false), isTactical(false),
                          historyScore(0), moveNumber(0) {}
};

// Position evaluation for LMR decisions
struct PositionContext {
    bool inCheck;
    bool isPVNode;
    bool isEndgame;
    bool isTactical;
    bool isImproving;    // Static eval improved from 2 plies ago
    bool isSingular;     // Only one good move
    int gamePhase;       // 0 = opening, 256 = endgame
    int staticEval;
    int evalTrend;       // Eval difference from previous ply
    
    PositionContext() : inCheck(false), isPVNode(false), isEndgame(false),
                       isTactical(false), isImproving(false), isSingular(false),
                       gamePhase(0), staticEval(0), evalTrend(0) {}
};

// Main LMR decision function
inline int calculateReduction(int depth, const MoveClassification& move, 
                            const PositionContext& position) {
    // Don't reduce if conditions aren't met
    if (depth < LMRParams::MIN_DEPTH || move.moveNumber < LMRParams::MIN_MOVE_COUNT) {
        return 0;
    }
    
    // Don't reduce in check or hash moves
    if (position.inCheck || move.isHashMove) {
        return 0;
    }
    
    // Get base reduction from table
    int reduction = reductionTable.getReduction(depth, move.moveNumber);
    
    // Adjust based on move type
    if (move.isCapture) {
        reduction = static_cast<int>(reduction * LMRParams::CAPTURE_FACTOR);
    }
    
    if (move.givesCheck) {
        reduction = static_cast<int>(reduction * LMRParams::CHECK_FACTOR);
    }
    
    if (move.isKiller) {
        reduction = static_cast<int>(reduction * LMRParams::KILLER_FACTOR);
    }
    
    if (move.isCounter) {
        reduction = static_cast<int>(reduction * LMRParams::COUNTER_FACTOR);
    }
    
    // Don't reduce promotions or castling
    if (move.isPromotion || move.isCastling) {
        return 0;
    }
    
    // Adjust based on history score
    if (move.historyScore > 0) {
        double historyFactor = 1.0 - (std::min(move.historyScore, 2000) / 4000.0);
        reduction = static_cast<int>(reduction * historyFactor);
    }
    
    // Adjust based on position
    if (position.isEndgame) {
        reduction = static_cast<int>(reduction * LMRParams::ENDGAME_FACTOR);
    }
    
    if (position.isTactical || move.isTactical) {
        reduction = static_cast<int>(reduction * LMRParams::TACTICAL_FACTOR);
    }
    
    if (position.isPVNode) {
        reduction = static_cast<int>(reduction * LMRParams::PV_FACTOR);
    }
    
    if (position.isImproving) {
        reduction = static_cast<int>(reduction * LMRParams::IMPROVING_FACTOR);
    }
    
    if (position.isSingular) {
        reduction = static_cast<int>(reduction * LMRParams::SINGULAR_FACTOR);
    }
    
    // Adjust based on game phase (less reduction in endgame)
    double phaseFactor = 1.0 - (position.gamePhase / 512.0) * 0.2;
    reduction = static_cast<int>(reduction * phaseFactor);
    
    // Ensure reduction is within bounds
    reduction = std::max(0, std::min(reduction, LMRParams::MAX_REDUCTION));
    reduction = std::min(reduction, depth - 1);
    
    return reduction;
}

// Helper function to classify a move
MoveClassification classifyMove(const Board& board, const std::pair<int, int>& move,
                               const KillerMoves& killers, int ply,
                               const ThreadSafeHistory& history,
                               const std::pair<int, int>& hashMove,
                               int moveNumber);

// Helper function to evaluate position context
PositionContext evaluatePosition(const Board& board, int staticEval, 
                               int previousEval, bool isPVNode);

// Verification search after reduction
inline bool shouldVerifyReduction(int reduction, int score, int alpha, int beta) {
    // Always verify if reduction was significant and score is near bounds
    return reduction >= 2 && (score > alpha - 50 || score < beta + 50);
}

// Dynamic reduction adjustment based on search results
inline int adjustReduction(int baseReduction, int failCount, bool improving) {
    // Increase reduction if failing low repeatedly
    if (failCount > 2) {
        return baseReduction + 1;
    }
    
    // Decrease reduction if position is improving
    if (improving) {
        return std::max(0, baseReduction - 1);
    }
    
    return baseReduction;
}

} // namespace LMREnhanced

#endif // LMR_ENHANCED_H