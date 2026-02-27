#pragma once

#include "../core/ChessBoard.h"
#include "search.h"

#include <algorithm>
#include <cmath>

namespace LMREnhanced {

class ReductionTable {
private:
    static constexpr int MAX_DEPTH = 64;
    static constexpr int MAX_MOVES = 64;
    static constexpr int ZERO_VALUE = 0;
    static constexpr int ONE_STEP = 1;
    static constexpr int MIN_DEPTH_FOR_REDUCTION = 3;
    static constexpr int MIN_MOVES_FOR_REDUCTION = 2;
    static constexpr double REDUCTION_BASE_OFFSET = 0.7;
    static constexpr double REDUCTION_LOG_DIVISOR = 2.25;
    static constexpr int DEPTH_REDUCTION_OFFSET = 2;
    int table[MAX_DEPTH][MAX_MOVES];

public:
    ReductionTable() {

        for (int depth = ZERO_VALUE; depth < MAX_DEPTH; ++depth) {
            for (int moves = ZERO_VALUE; moves < MAX_MOVES; ++moves) {
                if (depth < MIN_DEPTH_FOR_REDUCTION || moves < MIN_MOVES_FOR_REDUCTION) {
                    table[depth][moves] = ZERO_VALUE;
                } else {

                    double base =
                        REDUCTION_BASE_OFFSET +
                        (std::log(depth) * std::log(moves + ONE_STEP) / REDUCTION_LOG_DIVISOR);
                    table[depth][moves] = static_cast<int>(base);

                    table[depth][moves] =
                        std::min(table[depth][moves], depth - DEPTH_REDUCTION_OFFSET);
                }
            }
        }
    }

    int getReduction(int depth, int moveNumber) const {
        return table[std::min(depth, MAX_DEPTH - ONE_STEP)]
                    [std::min(moveNumber, MAX_MOVES - ONE_STEP)];
    }
};

extern ReductionTable reductionTable;

struct LMRParams {

    static constexpr int MIN_DEPTH = 2;
    static constexpr int MIN_MOVE_COUNT = 2;
    static constexpr int NO_REDUCTION = 0;
    static constexpr int HISTORY_POSITIVE_THRESHOLD = 0;
    static constexpr int MIN_DEPTH_REMAINING = 1;
    static constexpr int MAX_REDUCTION = 6;
    static constexpr int MAX_HISTORY_SCORE = 2000;
    static constexpr double HISTORY_SCORE_SCALE = 4000.0;
    static constexpr double BASE_PHASE_FACTOR = 1.0;
    static constexpr double PHASE_NORMALIZATION = 512.0;
    static constexpr double PHASE_REDUCTION_SCALE = 0.2;
    static constexpr int VERIFY_REDUCTION_THRESHOLD = 2;
    static constexpr int VERIFY_SCORE_MARGIN = 50;
    static constexpr int FAIL_COUNT_THRESHOLD = 2;
    static constexpr int REDUCTION_STEP = 1;
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
    static constexpr int DEFAULT_HISTORY_SCORE = 0;
    static constexpr int INITIAL_MOVE_NUMBER = 0;
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
          historyScore(DEFAULT_HISTORY_SCORE), moveNumber(INITIAL_MOVE_NUMBER) {}
};

struct PositionContext {
    static constexpr int INITIAL_GAME_PHASE = 0;
    static constexpr int INITIAL_STATIC_EVAL = 0;
    static constexpr int INITIAL_EVAL_TREND = 0;
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
          isSingular(false), gamePhase(INITIAL_GAME_PHASE), staticEval(INITIAL_STATIC_EVAL),
          evalTrend(INITIAL_EVAL_TREND) {}
};

inline int calculateReduction(int depth, const MoveClassification& move,
                              const PositionContext& position) {

    if (depth < LMRParams::MIN_DEPTH || move.moveNumber < LMRParams::MIN_MOVE_COUNT) {
        return LMRParams::NO_REDUCTION;
    }

    if (position.inCheck || move.isHashMove) {
        return LMRParams::NO_REDUCTION;
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
        return LMRParams::NO_REDUCTION;
    }

    if (move.historyScore > LMRParams::HISTORY_POSITIVE_THRESHOLD) {
        double historyFactor = LMRParams::BASE_PHASE_FACTOR -
                               (std::min(move.historyScore, LMRParams::MAX_HISTORY_SCORE) /
                                LMRParams::HISTORY_SCORE_SCALE);
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

    double phaseFactor =
        LMRParams::BASE_PHASE_FACTOR -
        ((position.gamePhase / LMRParams::PHASE_NORMALIZATION) * LMRParams::PHASE_REDUCTION_SCALE);
    reduction = static_cast<int>(reduction * phaseFactor);
    reduction = std::max(LMRParams::NO_REDUCTION, std::min(reduction, LMRParams::MAX_REDUCTION));
    reduction = std::min(reduction, depth - LMRParams::MIN_DEPTH_REMAINING);
    return reduction;
}

inline bool shouldVerifyReduction(int reduction, int score, int alpha, int beta) {

    return reduction >= LMRParams::VERIFY_REDUCTION_THRESHOLD &&
           (score > alpha - LMRParams::VERIFY_SCORE_MARGIN ||
            score < beta + LMRParams::VERIFY_SCORE_MARGIN);
}

inline int adjustReduction(int baseReduction, int failCount, bool improving) {

    if (failCount > LMRParams::FAIL_COUNT_THRESHOLD) {
        return baseReduction + LMRParams::REDUCTION_STEP;
    }

    if (improving) {
        return std::max(LMRParams::NO_REDUCTION, baseReduction - LMRParams::REDUCTION_STEP);
    }

    return baseReduction;
}

} // namespace LMREnhanced
