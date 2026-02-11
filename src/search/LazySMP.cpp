#include "LazySMP.h"
#include "../evaluation/Evaluation.h"
#include "AdvancedSearch.h"

#include <algorithm>
#include <iostream>
#include <random>

LazySMP::LazySMP(int threadCount) : shouldTerminate(false) {
    numThreads = threadCount > 0 ? threadCount : std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;

    std::cout << "Initializing Lazy SMP with " << numThreads << " threads" << std::endl;

    shared = std::make_unique<SharedData>();

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<ThreadData>(i));
        threads[i]->aspirationDelta = getAspirationDelta(i);
        threads[i]->depthOffset = getDepthOffset(i);
        threads[i]->useNullMove = shouldUseNullMove(i);
    }

    for (int i = 0; i < numThreads; ++i) {
        workers.emplace_back(&LazySMP::workerThread, this, threads[i].get());
    }
}

LazySMP::~LazySMP() {

    shouldTerminate = true;
    poolCV.notify_all();

    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void LazySMP::workerThread(ThreadData* data) {
    while (!shouldTerminate) {
        std::unique_lock<std::mutex> lock(poolMutex);
        poolCV.wait(lock, [this, data] { return shouldTerminate || data->searching.load(); });

        if (shouldTerminate)
            break;

        if (data->searching) {

            lock.unlock();

            while (data->searching && !data->stopFlag && !shared->globalStop) {

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void LazySMP::searchThread(ThreadData* data, int maxDepth, int timeLimit) {
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimit;
    context.nodeCount = 0;
    data->context = &context;

    int alpha = -999999;
    int beta = 999999;
    int aspirationDelta = 50 + data->aspirationDelta;

    int startDepth = std::max(1, 1 + data->depthOffset);

    for (int depth = startDepth; depth <= maxDepth && !data->stopFlag && !shared->globalStop;
         ++depth) {
        data->depth = depth;

        if (data->id > 0 && depth > 2 && data->bestScore > -900000) {
            alpha = data->bestScore - aspirationDelta;
            beta = data->bestScore + aspirationDelta;
        }

        Board boardCopy = data->board;
        int score;

        bool aspirationFail = false;
        do {
            score = PrincipalVariationSearch(boardCopy, depth, alpha, beta,
                                             boardCopy.turn == ChessPieceColor::WHITE, 0,
                                             data->historyTable, context, true);

            if (score <= alpha) {
                alpha = -999999;
                aspirationFail = true;
            } else if (score >= beta) {
                beta = 999999;
                aspirationFail = true;
            } else {
                aspirationFail = false;
            }
        } while (aspirationFail && !data->stopFlag && !shared->globalStop);

        if (score > data->bestScore && !data->stopFlag && !shared->globalStop) {
            data->bestScore = score;

            TTEntry entry;
            uint64_t hash = ComputeZobrist(boardCopy);
            if (shared->transTable->find(hash, entry) && entry.bestMove.first != -1) {
                data->bestMove = entry.bestMove;

                std::lock_guard<std::mutex> lock(shared->bestMoveMutex);
                if (depth > shared->bestDepth ||
                    (depth == shared->bestDepth && score > shared->bestScore)) {
                    shared->bestDepth = depth;
                    shared->bestScore = score;
                    shared->bestMove = data->bestMove;

                    if (data->id == 0) {
                        std::cout << "info depth " << depth << " score cp " << score << " nodes "
                                  << shared->nodesSearched.load() << " pv "
                                  << char('a' + data->bestMove.first % 8)
                                  << (data->bestMove.first / 8 + 1)
                                  << char('a' + data->bestMove.second % 8)
                                  << (data->bestMove.second / 8 + 1) << std::endl;
                    }
                }
            }
        }

        shared->nodesSearched += context.nodeCount.load();
        context.nodeCount = 0;

        aspirationDelta = std::min(aspirationDelta * 2, 500);
    }

    data->searching = false;
}

SearchResult LazySMP::search(const Board& board, int maxDepth, int timeLimit) {

    shared->globalStop = false;
    shared->nodesSearched = 0;
    shared->bestDepth = 0;
    shared->bestScore = -999999;
    shared->bestMove = {-1, -1};

    for (int i = 0; i < numThreads; ++i) {
        threads[i]->board = board;
        threads[i]->searching = true;
        threads[i]->stopFlag = false;
        threads[i]->depth = 0;
        threads[i]->bestScore = -999999;
        threads[i]->bestMove = {-1, -1};
    }

    std::vector<std::thread> searchThreads;
    for (int i = 0; i < numThreads; ++i) {
        searchThreads.emplace_back(&LazySMP::searchThread, this, threads[i].get(), maxDepth,
                                   timeLimit);
    }

    SMPTimeManager timeManager(timeLimit);

    while (!timeManager.shouldStop()) {
        bool allStopped = true;
        for (const auto& thread : threads) {
            if (thread->searching) {
                allStopped = false;
                break;
            }
        }

        if (allStopped || shared->bestDepth >= maxDepth) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    stop();

    for (auto& thread : searchThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    SearchResult result;
    result.bestMove = shared->bestMove;
    result.score = shared->bestScore;
    result.depth = shared->bestDepth;
    result.nodes = shared->nodesSearched;

    return result;
}

void LazySMP::stop() {
    shared->globalStop = true;
    for (auto& thread : threads) {
        thread->stopFlag = true;
    }
}

int LazySMP::getAspirationDelta(int threadId) {

    const int deltas[] = {0, 25, 50, 100, 150, 200, 300, 500};
    return deltas[threadId % 8];
}

int LazySMP::getDepthOffset(int threadId) {

    if (threadId == 0)
        return 0;
    if (threadId % 4 == 1)
        return -1;
    if (threadId % 4 == 2)
        return 1;
    return 0;
}

bool LazySMP::shouldUseNullMove(int threadId) {

    return threadId % 8 != 7;
}

SMPTimeManager::SMPTimeManager(int timeMs)
    : startTime(std::chrono::steady_clock::now()), allocatedTime(timeMs), hardStop(false) {}

bool SMPTimeManager::shouldStop() const {
    if (hardStop)
        return true;
    return getElapsedTime() >= allocatedTime;
}

int SMPTimeManager::getElapsedTime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}

int SMPTimeManager::getRemainingTime() const {
    return std::max(0, allocatedTime - getElapsedTime());
}