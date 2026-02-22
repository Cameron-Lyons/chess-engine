#include "LazySMP.h"
#include "../evaluation/Evaluation.h"
#include "AdvancedSearch.h"

#include <algorithm>
#include <iostream>
#include <random>

namespace {
constexpr int kZero = 0;
constexpr int kDefaultThreadCount = 4;
constexpr int kSingleMillisecondSleep = 1;
constexpr int kMonitorSleepMs = 10;
constexpr int kInitialAlpha = -999999;
constexpr int kInitialBeta = 999999;
constexpr int kInvalidScore = -999999;
constexpr int kAspirationDepthMin = 2;
constexpr int kAspirationThreadIdMin = 0;
constexpr int kAspirationScoreFloor = -900000;
constexpr int kDefaultAspirationDelta = 50;
constexpr int kAspirationGrowthFactor = 2;
constexpr int kMaxAspirationDelta = 500;
constexpr int kRootPly = 0;
constexpr int kStartDepthBase = 1;
constexpr int kBoardDimension = 8;
constexpr char kFileOffset = 'a';
constexpr int kRankDisplayOffset = 1;
constexpr int kDepthOffsetModulus = 4;
constexpr int kDepthOffsetNegativeBucket = 1;
constexpr int kDepthOffsetPositiveBucket = 2;
constexpr int kDepthOffsetNegativeValue = -1;
constexpr int kDepthOffsetPositiveValue = 1;
constexpr int kNullMoveModulus = 8;
constexpr int kNullMoveDisabledBucket = 7;
constexpr int kFallbackRemainingTime = 0;
constexpr int kAspirationDeltas[] = {0, 25, 50, 100, 150, 200, 300, 500};
} // namespace

LazySMP::LazySMP(int threadCount)
    : numThreads(threadCount > kZero ? threadCount
                                     : static_cast<int>(std::thread::hardware_concurrency())),
      shouldTerminate(false) {

    if (numThreads == kZero) {
        numThreads = kDefaultThreadCount;
    }

    std::cout << "Initializing Lazy SMP with " << numThreads << " threads" << '\n';
    shared = std::make_unique<SharedData>();

    for (int i = kZero; i < numThreads; ++i) {
        threads.push_back(std::make_unique<ThreadData>(i));
        threads[i]->aspirationDelta = getAspirationDelta(i);
        threads[i]->depthOffset = getDepthOffset(i);
        threads[i]->useNullMove = shouldUseNullMove(i);
    }

    for (int i = kZero; i < numThreads; ++i) {
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

        if (shouldTerminate) {
            break;
        }

        if (data->searching) {

            lock.unlock();

            while (data->searching && !data->stopFlag && !shared->globalStop) {

                std::this_thread::sleep_for(std::chrono::milliseconds(kSingleMillisecondSleep));
            }
        }
    }
}

void LazySMP::searchThread(ThreadData* data, int maxDepth, int timeLimit) {
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimit;
    context.nodeCount = kZero;
    data->context = &context;
    int alpha = kInitialAlpha;
    int beta = kInitialBeta;
    int aspirationDelta = kDefaultAspirationDelta + data->aspirationDelta;
    int startDepth = std::max(kStartDepthBase, kStartDepthBase + data->depthOffset);
    for (int depth = startDepth; depth <= maxDepth && !data->stopFlag && !shared->globalStop;
         ++depth) {
        data->depth = depth;

        if (data->id > kAspirationThreadIdMin && depth > kAspirationDepthMin &&
            data->bestScore > kAspirationScoreFloor) {
            alpha = data->bestScore - aspirationDelta;
            beta = data->bestScore + aspirationDelta;
        }

        Board boardCopy = data->board;
        int score = kZero;
        bool aspirationFail = false;
        do {
            score = PrincipalVariationSearch(boardCopy, depth, alpha, beta,
                                             boardCopy.turn == ChessPieceColor::WHITE, kRootPly,
                                             data->historyTable, context, true);

            if (score <= alpha) {
                alpha = kInitialAlpha;
                aspirationFail = true;
            } else if (score >= beta) {
                beta = kInitialBeta;
                aspirationFail = true;
            } else {
                aspirationFail = false;
            }
        } while (aspirationFail && !data->stopFlag && !shared->globalStop);

        if (score > data->bestScore && !data->stopFlag && !shared->globalStop) {
            data->bestScore = score;
            TTEntry entry;
            uint64_t hash = ComputeZobrist(boardCopy);
            if (shared->transTable->find(hash, entry) &&
                entry.bestMove.first != kInvalidMoveSquare) {
                data->bestMove = entry.bestMove;
                std::lock_guard<std::mutex> lock(shared->bestMoveMutex);
                if (depth > shared->bestDepth ||
                    (depth == shared->bestDepth && score > shared->bestScore)) {
                    shared->bestDepth = depth;
                    shared->bestScore = score;
                    shared->bestMove = data->bestMove;

                    if (data->id == kZero) {
                        std::cout << "info depth " << depth << " score cp " << score << " nodes "
                                  << shared->nodesSearched.load() << " pv "
                                  << char(kFileOffset + (data->bestMove.first % kBoardDimension))
                                  << ((data->bestMove.first / kBoardDimension) + kRankDisplayOffset)
                                  << char(kFileOffset + (data->bestMove.second % kBoardDimension))
                                  << ((data->bestMove.second / kBoardDimension) +
                                      kRankDisplayOffset)
                                  << '\n';
                    }
                }
            }
        }

        shared->nodesSearched += context.nodeCount.load();
        context.nodeCount = kZero;
        aspirationDelta = std::min(aspirationDelta * kAspirationGrowthFactor, kMaxAspirationDelta);
    }

    data->searching = false;
}

SearchResult LazySMP::search(const Board& board, int maxDepth, int timeLimit) {

    shared->globalStop = false;
    shared->nodesSearched = kZero;
    shared->bestDepth = kZero;
    shared->bestScore = kInvalidScore;
    shared->bestMove = {kInvalidMoveSquare, kInvalidMoveSquare};

    for (int i = kZero; i < numThreads; ++i) {
        threads[i]->board = board;
        threads[i]->searching = true;
        threads[i]->stopFlag = false;
        threads[i]->depth = kZero;
        threads[i]->bestScore = kInvalidScore;
        threads[i]->bestMove = {kInvalidMoveSquare, kInvalidMoveSquare};
    }

    std::vector<std::thread> searchThreads;
    searchThreads.reserve(numThreads);
    for (int i = kZero; i < numThreads; ++i) {
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

        std::this_thread::sleep_for(std::chrono::milliseconds(kMonitorSleepMs));
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
    return kAspirationDeltas[threadId % kNullMoveModulus];
}

int LazySMP::getDepthOffset(int threadId) {
    if (threadId == kZero) {
        return kZero;
    }
    if (threadId % kDepthOffsetModulus == kDepthOffsetNegativeBucket) {
        return kDepthOffsetNegativeValue;
    }
    if (threadId % kDepthOffsetModulus == kDepthOffsetPositiveBucket) {
        return kDepthOffsetPositiveValue;
    }
    return kZero;
}

bool LazySMP::shouldUseNullMove(int threadId) {
    return threadId % kNullMoveModulus != kNullMoveDisabledBucket;
}

SMPTimeManager::SMPTimeManager(int timeMs)
    : startTime(std::chrono::steady_clock::now()), allocatedTime(timeMs), hardStop(false) {}

bool SMPTimeManager::shouldStop() const {
    if (hardStop) {
        return true;
    }
    return getElapsedTime() >= allocatedTime;
}

int SMPTimeManager::getElapsedTime() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
}

int SMPTimeManager::getRemainingTime() const {
    return std::max(kFallbackRemainingTime, allocatedTime - getElapsedTime());
}
