#ifndef LMR_ENHANCED_H
#define LMR_ENHANCED_H

#include "../core/ChessBoard.h"
#include "search.h"
#include <algorithm>
#include <cmath>

namespace LMREnhanced {

class ReductionTable {
private:
    static constexpr int MAX_DEPTH = 64;
    static constexpr int MAX_MOVES = 64;
    int table[MAX_DEPTH][MAX_MOVES];

public:
    ReductionTable() {

        for (int depth = 0; depth < MAX_DEPTH; ++depth) {
            for (int moves = 0; moves < MAX_MOVES; ++moves) {
                if (depth < 3 || moves < 2) {
                    table[depth][moves] = 0;
                } else {

                    double base = 0.7 + std::log(depth) * std::log(moves + 1) / 2.25;
                    table[depth][moves] = static_cast<int>(base);

                    table[depth][moves] = std::min(table[depth][moves], depth - 2);
                }
            }
        }
    }

    int getReduction(int depth, int moveNumber) const {
        return table[std::min(depth, MAX_DEPTH - 1)][std::min(moveNumber, MAX_MOVES - 1)];
    }
};

extern ReductionTable reductionTable;

struct LMRParams {

    static constexpr int MIN_DEPTH = 2;
    static constexpr int MIN_MOVE_COUNT = 2;
    static constexpr int MAX_REDUCTION = 6;

    static constexpr double CAPTURE_FACTOR = 0.5;
    static constexpr double CHECK_FACTOR = 0.3;
    static constexpr double KILLER_FACTOR = 0.6;
    static constexpr double COUNTER_FACTOR = 0.7;
    static constexpr double HISTORY_FACTOR = 0.8;

    static constexpr double ENDGAME_FACTOR = 0.8;
    static constexpr double TACTICAL_FACTOR = 0.5;
    static constexpr double PV_FACTOR = 0.7;

    static constexpr double IMPROVING_FACTOR = 0.8;
    static constexpr double SINGULAR_FACTOR = 0.6;
};

struct MoveClassification {
    bool isCapture;
    bool givesCheck;
    bool isKiller;
    bool isCounter;
    bool isHashMove;
    bool isPromotion;
    bool isCastling;
    bool isPassed;
    bool isTactical;
    int historyScore;
    int moveNumber;

    MoveClassification()
        : isCapture(false), givesCheck(false), isKiller(false), isCounter(false), isHashMove(false),
          isPromotion(false), isCastling(false), isPassed(false), isTactical(false),
          historyScore(0), moveNumber(0) {}
};

struct PositionContext {
    bool inCheck;
    bool isPVNode;
    bool isEndgame;
    bool isTactical;
    bool isImproving;
    bool isSingular;
    int gamePhase;
    int staticEval;
    int evalTrend;

    PositionContext()
        : inCheck(false), isPVNode(false), isEndgame(false), isTactical(false), isImproving(false),
          isSingular(false), gamePhase(0), staticEval(0), evalTrend(0) {}
};

inline int calculateReduction(int depth, const MoveClassification& move,
                              const PositionContext& position) {

    if (depth < LMRParams::MIN_DEPTH || move.moveNumber < LMRParams::MIN_MOVE_COUNT) {
        return 0;
    }

    if (position.inCheck || move.isHashMove) {
        return 0;
    }

    int reduction = reductionTable.getReduction(depth, move.moveNumber);

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

    if (move.isPromotion || move.isCastling) {
        return 0;
    }

    if (move.historyScore > 0) {
        double historyFactor = 1.0 - (std::min(move.historyScore, 2000) / 4000.0);
        reduction = static_cast<int>(reduction * historyFactor);
    }

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

    double phaseFactor = 1.0 - (position.gamePhase / 512.0) * 0.2;
    reduction = static_cast<int>(reduction * phaseFactor);

    reduction = std::max(0, std::min(reduction, LMRParams::MAX_REDUCTION));
    reduction = std::min(reduction, depth - 1);

    return reduction;
}

MoveClassification classifyMove(const Board& board, const std::pair<int, int>& move,
                                const KillerMoves& killers, int ply,
                                const ThreadSafeHistory& history,
                                const std::pair<int, int>& hashMove, int moveNumber);

PositionContext evaluatePosition(const Board& board, int staticEval, int previousEval,
                                 bool isPVNode);

inline bool shouldVerifyReduction(int reduction, int score, int alpha, int beta) {

    return reduction >= 2 && (score > alpha - 50 || score < beta + 50);
}

inline int adjustReduction(int baseReduction, int failCount, bool improving) {

    if (failCount > 2) {
        return baseReduction + 1;
    }

    if (improving) {
        return std::max(0, baseReduction - 1);
    }

    return baseReduction;
}

} 

#endif