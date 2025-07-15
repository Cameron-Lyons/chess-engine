#ifndef SEARCH_IMPROVEMENTS_H
#define SEARCH_IMPROVEMENTS_H

#include "../core/ChessBoard.h"
#include "search.h"
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <queue>

// Advanced search improvements for stronger chess engine
class SearchImprovements {
public:
    // Multi-threaded search with work stealing
    struct ThreadData {
        std::thread thread;
        std::atomic<bool> stop;
        std::atomic<int> nodes;
        std::chrono::steady_clock::time_point startTime;
        int timeLimitMs;
        int threadId;
        
        ThreadData() : stop(false), nodes(0), timeLimitMs(0), threadId(0) {}
    };
    
    // Aspiration window search
    struct AspirationWindow {
        int alpha;
        int beta;
        int windowSize;
        int failedHigh;
        int failedLow;
        
        AspirationWindow(int initialScore = 0) 
            : alpha(initialScore - 50), beta(initialScore + 50), 
              windowSize(50), failedHigh(0), failedLow(0) {}
        
        void adjustWindow(int score, bool failedHigh, bool failedLow);
        bool shouldExpand() const;
    };
    
    // Enhanced pruning techniques
    struct PruningData {
        int staticEval;
        int depth;
        int moveNumber;
        bool inCheck;
        bool isPVNode;
        int alpha;
        int beta;
        
        PruningData() : staticEval(0), depth(0), moveNumber(0), 
                       inCheck(false), isPVNode(false), alpha(0), beta(0) {}
    };
    
    // Multi-threaded search
    static SearchResult parallelSearch(Board& board, int maxDepth, int timeLimitMs, int numThreads);
    static void workerThread(Board& board, int depth, int alpha, int beta, ThreadData& data, SearchResult& result);
    
    // Aspiration window search
    static SearchResult aspirationSearch(Board& board, int maxDepth, int timeLimitMs);
    static int aspirationWindowSearch(Board& board, int depth, AspirationWindow& window, ThreadSafeHistory& history, ParallelSearchContext& context);
    
    // Enhanced pruning
    static bool enhancedFutilityPruning(const Board& board, const PruningData& data);
    static bool enhancedNullMovePruning(const Board& board, const PruningData& data);
    static bool enhancedLateMovePruning(const Board& board, const PruningData& data);
    static bool enhancedHistoryPruning(const Board& board, const PruningData& data, const ThreadSafeHistory& history);
    
    // Multi-cut pruning
    static bool multiCutPruning(const Board& board, int depth, int alpha, int beta, int r);
    
    // Singular extensions
    static bool singularExtension(const Board& board, int depth, const std::pair<int, int>& move, int alpha, int beta);
    
    // Internal iterative deepening
    static std::pair<int, int> internalIterativeDeepening(const Board& board, int depth, int alpha, int beta);
    
    // Time management
    static int calculateTimeLimit(const Board& board, int timeLeft, int increment, int movesToGo);
    static bool shouldStopSearch(const ThreadData& data);
    static void updateTimeManagement(ThreadData& data, int nodes, int depth);
    
    // Move ordering improvements
    static std::vector<ScoredMove> enhancedMoveOrdering(const Board& board, const std::vector<std::pair<int, int>>& moves, 
                                                       const ThreadSafeHistory& history, const KillerMoves& killers, 
                                                       int ply, const std::pair<int, int>& hashMove);
    
    // Evaluation cache
    struct EvalCache {
        struct Entry {
            uint64_t hash;
            int eval;
            int depth;
            std::chrono::steady_clock::time_point timestamp;
            
            Entry() : hash(0), eval(0), depth(0) {}
            Entry(uint64_t h, int e, int d) : hash(h), eval(e), depth(d), timestamp(std::chrono::steady_clock::now()) {}
        };
        
        std::unordered_map<uint64_t, Entry> cache;
        mutable std::mutex mutex;
        static constexpr size_t MAX_SIZE = 1000000;
        
        void insert(uint64_t hash, int eval, int depth);
        bool find(uint64_t hash, int& eval, int& depth) const;
        void clear();
        void cleanup();
    };
    
    static EvalCache evalCache;
    
    // Statistics and monitoring
    struct SearchStats {
        std::atomic<int64_t> totalNodes;
        std::atomic<int64_t> tTHits;
        std::atomic<int64_t> tTProbes;
        std::atomic<int64_t> betaCutoffs;
        std::atomic<int64_t> nullMoveCutoffs;
        std::atomic<int64_t> futilityPrunings;
        std::atomic<int64_t> lateMovePrunings;
        std::chrono::steady_clock::time_point startTime;
        
        SearchStats() : totalNodes(0), tTHits(0), tTProbes(0), betaCutoffs(0), 
                       nullMoveCutoffs(0), futilityPrunings(0), lateMovePrunings(0) {}
        
        void reset();
        void print() const;
        double getNodesPerSecond() const;
    };
    
    static SearchStats stats;

private:
    // Helper functions
    static bool isTacticalPosition(const Board& board);
    static int estimateGamePhase(const Board& board);
    static bool isEndgame(const Board& board);
    static bool isPawnEndgame(const Board& board);
    static bool isKingAndPawnEndgame(const Board& board);
    
    // Advanced pruning helpers
    static int calculateFutilityMargin(int depth, bool inCheck);
    static int calculateNullMoveR(int depth, int gamePhase);
    static bool isQuietMove(const Board& board, const std::pair<int, int>& move);
    static int getMoveHistoryScore(const ThreadSafeHistory& history, const std::pair<int, int>& move);
};

// Enhanced move ordering namespace
namespace EnhancedMoveOrdering {
    // Move scoring constants (defined in search.h)
    
    // MVV-LVA scoring
    int getMVVLVA_Score(const Board& board, int fromSquare, int toSquare);
    
    // SEE (Static Exchange Evaluation) scoring
    int getSEEScore(const Board& board, const std::pair<int, int>& move);
    int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare);
    
    // History scoring
    int getHistoryScore(const ThreadSafeHistory& history, int fromSquare, int toSquare);
    
    // Positional scoring
    int getPositionalScore(const Board& board, const std::pair<int, int>& move);
    int getMobilityScore(const Board& board, const std::pair<int, int>& move);
    int getThreatScore(const Board& board, const std::pair<int, int>& move);
    
    // Combined scoring
    int getCombinedScore(const Board& board, const std::pair<int, int>& move, 
                        const ThreadSafeHistory& history, const KillerMoves& killers, 
                        int ply, const std::pair<int, int>& hashMove);
}

#endif // SEARCH_IMPROVEMENTS_H 