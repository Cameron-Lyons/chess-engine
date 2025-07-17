#include "LazySMP.h"
#include "AdvancedSearch.h"
#include "../evaluation/Evaluation.h"
#include <algorithm>
#include <random>
#include <iostream>

LazySMP::LazySMP(int threadCount) : shouldTerminate(false) {
    numThreads = threadCount > 0 ? threadCount : std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    std::cout << "Initializing Lazy SMP with " << numThreads << " threads" << std::endl;
    
    shared = std::make_unique<SharedData>();
    
    // Create thread data
    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::make_unique<ThreadData>(i));
        threads[i]->aspirationDelta = getAspirationDelta(i);
        threads[i]->depthOffset = getDepthOffset(i);
        threads[i]->useNullMove = shouldUseNullMove(i);
    }
    
    // Start worker threads
    for (int i = 0; i < numThreads; ++i) {
        workers.emplace_back(&LazySMP::workerThread, this, threads[i].get());
    }
}

LazySMP::~LazySMP() {
    // Signal threads to terminate
    shouldTerminate = true;
    poolCV.notify_all();
    
    // Wait for all threads to finish
    for (auto& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void LazySMP::workerThread(ThreadData* data) {
    while (!shouldTerminate) {
        std::unique_lock<std::mutex> lock(poolMutex);
        poolCV.wait(lock, [this, data] { 
            return shouldTerminate || data->searching.load(); 
        });
        
        if (shouldTerminate) break;
        
        if (data->searching) {
            // Thread has work to do
            lock.unlock();
            
            // Perform the search
            while (data->searching && !data->stopFlag && !shared->globalStop) {
                // The actual search is performed in searchThread
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
    
    // Initialize aspiration window based on thread ID
    int alpha = -999999;
    int beta = 999999;
    int aspirationDelta = 50 + data->aspirationDelta;
    
    // Start depth based on thread ID for diversity
    int startDepth = std::max(1, 1 + data->depthOffset);
    
    for (int depth = startDepth; depth <= maxDepth && !data->stopFlag && !shared->globalStop; ++depth) {
        data->depth = depth;
        
        // Use aspiration windows for threads > 0
        if (data->id > 0 && depth > 2 && data->bestScore > -900000) {
            alpha = data->bestScore - aspirationDelta;
            beta = data->bestScore + aspirationDelta;
        }
        
        Board boardCopy = data->board;
        int score;
        
        // Try with aspiration window
        bool aspirationFail = false;
        do {
            score = PrincipalVariationSearch(boardCopy, depth, alpha, beta, 
                boardCopy.turn == ChessPieceColor::WHITE, 0, 
                data->historyTable, context, true);
            
            // Check for aspiration window failure
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
        
        // Update thread's best move
        if (score > data->bestScore && !data->stopFlag && !shared->globalStop) {
            data->bestScore = score;
            
            // Get best move from PV
            TTEntry entry;
            uint64_t hash = ComputeZobrist(boardCopy);
            if (shared->transTable->find(hash, entry) && entry.bestMove.first != -1) {
                data->bestMove = entry.bestMove;
                
                // Update shared best move if this is better
                std::lock_guard<std::mutex> lock(shared->bestMoveMutex);
                if (depth > shared->bestDepth || 
                    (depth == shared->bestDepth && score > shared->bestScore)) {
                    shared->bestDepth = depth;
                    shared->bestScore = score;
                    shared->bestMove = data->bestMove;
                    
                    // Only main thread prints info
                    if (data->id == 0) {
                        std::cout << "info depth " << depth 
                                  << " score cp " << score
                                  << " nodes " << shared->nodesSearched.load()
                                  << " pv " << char('a' + data->bestMove.first % 8) 
                                  << (data->bestMove.first / 8 + 1)
                                  << char('a' + data->bestMove.second % 8) 
                                  << (data->bestMove.second / 8 + 1)
                                  << std::endl;
                    }
                }
            }
        }
        
        // Update shared node count
        shared->nodesSearched += context.nodeCount.load();
        context.nodeCount = 0;
        
        // Increase aspiration window for next iteration
        aspirationDelta = std::min(aspirationDelta * 2, 500);
    }
    
    data->searching = false;
}

SearchResult LazySMP::search(const Board& board, int maxDepth, int timeLimit) {
    // Reset shared data
    shared->globalStop = false;
    shared->nodesSearched = 0;
    shared->bestDepth = 0;
    shared->bestScore = -999999;
    shared->bestMove = {-1, -1};
    
    // Setup each thread
    for (int i = 0; i < numThreads; ++i) {
        threads[i]->board = board;
        threads[i]->searching = true;
        threads[i]->stopFlag = false;
        threads[i]->depth = 0;
        threads[i]->bestScore = -999999;
        threads[i]->bestMove = {-1, -1};
        // History table and killer moves are already initialized in ThreadData constructor
    }
    
    // Start search threads
    std::vector<std::thread> searchThreads;
    for (int i = 0; i < numThreads; ++i) {
        searchThreads.emplace_back(&LazySMP::searchThread, this, threads[i].get(), maxDepth, timeLimit);
    }
    
    // Time management
    SMPTimeManager timeManager(timeLimit);
    
    // Monitor search progress
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
    
    // Stop all threads
    stop();
    
    // Wait for search threads to complete
    for (auto& thread : searchThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Return best result
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

// Thread parameter diversity functions
int LazySMP::getAspirationDelta(int threadId) {
    // Different aspiration windows for each thread
    const int deltas[] = {0, 25, 50, 100, 150, 200, 300, 500};
    return deltas[threadId % 8];
}

int LazySMP::getDepthOffset(int threadId) {
    // Some threads start at different depths
    if (threadId == 0) return 0;  // Main thread starts at depth 1
    if (threadId % 4 == 1) return -1;  // Some threads start shallower
    if (threadId % 4 == 2) return 1;   // Some threads start deeper
    return 0;
}

bool LazySMP::shouldUseNullMove(int threadId) {
    // Most threads use null move, but some don't for diversity
    return threadId % 8 != 7;
}

// SMPTimeManager implementation
SMPTimeManager::SMPTimeManager(int timeMs) 
    : startTime(std::chrono::steady_clock::now()), 
      allocatedTime(timeMs), 
      hardStop(false) {}

bool SMPTimeManager::shouldStop() const {
    if (hardStop) return true;
    return getElapsedTime() >= allocatedTime;
}

int SMPTimeManager::getElapsedTime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
}

int SMPTimeManager::getRemainingTime() const {
    return std::max(0, allocatedTime - getElapsedTime());
}