#include "search_internal.h"
#include "SearchTuning.h"

#include "../ai/SyzygyTablebase.h"
#include "../utils/SearchThread.h"
#include "../utils/engine_globals.h"
#include "BookUtils.h"
#include "LazySMP.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <ranges>

using namespace SearchInternal;

namespace SearchInternal {
int chooseRootSplitWorkerCount(int depth, int numThreads, int remainingMoves) {
    if (depth < kRootSplitMinDepth || remainingMoves <= kZero) {
        return kZero;
    }

    int workerCount = std::min(numThreads - kOne, remainingMoves);
    if (depth <= kRootSplitLowDepthThreshold) {
        workerCount = std::min(workerCount, kRootSplitLowDepthMaxWorkers);
    }

    return std::max(kZero, workerCount);
}

bool isPreferredRootMove(Move lhs, Move rhs) {
    if (rhs.first < kZero) {
        return true;
    }
    if (lhs.first != rhs.first) {
        return lhs.first < rhs.first;
    }
    return lhs.second < rhs.second;
}

bool checkRootSplitTimeLimit(RootSplitSharedState& shared) {
    if (shared.timeLimitMs <= kZero) {
        return false;
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - shared.startTime)
                               .count();
    if (elapsedMs >= shared.timeLimitMs) {
        shared.timeExpired.store(true, std::memory_order_relaxed);
        shared.stop.store(true, std::memory_order_relaxed);
        return true;
    }

    return false;
}

bool evaluateRootSplitMove(const RootSplitSharedState& shared, Move move, int alphaWindow,
                           int betaWindow, int& evalOut, int& nodesOut) {
    if (shared.depth <= kZero || shared.board == nullptr) {
        return false;
    }

    Board newBoard = *shared.board;
    MoveApplicationData moveData{};
    if (!applySearchMoveWithData(newBoard, move.first, move.second, true, &moveData)) {
        return false;
    }

    if (isInCheck(newBoard, shared.board->turn)) {
        return false;
    }

    const uint64_t childZobristKey =
        computeChildZobrist(shared.rootZobristKey, newBoard, move.first, move.second, moveData);
    newBoard.turn =
        (newBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    ThreadSafeHistory localHistory = shared.historySeed;
    ParallelSearchContext localContext(kOne);
    localContext.startTime = shared.startTime;
    localContext.timeLimitMs = shared.timeLimitMs;
    localContext.contempt = shared.contempt;
    localContext.multiPV = kOne;
    localContext.repetitionHistory = shared.repetitionHistory;
    localContext.pathHashes[0] = shared.rootZobristKey;
    localContext.optimalTimeMs = shared.optimalTimeMs;
    localContext.maxTimeMs = shared.maxTimeMs;
    localContext.useSyzygy = shared.useSyzygy;
    localContext.transTable.resize(std::max(kOne, shared.hashSizeMb));
    localContext.transTable.newSearch();

    evalOut = AlphaBetaSearch(newBoard, shared.depth - kOne, alphaWindow, betaWindow,
                              !shared.maximizingPlayer, kOne, localHistory, localContext,
                              childZobristKey);
    nodesOut = localContext.nodeCount;
    return true;
}

void commitRootSplitResult(RootSplitSharedState& shared, int eval, Move move) {
    std::lock_guard<std::mutex> lock(shared.bestMutex);
    if (!shared.hasBestMove ||
        (shared.maximizingPlayer ? (eval > shared.bestScore) : (eval < shared.bestScore)) ||
        (eval == shared.bestScore && isPreferredRootMove(move, shared.bestMove))) {
        shared.bestScore = eval;
        shared.bestMove = move;
        shared.hasBestMove = true;
    }

    if (shared.maximizingPlayer) {
        shared.alpha = std::max(shared.alpha, eval);
    } else {
        shared.beta = std::min(shared.beta, eval);
    }

    if (shared.alpha >= shared.beta) {
        shared.stop.store(true, std::memory_order_relaxed);
    }
}

void rootSplitWorker(RootSplitSharedState& shared) {
    while (!shared.stop.load(std::memory_order_relaxed)) {
        if (checkRootSplitTimeLimit(shared)) {
            break;
        }

        const int moveIndex = shared.nextMoveIndex.fetch_add(kOne, std::memory_order_relaxed);
        if (moveIndex < kZero || moveIndex >= static_cast<int>(shared.rootMoves->size())) {
            break;
        }

        const Move move = (*shared.rootMoves)[static_cast<std::size_t>(moveIndex)].move;
        int alphaWindow = -kMateScore;
        int betaWindow = kMateScore;
        {
            std::lock_guard<std::mutex> lock(shared.bestMutex);
            alphaWindow = shared.alpha;
            betaWindow = shared.beta;
        }

        int eval = kZero;
        int nodes = kZero;
        if (!evaluateRootSplitMove(shared, move, alphaWindow, betaWindow, eval, nodes)) {
            continue;
        }

        shared.nodes.fetch_add(nodes, std::memory_order_relaxed);
        commitRootSplitResult(shared, eval, move);
    }
}

RootSplitResult searchRootMovesYBWC(const Board& board, int depth, int alpha, int beta,
                                    bool maximizingPlayer, const ThreadSafeHistory& historyTable,
                                    const ParallelSearchContext& context, int numThreads,
                                    int hashSizeMb) {
    RootSplitResult result;
    if (numThreads <= kOne || depth <= kZero) {
        return result;
    }

    Board rootBoard = board;
    std::vector<Move> moves = GetAllMoves(rootBoard, rootBoard.turn);
    if (moves.empty()) {
        return result;
    }

    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
        rootBoard, moves, historyTable, context.killerMoves, kZero,
        {kInvalidSquare, kInvalidSquare}, {kInvalidSquare, kInvalidSquare}, &context);
    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());

    std::vector<ScoredMove> rootMoves;
    rootMoves.reserve(scoredMoves.size());
    for (const auto& scoredMove : scoredMoves) {
        bool excluded = false;
        for (const auto& excludedMove : context.excludedRootMoves) {
            if (excludedMove == scoredMove.move) {
                excluded = true;
                break;
            }
        }
        if (!excluded) {
            rootMoves.push_back(scoredMove);
        }
    }
    if (rootMoves.empty()) {
        return result;
    }
    if (static_cast<int>(rootMoves.size()) < kRootSplitMinMoves) {
        return result;
    }

    RootSplitSharedState shared;
    shared.board = &board;
    shared.rootMoves = &rootMoves;
    shared.rootZobristKey = ComputeZobrist(board);
    shared.depth = depth;
    shared.maximizingPlayer = maximizingPlayer;
    shared.hashSizeMb = std::max(kOne, hashSizeMb);
    shared.historySeed = historyTable;
    shared.repetitionHistory = context.repetitionHistory;
    shared.startTime = context.startTime;
    shared.timeLimitMs = context.timeLimitMs;
    shared.contempt = context.contempt;
    shared.optimalTimeMs = context.optimalTimeMs;
    shared.maxTimeMs = context.maxTimeMs;
    shared.useSyzygy = context.useSyzygy;
    shared.alpha = alpha;
    shared.beta = beta;
    shared.bestScore = maximizingPlayer ? -kMateScore : kMateScore;

    int seededIndex = kInvalidSquare;
    for (int i = kZero; i < static_cast<int>(rootMoves.size()); ++i) {
        int eval = kZero;
        int nodes = kZero;
        if (!evaluateRootSplitMove(shared, rootMoves[static_cast<std::size_t>(i)].move,
                                   shared.alpha, shared.beta, eval, nodes)) {
            continue;
        }

        shared.nodes.fetch_add(nodes, std::memory_order_relaxed);
        commitRootSplitResult(shared, eval, rootMoves[static_cast<std::size_t>(i)].move);
        seededIndex = i;
        break;
    }

    if (seededIndex == kInvalidSquare) {
        return result;
    }

    shared.nextMoveIndex.store(seededIndex + kOne, std::memory_order_relaxed);

    const int remainingMoves = static_cast<int>(rootMoves.size()) - (seededIndex + kOne);
    const int workerCount = chooseRootSplitWorkerCount(depth, numThreads, remainingMoves);
    if (workerCount > kZero && !shared.stop.load(std::memory_order_relaxed)) {
        std::vector<SearchThread> workers(static_cast<std::size_t>(workerCount));

        for (int i = kZero; i < workerCount; ++i) {
            const int createResult = workers[static_cast<std::size_t>(i)].start(
                [&shared]() { rootSplitWorker(shared); }, kRootSplitThreadStackBytes);
            if (createResult != 0) {
                rootSplitWorker(shared);
            }
        }

        for (auto& worker : workers) {
            worker.join();
        }
    }

    if (!shared.hasBestMove) {
        return result;
    }

    result.hasLegalMove = true;
    result.bestMove = shared.bestMove;
    result.score = shared.bestScore;
    const long long totalNodes = shared.nodes.load(std::memory_order_relaxed);
    result.nodes = (totalNodes > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                                                                  : static_cast<int>(totalNodes);
    result.timeExpired = shared.timeExpired.load(std::memory_order_relaxed);
    return result;
}
} // namespace SearchInternal

std::vector<Move> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

std::string getBookMove(const std::string& fen) {
    return lookupBookMoveString(fen, OpeningBookOptions, OpeningBook, true);
}

SearchResult iterativeDeepeningParallel(Board& board, const SearchConfig& config,
                                        SearchContext& searchContext) {
    const int maxDepth = config.maxDepth;
    const int timeLimitMs = config.timeLimitMs;
    const int contempt = config.contempt;
    const int multiPV = config.multiPV;
    const int optimalTimeMs = config.optimalTimeMs;
    const int maxTimeMs = config.maxTimeMs;
    const int numThreads = std::max(kOne, searchContext.threads);
    const int hashSizeMb = std::max(kOne, searchContext.hashSizeMb);

    if (numThreads > kOne && multiPV == kOne) {
        auto smpStart = std::chrono::steady_clock::now();
        LazySMP smp(numThreads, hashSizeMb, contempt);
        SearchResult smpResult = smp.search(board, maxDepth, timeLimitMs);
        const auto smpElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - smpStart)
                                      .count();
        smpResult.timeMs = (smpElapsedMs > std::numeric_limits<int>::max())
                               ? std::numeric_limits<int>::max()
                               : static_cast<int>(smpElapsedMs);
        return smpResult;
    }

    SearchResult result;
    auto contextPtr = std::make_unique<ParallelSearchContext>(numThreads);
    auto& context = *contextPtr;
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    context.contempt = contempt;
    context.multiPV = multiPV;
    context.repetitionHistory = config.previousPositionHashes;
    context.optimalTimeMs = optimalTimeMs;
    context.maxTimeMs = maxTimeMs;
    context.externalStop = config.externalStop;
    context.externalStopToken = config.externalStopToken;
    context.useSyzygy = !Syzygy::getPath().empty();
    context.transTable.resize(std::max(SearchConstants::kOne, hashSizeMb));
    context.transTable.newSearch();
    int lastScore = kZero;
    const int aspirationWindow = SearchTuning::aspirationWindow();
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        if (auto parsed = parseAlgebraicMove(bookMove, board)) {
            result.bestMove = {(parsed->srcRow * kBoardDimension) + parsed->srcCol,
                               (parsed->destRow * kBoardDimension) + parsed->destCol};
            result.score = kZero;
            result.depth = kOne;
            result.nodes = kOne;
            result.timeMs = kZero;
            return result;
        }
    }

    Move previousBestMove = {kInvalidSquare, kInvalidSquare};
    int bestMoveStableCount = kZero;
    int bestMoveChangeCount = kZero;
    const bool rootSplitEnabled = numThreads > kOne;
    auto getLegalRootMoves = [&]() {
        std::vector<Move> pseudoMoves = GetAllMoves(board, board.turn);
        std::vector<Move> legalMoves;
        legalMoves.reserve(pseudoMoves.size());
        for (const auto& move : pseudoMoves) {
            if (IsMoveLegal(board, move.first, move.second)) {
                legalMoves.push_back(move);
            }
        }
        return legalMoves;
    };
    auto selectRootBestMove = [&](Move candidateMove) {
        std::vector<Move> moves = getLegalRootMoves();
        if (candidateMove.first >= kZero && moveExistsInList(moves, candidateMove)) {
            return candidateMove;
        }
        uint64_t rootHash = ComputeZobrist(board);
        if (const auto rootEntry = context.transTable.find(rootHash);
            rootEntry && rootEntry->bestMove.first >= kZero &&
            moveExistsInList(moves, rootEntry->bestMove)) {
            return rootEntry->bestMove;
        }
        if (!moves.empty()) {
            std::vector<ScoredMove> scoredMoves =
                scoreMovesOptimized(board, moves, context.historyTable, context.killerMoves, kZero);
            std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
            return scoredMoves[kZero].move;
        }
        return Move{kInvalidSquare, kInvalidSquare};
    };

    for (int pvIdx = kZero; pvIdx < context.multiPV; ++pvIdx) {
        lastScore = kZero;
        context.stopSearch = false;

        for (int depth = kOne; depth <= maxDepth; depth++) {
            int alpha = -kMateScore;
            int beta = kMateScore;

            if (depth <= kEarlyDepthNoAspirationLimit) {
                alpha = -kMateScore;
                beta = kMateScore;
            } else {
                alpha = lastScore - aspirationWindow;
                beta = lastScore + aspirationWindow;
            }

            int searchScore = kZero;
            bool failedHigh = false;
            bool failedLow = false;
            int searchAttempts = kZero;
            const int maxSearchAttempts = kMaxSearchAttempts;
            const uint64_t rootZobristKey = ComputeZobrist(board);
            Move depthBestMove = {kInvalidSquare, kInvalidSquare};

            do {
                const bool shouldUseRootSplitAttempt =
                    rootSplitEnabled && depth >= kRootSplitMinDepth &&
                    searchAttempts <= kRootSplitMaxAspirationSplitAttempt;
                if (shouldUseRootSplitAttempt) {
                    RootSplitResult rootSplitResult = searchRootMovesYBWC(
                        board, depth, alpha, beta, board.turn == ChessPieceColor::WHITE,
                        context.historyTable, context, numThreads, hashSizeMb);
                    if (rootSplitResult.hasLegalMove) {
                        searchScore = rootSplitResult.score;
                        depthBestMove = rootSplitResult.bestMove;
                        context.nodeCount += rootSplitResult.nodes;
                        if (rootSplitResult.timeExpired) {
                            context.stopSearch = true;
                        }
                    } else {
                        searchScore = PrincipalVariationSearch(
                            board, depth, alpha, beta, board.turn == ChessPieceColor::WHITE, kZero,
                            context.historyTable, context, true, rootZobristKey);
                    }
                } else {
                    searchScore = PrincipalVariationSearch(
                        board, depth, alpha, beta, board.turn == ChessPieceColor::WHITE, kZero,
                        context.historyTable, context, true, rootZobristKey);
                }
                searchAttempts++;

                if (context.stopSearch) {
                    break;
                }

                if (searchScore <= alpha) {
                    alpha = alpha - (aspirationWindow * (kOne << searchAttempts));
                    failedLow = true;
                    alpha = std::max(alpha, -kMateScore);
                } else if (searchScore >= beta) {
                    beta = beta + (aspirationWindow * (kOne << searchAttempts));
                    failedHigh = true;
                    beta = std::min(beta, kMateScore);
                } else {
                    break;
                }

                if (searchAttempts >= maxSearchAttempts) {
                    alpha = -kMateScore;
                    beta = kMateScore;
                }

            } while ((failedHigh || failedLow) && searchAttempts < maxSearchAttempts &&
                     !context.stopSearch);

            if (context.stopSearch) {
                break;
            }

            lastScore = searchScore;
            result.score = searchScore;
            result.depth = depth;
            result.nodes = context.nodeCount;
            result.bestMove = selectRootBestMove(depthBestMove);

            if (pvIdx == kZero) {
                if (result.bestMove == previousBestMove) {
                    bestMoveStableCount++;
                } else {
                    bestMoveChangeCount++;
                    bestMoveStableCount = kZero;
                }
                previousBestMove = result.bestMove;
            }

            if (context.multiPV > kOne) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - context.startTime)
                                   .count();
                const auto nodesPerSecond =
                    elapsed > kZero
                        ? ((static_cast<long long>(result.nodes) * kNpsMillisScaleLl) / elapsed)
                        : 0LL;
                int nps = static_cast<int>(nodesPerSecond);
                std::cout << "info depth " << depth << " multipv " << (pvIdx + kOne) << " score cp "
                          << result.score << " nodes " << result.nodes << " nps " << nps
                          << " hashfull " << context.transTable.hashfull();
                if (context.tbHits > kZero) {
                    std::cout << " tbhits " << context.tbHits;
                }
                std::cout << '\n';
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - context.startTime)
                               .count();

            int optTime = context.optimalTimeMs > kZero ? context.optimalTimeMs : timeLimitMs;
            int maxTime = context.maxTimeMs > kZero ? context.maxTimeMs : timeLimitMs;
            double stabilityFactor = 1.0;
            if (bestMoveChangeCount >= kStabilityChangeThreshold &&
                bestMoveStableCount < kStabilityLowThreshold) {
                stabilityFactor = kUnstableStabilityFactor;
            } else if (bestMoveStableCount >= kStabilityHighThreshold) {
                stabilityFactor = kStableStabilityFactor;
            }

            double scoreFactor = 1.0;
            if (depth >= kScoreInstabilityDepth && pvIdx == kZero) {
                if (lastScore < result.score - kScoreRiseThreshold) {
                    scoreFactor = kScoreRiseFactor;
                } else if (lastScore > result.score + kScoreDropThreshold) {
                    scoreFactor = kScoreDropFactor;
                }
            }

            int softLimit = static_cast<int>(optTime * stabilityFactor * scoreFactor);
            bool timeToStop =
                (elapsed >= maxTime) || (depth >= kSoftLimitDepthThreshold && elapsed >= softLimit);

            if (timeToStop) {
                break;
            }
        }

        result.bestMove = selectRootBestMove(result.bestMove);

        if (pvIdx < context.multiPV - kOne && result.bestMove.first >= kZero) {
            context.excludedRootMoves.push_back(result.bestMove);
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - context.startTime).count();
    result.timeMs = (elapsedMs > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                                                                  : static_cast<int>(elapsedMs);

    return result;
}
