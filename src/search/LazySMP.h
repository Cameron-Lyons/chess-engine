#pragma once

#include "../core/ChessBoard.h"
#include "search.h"

#include <atomic>
#include <memory>
#include <vector>

class LazySMP {
public:
    static constexpr int kAutoThreadCount = 0;
    static constexpr int kInitialDepth = 0;
    static constexpr int kInitialScore = -999999;
    static constexpr int kInvalidMoveSquare = -1;

    struct ThreadData {
        int id;
        Board board;
        std::atomic<bool> searching;
        std::atomic<bool> stopFlag;
        std::atomic<int> depth;
        std::atomic<int> bestScore;
        std::atomic<int> bestDepth;
        std::pair<int, int> bestMove;
        ThreadSafeHistory historyTable;
        int aspirationDelta;
        int depthOffset;

        ThreadData(int threadId)
            : id(threadId), searching(false), stopFlag(false), depth(kInitialDepth),
              bestScore(kInitialScore), bestDepth(kInitialDepth),
              bestMove({kInvalidMoveSquare, kInvalidMoveSquare}), aspirationDelta(kInitialDepth),
              depthOffset(kInitialDepth) {}
    };

    struct SharedData {
        std::atomic<bool> globalStop;
        std::atomic<int> nodesSearched;
        std::atomic<int> bestDepth;
        std::atomic<int> bestScore;
        std::pair<int, int> bestMove;
        std::mutex bestMoveMutex;

        SharedData()
            : globalStop(false), nodesSearched(kInitialDepth), bestDepth(kInitialDepth),
              bestScore(kInitialScore), bestMove({kInvalidMoveSquare, kInvalidMoveSquare}) {}
    };

private:
    struct SearchThreadArgs {
        const LazySMP* self;
        ThreadData* data;
        int maxDepth;
        int timeLimit;
        const std::vector<std::pair<int, int>>* excludedRootMoves;
    };

    static void* searchThreadEntry(void* arg);

    int numThreads;
    int hashSizeMb;
    int contempt;
    std::vector<std::unique_ptr<ThreadData>> threads;
    std::unique_ptr<SharedData> shared;
    void searchThread(ThreadData* data, int maxDepth, int timeLimit,
                      const std::vector<std::pair<int, int>>& excludedRootMoves) const;

public:
    explicit LazySMP(int threadCount = kAutoThreadCount,
                     int hashMb = SearchConstants::kDefaultTranspositionTableMb,
                     int contemptValue = SearchConstants::kZero);
    ~LazySMP();
    SearchResult search(const Board& board, int maxDepth, int timeLimit,
                        const std::vector<std::pair<int, int>>& excludedRootMoves = {});
    void stop();

    int getNodesSearched() const {
        return shared->nodesSearched.load();
    }
    int getBestDepth() const {
        return shared->bestDepth.load();
    }

    static int getAspirationDelta(int threadId);
    static int getDepthOffset(int threadId);
};

class SMPTimeManager {
private:
    std::chrono::steady_clock::time_point startTime;
    int allocatedTime;

public:
    explicit SMPTimeManager(int timeMs);
    bool shouldStop() const;
    int getElapsedTime() const;
};
