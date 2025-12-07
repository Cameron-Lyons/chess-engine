#ifndef LAZY_SMP_H
#define LAZY_SMP_H

#include "../core/ChessBoard.h"
#include "search.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>

class LazySMP {
public:
    struct ThreadData {
        int id;
        Board board;
        std::atomic<bool> searching;
        std::atomic<bool> stopFlag;
        std::atomic<int> depth;
        std::atomic<int> bestScore;
        std::pair<int, int> bestMove;
        ThreadSafeHistory historyTable;
        KillerMoves killerMoves;
        ParallelSearchContext* context;

        int aspirationDelta;
        int depthOffset;
        bool useNullMove;

        ThreadData(int threadId)
            : id(threadId), searching(false), stopFlag(false), depth(0), bestScore(-999999),
              bestMove({-1, -1}), context(nullptr), aspirationDelta(0), depthOffset(0),
              useNullMove(true) {}
    };

    struct SharedData {
        std::atomic<bool> globalStop;
        std::atomic<int> nodesSearched;
        std::atomic<int> bestDepth;
        std::atomic<int> bestScore;
        std::pair<int, int> bestMove;
        std::mutex bestMoveMutex;

        ThreadSafeTT* transTable;

        SharedData()
            : globalStop(false), nodesSearched(0), bestDepth(0), bestScore(-999999),
              bestMove({-1, -1}), transTable(&TransTable) {}
    };

private:
    int numThreads;
    std::vector<std::unique_ptr<ThreadData>> threads;
    std::unique_ptr<SharedData> shared;
    std::vector<std::thread> workers;

    std::mutex poolMutex;
    std::condition_variable poolCV;
    std::atomic<bool> shouldTerminate;

    void workerThread(ThreadData* data);
    void searchThread(ThreadData* data, int maxDepth, int timeLimit);

public:
    explicit LazySMP(int threadCount = 0);
    ~LazySMP();

    SearchResult search(const Board& board, int maxDepth, int timeLimit);

    void stop();

    int getNodesSearched() const {
        return shared->nodesSearched.load();
    }
    int getBestDepth() const {
        return shared->bestDepth.load();
    }

    static int getAspirationDelta(int threadId);
    static int getDepthOffset(int threadId);
    static bool shouldUseNullMove(int threadId);
};

class SMPTimeManager {
private:
    std::chrono::steady_clock::time_point startTime;
    int allocatedTime;
    std::atomic<bool> hardStop;

public:
    explicit SMPTimeManager(int timeMs);

    bool shouldStop() const;
    void forceStop() {
        hardStop = true;
    }
    int getElapsedTime() const;
    int getRemainingTime() const;
};

#endif