#pragma once

#include "../core/ChessBoard.h"
#include "../core/MoveContent.h"
#include "../utils/Profiler.h"
#include "MoveOrdering.h"

#include <algorithm>
#include <cmath>
#include <limits>

class SearchEnhancements {
public:
    static constexpr int ZERO = 0;
    static constexpr int ONE = 1;
    static constexpr int TWO = 2;
    static constexpr int THREE = 3;
    static constexpr int FOUR = 4;
    static constexpr int BOARD_SQUARES = 64;
    static constexpr int MAX_BOARD_INDEX = BOARD_SQUARES - ONE;
    static constexpr int LARGE_SCORE_CLAMP = 30000;
    static constexpr int TT_SCORE_LIMIT = 10000;
    static constexpr int SEARCH_INFO_MAX_PLY = 128;

    struct AspirationWindow {
        int alpha;
        int beta;
        int delta;
        static constexpr int INITIAL_DELTA = 25;
        static constexpr int MAX_DELTA = 500;

        AspirationWindow(int score = ZERO) {
            reset(score);
        }

        void reset(int score) {
            delta = INITIAL_DELTA;
            alpha = score - delta;
            beta = score + delta;
        }

        void widen(bool failHigh) {
            delta = std::min(delta * TWO, MAX_DELTA);
            if (failHigh) {
                beta += delta;
                if (beta > LARGE_SCORE_CLAMP) {
                    beta = std::numeric_limits<int>::max();
                }
            } else {
                alpha -= delta;
                if (alpha < -LARGE_SCORE_CLAMP) {
                    alpha = std::numeric_limits<int>::min();
                }
            }
        }

        bool isNarrow() const {
            return delta < MAX_DELTA;
        }
    };

    struct SingularExtension {
        static constexpr int SE_DEPTH_THRESHOLD = 8;
        static constexpr int SE_MARGIN_BASE = 2;
        static constexpr int SE_MARGIN_MULTIPLIER = 3;

        bool shouldExtend(int depth, int beta, int ttValue, int ttDepth) const {
            return depth >= SE_DEPTH_THRESHOLD && ttDepth >= depth - THREE &&
                   abs(ttValue) < TT_SCORE_LIMIT && abs(beta) < TT_SCORE_LIMIT;
        }

        int getMargin(int depth) const {
            return SE_MARGIN_BASE * depth;
        }

        int getReducedBeta(int ttValue, int depth) const {
            return ttValue - getMargin(depth);
        }
    };

    struct LateMoveReduction {
        int reductions[BOARD_SQUARES][BOARD_SQUARES];

        LateMoveReduction() {
            for (int depth = ZERO; depth < BOARD_SQUARES; depth++) {
                for (int moves = ZERO; moves < BOARD_SQUARES; moves++) {
                    if (depth < THREE || moves < THREE) {
                        reductions[depth][moves] = ZERO;
                    } else {
                        double logDepth = log(depth);
                        double logMoves = log(moves);
                        reductions[depth][moves] = int(0.75 + (logDepth * logMoves / 2.25));
                    }
                }
            }
        }

        int getReduction(int depth, int moveNum, bool pvNode, bool improving, bool inCheck,
                         bool givesCheck, bool killer, bool counter) const {
            if (depth < THREE || moveNum < THREE) {
                return ZERO;
            }
            if (inCheck || givesCheck) {
                return ZERO;
            }

            int reduction =
                reductions[std::min(depth, MAX_BOARD_INDEX)][std::min(moveNum, MAX_BOARD_INDEX)];

            if (pvNode) {
                reduction--;
            }
            if (!improving) {
                reduction++;
            }
            if (killer) {
                reduction--;
            }
            if (counter) {
                reduction--;
            }

            return std::max(ZERO, reduction);
        }
    };

    struct LateMovePruning {
        static constexpr int LMP_BASE_MOVES = 3;
        static constexpr int LMP_DEPTH_LIMIT = 6;

        bool canPrune(int depth, int moveNum, bool improving, bool inCheck, bool hasGoodCapture,
                      int eval, int beta) const {
            if (depth > LMP_DEPTH_LIMIT || inCheck || hasGoodCapture) {
                return false;
            }

            int moveLimit = LMP_BASE_MOVES + (depth * depth);
            if (!improving) {
                moveLimit /= TWO;
            }

            return moveNum > moveLimit;
        }

        int getMoveLimit(int depth, bool improving) const {
            int limit = LMP_BASE_MOVES + (depth * depth);
            if (!improving) {
                limit /= TWO;
            }
            return limit;
        }
    };

    struct MultiCut {
        static constexpr int MC_CUT_NODES = 3;
        static constexpr int MC_MAX_MOVES = 6;
        static constexpr int MC_DEPTH_THRESHOLD = 8;

        bool canApply(int depth, bool pvNode, bool inCheck) const {
            return depth >= MC_DEPTH_THRESHOLD && !pvNode && !inCheck;
        }

        bool shouldCut(int cutNodes) const {
            return cutNodes >= MC_CUT_NODES;
        }
    };

    struct FutilityPruning {
        static constexpr int FUTILITY_MARGIN_BASE = 100;
        static constexpr int FUTILITY_MARGIN_MULTIPLIER = 80;
        static constexpr int FUTILITY_DEPTH_LIMIT = 7;

        bool canPrune(int depth, int eval, int alpha, int beta, bool inCheck) const {
            if (depth > FUTILITY_DEPTH_LIMIT || inCheck) {
                return false;
            }

            int margin = FUTILITY_MARGIN_BASE + (depth * FUTILITY_MARGIN_MULTIPLIER);
            return eval + margin < alpha;
        }

        int getMargin(int depth) const {
            return FUTILITY_MARGIN_BASE + (depth * FUTILITY_MARGIN_MULTIPLIER);
        }
    };

    struct RazorPruning {
        static constexpr int RAZOR_MARGIN = 300;
        static constexpr int RAZOR_DEPTH_LIMIT = 3;

        bool canApply(int depth, int eval, int alpha, bool pvNode, bool inCheck) const {
            return depth <= RAZOR_DEPTH_LIMIT && !pvNode && !inCheck &&
                   eval + (RAZOR_MARGIN * depth) < alpha;
        }

        int getMargin(int depth) const {
            return RAZOR_MARGIN * depth;
        }
    };

    struct ProbCut {
        static constexpr int PROBCUT_DEPTH_THRESHOLD = 5;
        static constexpr int PROBCUT_MARGIN = 100;

        bool canApply(int depth, int beta, bool pvNode, bool inCheck) const {
            return depth >= PROBCUT_DEPTH_THRESHOLD && !pvNode && !inCheck &&
                   abs(beta) < TT_SCORE_LIMIT;
        }

        int getBeta(int beta, int depth) const {
            return beta + PROBCUT_MARGIN;
        }
    };

    struct StaticExchangeEvaluation {
        static constexpr int SEE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

        bool see(const Board& board, const MoveContent& move, int threshold = ZERO) const {
            int from = move.src;
            int to = move.dest;
            int score = ZERO;
            if (move.capture != ChessPieceType::NONE) {
                score = SEE_VALUES[static_cast<int>(move.capture)];
            }

            score -= threshold;
            if (score < ZERO) {
                return false;
            }

            score -= SEE_VALUES[static_cast<int>(move.piece)];
            if (score >= ZERO) {
                return true;
            }

            return seeRecursive(board, to, board.turn, score);
        }

    private:
        bool seeRecursive(const Board& board, int square, ChessPieceColor side, int score) const {
            return score >= ZERO;
        }
    };

    struct HistoryPruning {
        static constexpr int HISTORY_PRUNING_THRESHOLD = -1000;
        static constexpr int HISTORY_DEPTH_LIMIT = 3;

        bool canPrune(int depth, int historyScore, bool improving) const {
            if (depth > HISTORY_DEPTH_LIMIT) {
                return false;
            }

            int threshold = HISTORY_PRUNING_THRESHOLD;
            if (!improving) {
                threshold /= TWO;
            }

            return historyScore < threshold;
        }
    };

    struct IIDSearch {
        static constexpr int IID_DEPTH_THRESHOLD = 5;
        static constexpr int IID_REDUCTION = 2;

        bool shouldApply(int depth, bool hasTTMove, bool pvNode) const {
            return depth >= IID_DEPTH_THRESHOLD && !hasTTMove && pvNode;
        }

        int getReducedDepth(int depth) const {
            return depth - IID_REDUCTION;
        }
    };

    struct DeltaPruning {
        static constexpr int DELTA_MARGIN = 200;

        bool canPrune(int eval, int alpha, int moveValue) const {
            return eval + moveValue + DELTA_MARGIN < alpha;
        }
    };

    class SearchInfo {
    public:
        int ply = ZERO;
        int rootDepth = ZERO;
        int selectiveDepth = ZERO;
        uint64_t nodes = ZERO;
        uint64_t qnodes = ZERO;
        uint64_t tbhits = ZERO;
        bool stopped = false;
        bool pondering = false;
        int multiPV = ONE;
        int staticEval[SEARCH_INFO_MAX_PLY];
        bool improving[SEARCH_INFO_MAX_PLY];
        MoveContent killers[SEARCH_INFO_MAX_PLY][TWO];
        int historyMax = ZERO;

        void clear() {
            ply = ZERO;
            rootDepth = ZERO;
            selectiveDepth = ZERO;
            nodes = ZERO;
            qnodes = ZERO;
            tbhits = ZERO;
            stopped = false;
            pondering = false;
            std::fill(std::begin(staticEval), std::end(staticEval), ZERO);
            std::fill(std::begin(improving), std::end(improving), false);
        }

        void updateSelectiveDepth() {
            selectiveDepth = std::max(selectiveDepth, ply);
        }
    };

private:
    AspirationWindow aspiration;
    SingularExtension singular;
    LateMoveReduction lmr;
    LateMovePruning lmp;
    MultiCut multiCut;
    FutilityPruning futility;
    RazorPruning razor;
    ProbCut probCut;
    StaticExchangeEvaluation see;
    HistoryPruning historyPruning;
    IIDSearch iid;
    DeltaPruning delta;

public:
    SearchEnhancements() = default;

    AspirationWindow& getAspiration() {
        return aspiration;
    }
    const SingularExtension& getSingular() const {
        return singular;
    }
    const LateMoveReduction& getLMR() const {
        return lmr;
    }
    const LateMovePruning& getLMP() const {
        return lmp;
    }
    const MultiCut& getMultiCut() const {
        return multiCut;
    }
    const FutilityPruning& getFutility() const {
        return futility;
    }
    const RazorPruning& getRazor() const {
        return razor;
    }
    const ProbCut& getProbCut() const {
        return probCut;
    }
    const StaticExchangeEvaluation& getSEE() const {
        return see;
    }
    const HistoryPruning& getHistoryPruning() const {
        return historyPruning;
    }
    const IIDSearch& getIID() const {
        return iid;
    }
    const DeltaPruning& getDelta() const {
        return delta;
    }
};
