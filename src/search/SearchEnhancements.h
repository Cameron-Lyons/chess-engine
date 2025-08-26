#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include "../core/ChessBoard.h"
#include "../core/MoveContent.h"
#include "MoveOrdering.h"
#include "../utils/Profiler.h"

class SearchEnhancements {
public:
    struct AspirationWindow {
        int alpha;
        int beta;
        int delta;
        static constexpr int INITIAL_DELTA = 25;
        static constexpr int MAX_DELTA = 500;
        
        AspirationWindow(int score = 0) {
            reset(score);
        }
        
        void reset(int score) {
            delta = INITIAL_DELTA;
            alpha = score - delta;
            beta = score + delta;
        }
        
        void widen(bool failHigh) {
            delta = std::min(delta * 2, MAX_DELTA);
            if (failHigh) {
                beta += delta;
                if (beta > 30000) beta = std::numeric_limits<int>::max();
            } else {
                alpha -= delta;
                if (alpha < -30000) alpha = std::numeric_limits<int>::min();
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
            return depth >= SE_DEPTH_THRESHOLD &&
                   ttDepth >= depth - 3 &&
                   abs(ttValue) < 10000 &&
                   abs(beta) < 10000;
        }
        
        int getMargin(int depth) const {
            return SE_MARGIN_BASE * depth;
        }
        
        int getReducedBeta(int ttValue, int depth) const {
            return ttValue - getMargin(depth);
        }
    };
    
    struct LateMoveReduction {
        int reductions[64][64];
        
        LateMoveReduction() {
            for (int depth = 0; depth < 64; depth++) {
                for (int moves = 0; moves < 64; moves++) {
                    if (depth < 3 || moves < 3) {
                        reductions[depth][moves] = 0;
                    } else {
                        double logDepth = log(depth);
                        double logMoves = log(moves);
                        reductions[depth][moves] = int(0.75 + logDepth * logMoves / 2.25);
                    }
                }
            }
        }
        
        int getReduction(int depth, int moveNum, bool pvNode, bool improving, 
                         bool inCheck, bool givesCheck, bool killer, bool counter) const {
            if (depth < 3 || moveNum < 3) return 0;
            if (inCheck || givesCheck) return 0;
            
            int reduction = reductions[std::min(depth, 63)][std::min(moveNum, 63)];
            
            if (pvNode) reduction--;
            if (!improving) reduction++;
            if (killer) reduction--;
            if (counter) reduction--;
            
            return std::max(0, reduction);
        }
    };
    
    struct LateMovePruning {
        static constexpr int LMP_BASE_MOVES = 3;
        
        bool canPrune(int depth, int moveNum, bool improving, bool inCheck, 
                     bool hasGoodCapture, int eval, int beta) const {
            if (depth > 6 || inCheck || hasGoodCapture) return false;
            
            int moveLimit = LMP_BASE_MOVES + depth * depth;
            if (!improving) moveLimit /= 2;
            
            return moveNum > moveLimit;
        }
        
        int getMoveLimit(int depth, bool improving) const {
            int limit = LMP_BASE_MOVES + depth * depth;
            if (!improving) limit /= 2;
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
            if (depth > FUTILITY_DEPTH_LIMIT || inCheck) return false;
            
            int margin = FUTILITY_MARGIN_BASE + depth * FUTILITY_MARGIN_MULTIPLIER;
            return eval + margin < alpha;
        }
        
        int getMargin(int depth) const {
            return FUTILITY_MARGIN_BASE + depth * FUTILITY_MARGIN_MULTIPLIER;
        }
    };
    
    struct RazorPruning {
        static constexpr int RAZOR_MARGIN = 300;
        static constexpr int RAZOR_DEPTH_LIMIT = 3;
        
        bool canApply(int depth, int eval, int alpha, bool pvNode, bool inCheck) const {
            return depth <= RAZOR_DEPTH_LIMIT && 
                   !pvNode && 
                   !inCheck &&
                   eval + RAZOR_MARGIN * depth < alpha;
        }
        
        int getMargin(int depth) const {
            return RAZOR_MARGIN * depth;
        }
    };
    
    struct ProbCut {
        static constexpr int PROBCUT_DEPTH_THRESHOLD = 5;
        static constexpr int PROBCUT_MARGIN = 100;
        
        bool canApply(int depth, int beta, bool pvNode, bool inCheck) const {
            return depth >= PROBCUT_DEPTH_THRESHOLD &&
                   !pvNode &&
                   !inCheck &&
                   abs(beta) < 10000;
        }
        
        int getBeta(int beta, int depth) const {
            return beta + PROBCUT_MARGIN;
        }
    };
    
    struct StaticExchangeEvaluation {
        static constexpr int SEE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};
        
        bool see(const Board& board, const MoveContent& move, int threshold = 0) const {
            int from = move.src;
            int to = move.dest;
            
            int score = 0;
            if (move.capture != ChessPieceType::NONE) {
                score = SEE_VALUES[static_cast<int>(move.capture)];
            }
            
            score -= threshold;
            if (score < 0) return false;
            
            score -= SEE_VALUES[static_cast<int>(move.piece)];
            if (score >= 0) return true;
            
            return seeRecursive(board, to, board.turn, score);
        }
        
    private:
        bool seeRecursive(const Board& board, int square, ChessPieceColor side, int score) const {
            return score >= 0;
        }
    };
    
    struct HistoryPruning {
        static constexpr int HISTORY_PRUNING_THRESHOLD = -1000;
        static constexpr int HISTORY_DEPTH_LIMIT = 3;
        
        bool canPrune(int depth, int historyScore, bool improving) const {
            if (depth > HISTORY_DEPTH_LIMIT) return false;
            
            int threshold = HISTORY_PRUNING_THRESHOLD;
            if (!improving) threshold /= 2;
            
            return historyScore < threshold;
        }
    };
    
    struct IIDSearch {
        static constexpr int IID_DEPTH_THRESHOLD = 5;
        static constexpr int IID_REDUCTION = 2;
        
        bool shouldApply(int depth, bool hasTTMove, bool pvNode) const {
            return depth >= IID_DEPTH_THRESHOLD && 
                   !hasTTMove && 
                   pvNode;
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
        int ply = 0;
        int rootDepth = 0;
        int selectiveDepth = 0;
        uint64_t nodes = 0;
        uint64_t qnodes = 0;
        uint64_t tbhits = 0;
        bool stopped = false;
        bool pondering = false;
        int multiPV = 1;
        
        int staticEval[128];
        bool improving[128];
        MoveContent killers[128][2];
        int historyMax = 0;
        
        void clear() {
            ply = 0;
            rootDepth = 0;
            selectiveDepth = 0;
            nodes = 0;
            qnodes = 0;
            tbhits = 0;
            stopped = false;
            pondering = false;
            std::fill(std::begin(staticEval), std::end(staticEval), 0);
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
    
    AspirationWindow& getAspiration() { return aspiration; }
    const SingularExtension& getSingular() const { return singular; }
    const LateMoveReduction& getLMR() const { return lmr; }
    const LateMovePruning& getLMP() const { return lmp; }
    const MultiCut& getMultiCut() const { return multiCut; }
    const FutilityPruning& getFutility() const { return futility; }
    const RazorPruning& getRazor() const { return razor; }
    const ProbCut& getProbCut() const { return probCut; }
    const StaticExchangeEvaluation& getSEE() const { return see; }
    const HistoryPruning& getHistoryPruning() const { return historyPruning; }
    const IIDSearch& getIID() const { return iid; }
    const DeltaPruning& getDelta() const { return delta; }
};