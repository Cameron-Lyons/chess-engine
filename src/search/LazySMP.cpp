#include "LazySMP.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <utility>
#include <vector>

namespace {
constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kDefaultThreadCount = 4;
constexpr int kMonitorSleepMs = 10;
constexpr int kInvalidScore = -999999;
constexpr int kSearchWindowLimit = 32000;
constexpr int kScoreClampLimit = kSearchWindowLimit;
constexpr int kAspirationDepthMin = 2;
constexpr int kAspirationThreadIdMin = 0;
constexpr int kAspirationScoreFloor = -900000;
constexpr int kDefaultAspirationDelta = 50;
constexpr int kAspirationGrowthFactor = 2;
constexpr int kMaxAspirationDelta = 500;
constexpr int kMaxAspirationAttempts = 6;
constexpr int kMaxSmpSearchDepth = 20;
constexpr int kRootPly = 0;
constexpr int kStartDepthBase = 1;
constexpr int kBoardDimension = 8;
constexpr char kFileOffset = 'a';
constexpr int kRankDisplayOffset = 1;
constexpr int kMoveSquareLimit = 64;
constexpr int kMovePackShift = 6;
constexpr int kDepthOffsetModulus = 4;
constexpr int kDepthOffsetNegativeBucket = 1;
constexpr int kDepthOffsetPositiveBucket = 2;
constexpr int kDepthOffsetNegativeValue = -1;
constexpr int kDepthOffsetPositiveValue = 1;
constexpr int kNullMoveModulus = 8;
constexpr int kAspirationDeltas[] = {0, 25, 50, 100, 150, 200, 300, 500};
constexpr std::size_t kSearchThreadStackBytes = 8ULL * 1024ULL * 1024ULL;

bool isValidMove(std::pair<int, int> move) {
    return move.first >= kZero && move.first < kMoveSquareLimit && move.second >= kZero &&
           move.second < kMoveSquareLimit && move.first != move.second;
}

bool isPreferredMove(std::pair<int, int> lhs, std::pair<int, int> rhs) {
    if (rhs.first < kZero) {
        return true;
    }
    if (lhs.first != rhs.first) {
        return lhs.first < rhs.first;
    }
    return lhs.second < rhs.second;
}

int clampScore(int score) {
    return std::clamp(score, -kScoreClampLimit, kScoreClampLimit);
}

std::pair<int, int> selectFallbackRootMove(const Board& board) {
    Board rootBoard = board;
    GenValidMoves(rootBoard);
    const auto moves = GetAllMoves(rootBoard, rootBoard.turn);
    for (const auto& move : moves) {
        Board testBoard = rootBoard;
        if (!applySearchMove(testBoard, move.first, move.second)) {
            continue;
        }
        if (!isInCheck(testBoard, rootBoard.turn)) {
            return move;
        }
    }
    if (!moves.empty()) {
        return moves.front();
    }
    return {LazySMP::kInvalidMoveSquare, LazySMP::kInvalidMoveSquare};
}

std::uint16_t packMoveKey(std::pair<int, int> move) {
    return static_cast<std::uint16_t>((move.first << kMovePackShift) | move.second);
}

} // namespace

LazySMP::LazySMP(int threadCount, int hashMb, int contemptValue)
    : numThreads(threadCount > kZero ? threadCount
                                     : static_cast<int>(std::thread::hardware_concurrency())),
      hashSizeMb(std::max(SearchConstants::kOne, hashMb)), contempt(contemptValue) {

    if (numThreads == kZero) {
        numThreads = kDefaultThreadCount;
    }

    std::cout << "Initializing Lazy SMP with " << numThreads << " threads" << '\n';
    shared = std::make_unique<SharedData>();

    for (int i = kZero; i < numThreads; ++i) {
        threads.push_back(std::make_unique<ThreadData>(i));
        threads[i]->aspirationDelta = getAspirationDelta(i);
        threads[i]->depthOffset = getDepthOffset(i);
    }
}

LazySMP::~LazySMP() = default;

void* LazySMP::searchThreadEntry(void* arg) {
    auto* args = static_cast<SearchThreadArgs*>(arg);
    args->self->searchThread(args->data, args->maxDepth, args->timeLimit, *args->excludedRootMoves);
    return nullptr;
}

void LazySMP::searchThread(ThreadData* data, int maxDepth, int timeLimit,
                           const std::vector<std::pair<int, int>>& excludedRootMoves) const {
    auto context = std::make_unique<ParallelSearchContext>(numThreads);
    context->startTime = std::chrono::steady_clock::now();
    context->timeLimitMs = timeLimit;
    context->contempt = contempt;
    context->excludedRootMoves = excludedRootMoves;
    context->transTable.resize(
        std::max(SearchConstants::kOne, hashSizeMb / std::max(SearchConstants::kOne, numThreads)));
    context->transTable.newSearch();
    context->nodeCount = kZero;
    int aspirationDelta = kDefaultAspirationDelta + data->aspirationDelta;
    int startDepth = std::max(kStartDepthBase, kStartDepthBase + data->depthOffset);
    const uint64_t rootHash = ComputeZobrist(data->board);
    for (int depth = startDepth; depth <= maxDepth && !data->stopFlag && !shared->globalStop;
         ++depth) {
        data->depth = depth;
        int alpha = -kSearchWindowLimit;
        int beta = kSearchWindowLimit;
        if (data->id > kAspirationThreadIdMin && depth > kAspirationDepthMin) {
            const int center = clampScore(data->bestScore.load());
            if (center > kAspirationScoreFloor) {
                alpha = std::max(-kSearchWindowLimit, center - aspirationDelta);
                beta = std::min(kSearchWindowLimit, center + aspirationDelta);
            }
        }

        int score = kZero;
        for (int attempt = kZero;
             attempt < kMaxAspirationAttempts && !data->stopFlag && !shared->globalStop;
             ++attempt) {
            Board boardCopy = data->board;
            score = PrincipalVariationSearch(boardCopy, depth, alpha, beta,
                                             boardCopy.turn == ChessPieceColor::WHITE, kRootPly,
                                             data->historyTable, *context, true);

            if (score <= alpha) {
                if (alpha <= -kSearchWindowLimit) {
                    break;
                }
                alpha = -kSearchWindowLimit;
            } else if (score >= beta) {
                if (beta >= kSearchWindowLimit) {
                    break;
                }
                beta = kSearchWindowLimit;
            } else {
                break;
            }
        }

        score = clampScore(score);
        std::pair<int, int> candidateMove = {kInvalidMoveSquare, kInvalidMoveSquare};
        TTEntry entry;
        if (context->transTable.find(rootHash, entry) && isValidMove(entry.bestMove)) {
            candidateMove = entry.bestMove;
        }
        if (!isValidMove(candidateMove)) {
            candidateMove = selectFallbackRootMove(data->board);
        }

        if (isValidMove(candidateMove)) {
            data->bestMove = candidateMove;
            data->bestScore = score;
            data->bestDepth = depth;

            std::lock_guard<std::mutex> lock(shared->bestMoveMutex);
            if (depth > shared->bestDepth ||
                (depth == shared->bestDepth && score > shared->bestScore) ||
                (depth == shared->bestDepth && score == shared->bestScore &&
                 isPreferredMove(candidateMove, shared->bestMove))) {
                shared->bestDepth = depth;
                shared->bestScore = score;
                shared->bestMove = candidateMove;

                if (data->id == kZero) {
                    std::cout << "info depth " << depth << " score cp " << score << " nodes "
                              << shared->nodesSearched.load() << " pv "
                              << char(kFileOffset + (candidateMove.first % kBoardDimension))
                              << ((candidateMove.first / kBoardDimension) + kRankDisplayOffset)
                              << char(kFileOffset + (candidateMove.second % kBoardDimension))
                              << ((candidateMove.second / kBoardDimension) + kRankDisplayOffset)
                              << '\n';
                }
            }
        }

        shared->nodesSearched += context->nodeCount.load();
        context->nodeCount = kZero;
        aspirationDelta = std::min(aspirationDelta * kAspirationGrowthFactor, kMaxAspirationDelta);
    }

    data->searching = false;
}

SearchResult LazySMP::search(const Board& board, int maxDepth, int timeLimit,
                             const std::vector<std::pair<int, int>>& excludedRootMoves) {
    const int effectiveMaxDepth = std::max(kStartDepthBase, std::min(maxDepth, kMaxSmpSearchDepth));

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
        threads[i]->bestDepth = kZero;
        threads[i]->bestMove = {kInvalidMoveSquare, kInvalidMoveSquare};
    }

    std::vector<pthread_t> searchThreads(static_cast<std::size_t>(numThreads));
    std::vector<SearchThreadArgs> searchArgs(static_cast<std::size_t>(numThreads));
    std::vector<bool> searchThreadCreated(static_cast<std::size_t>(numThreads), false);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, kSearchThreadStackBytes);

    for (int i = kZero; i < numThreads; ++i) {
        searchArgs[static_cast<std::size_t>(i)] = {this, threads[i].get(), effectiveMaxDepth,
                                                   timeLimit, &excludedRootMoves};
        if (pthread_create(&searchThreads[static_cast<std::size_t>(i)], &attr,
                           &LazySMP::searchThreadEntry,
                           &searchArgs[static_cast<std::size_t>(i)]) != 0) {
            threads[i]->stopFlag = true;
            threads[i]->searching = false;
        } else {
            searchThreadCreated[static_cast<std::size_t>(i)] = true;
        }
    }
    pthread_attr_destroy(&attr);

    SMPTimeManager timeManager(timeLimit);

    while (!timeManager.shouldStop()) {
        bool allStopped = true;
        for (const auto& thread : threads) {
            if (thread->searching) {
                allStopped = false;
                break;
            }
        }

        if (allStopped || shared->bestDepth >= effectiveMaxDepth) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kMonitorSleepMs));
    }

    stop();

    for (int i = kZero; i < numThreads; ++i) {
        if (searchThreadCreated[static_cast<std::size_t>(i)]) {
            pthread_join(searchThreads[static_cast<std::size_t>(i)], nullptr);
        }
    }

    SearchResult result;
    {
        std::lock_guard<std::mutex> lock(shared->bestMoveMutex);
        result.bestMove = shared->bestMove;
        result.score = shared->bestScore;
        result.depth = shared->bestDepth;
    }
    result.nodes = shared->nodesSearched;

    if (!isValidMove(result.bestMove)) {
        struct MoveStats {
            int votes = kZero;
            int depth = kZero;
            int score = kInvalidScore;
        };
        std::map<std::uint16_t, MoveStats> moveVotes;

        for (const auto& thread : threads) {
            const std::pair<int, int> move = thread->bestMove;
            if (!isValidMove(move)) {
                continue;
            }
            const int depth = thread->bestDepth.load();
            const int score = clampScore(thread->bestScore.load());
            MoveStats& stats = moveVotes[packMoveKey(move)];
            stats.votes += std::max(kStartDepthBase, depth);
            if (depth > stats.depth || (depth == stats.depth && score > stats.score)) {
                stats.depth = depth;
                stats.score = score;
            }
        }

        std::uint16_t chosenKey = 0;
        bool foundConsensus = false;
        MoveStats chosenStats{};
        for (const auto& [key, stats] : moveVotes) {
            const std::pair<int, int> decodedMove = {key >> kMovePackShift,
                                                     key & ((kOne << kMovePackShift) - kOne)};
            const std::pair<int, int> chosenMove = {chosenKey >> kMovePackShift,
                                                    chosenKey & ((kOne << kMovePackShift) - kOne)};
            if (!foundConsensus || stats.votes > chosenStats.votes ||
                (stats.votes == chosenStats.votes && stats.depth > chosenStats.depth) ||
                (stats.votes == chosenStats.votes && stats.depth == chosenStats.depth &&
                 stats.score > chosenStats.score) ||
                (stats.votes == chosenStats.votes && stats.depth == chosenStats.depth &&
                 stats.score == chosenStats.score && isPreferredMove(decodedMove, chosenMove))) {
                chosenKey = key;
                chosenStats = stats;
                foundConsensus = true;
            }
        }

        if (foundConsensus) {
            result.bestMove = {chosenKey >> kMovePackShift,
                               chosenKey & ((kOne << kMovePackShift) - kOne)};
            result.depth = chosenStats.depth;
            result.score = chosenStats.score;
        }
    }

    if (!isValidMove(result.bestMove)) {
        result.bestMove = selectFallbackRootMove(board);
        if (isValidMove(result.bestMove)) {
            result.depth = std::max(result.depth, kStartDepthBase);
            if (result.score <= kInvalidScore) {
                result.score = kZero;
            }
        }
    }

    result.depth = std::max(kZero, result.depth);
    result.score = clampScore(result.score);
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

SMPTimeManager::SMPTimeManager(int timeMs)
    : startTime(std::chrono::steady_clock::now()), allocatedTime(timeMs) {}

bool SMPTimeManager::shouldStop() const {
    return getElapsedTime() >= allocatedTime;
}

int SMPTimeManager::getElapsedTime() const {
    auto now = std::chrono::steady_clock::now();
    return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count());
}
