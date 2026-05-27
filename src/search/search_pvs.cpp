#include "SearchTuning.h"
#include "search_internal.h"

#include "../ai/SyzygyTablebase.h"

using namespace SearchInternal;

void SearchInternal::updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare,
                                        int toSquare, int depth) {
    int bonus = depth * depth;
    historyTable.update(fromSquare, toSquare, bonus);
}

namespace SearchInternal {
void penalizeHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare,
                          int depth) {
    int penalty = -(depth * depth) / kTwo;
    historyTable.update(fromSquare, toSquare, penalty);
}
} // namespace SearchInternal

bool SearchInternal::isTimeUp(const std::chrono::steady_clock::time_point& startTime,
                              int timeLimitMs) {
    if (timeLimitMs <= kZero) {
        return false;
    }

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

namespace SearchInternal {
bool countNodeAndCheckTime(ParallelSearchContext& context) {
    if (context.stopSearch) {
        return false;
    }

    if (context.externalStop != nullptr && context.externalStop->load()) {
        context.stopSearch = true;
        return false;
    }

    if (context.externalStopToken != nullptr && context.externalStopToken->stop_requested()) {
        context.stopSearch = true;
        return false;
    }

    ++context.nodeCount;
    if ((context.nodeCount & kTimeCheckMask) != kZero) {
        return true;
    }

    if (isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch = true;
        return false;
    }

    return true;
}

bool isRepetitionDraw(const Board& board, uint64_t nodeZobristKey,
                      const ParallelSearchContext& context, int ply) {
    if (board.halfmoveClock <= 0) {
        return false;
    }

    int priorMatches = 0;
    int remainingPlies = board.halfmoveClock;
    for (int i = static_cast<int>(context.repetitionHistory.size()) - 1;
         i >= 0 && remainingPlies > 0; --i, --remainingPlies) {
        if (context.repetitionHistory[static_cast<std::size_t>(i)] == nodeZobristKey) {
            ++priorMatches;
            if (priorMatches >= 2) {
                return true;
            }
        }
    }

    for (int i = ply - 1; i >= 0 && remainingPlies > 0; --i, --remainingPlies) {
        if (context.pathHashes[static_cast<std::size_t>(i)] == nodeZobristKey) {
            ++priorMatches;
            if (priorMatches >= 2) {
                return true;
            }
        }
    }

    return false;
}
} // namespace SearchInternal

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply,
                     uint64_t zobristKey) {
    if (ply < kZero || ply >= kMaxSearchPly) {
        ScopedFastEvalMode fastEvalScope(true);
        return evaluatePosition(board, context.contempt);
    }

    if (board.halfmoveClock >= 100) {
        return -context.contempt;
    }

    if (!countNodeAndCheckTime(context)) {
        return kZero;
    }

    const int originalAlpha = alpha;
    const int originalBeta = beta;

    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    if (isRepetitionDraw(board, nodeZobristKey, context, ply)) {
        return -context.contempt;
    }
    context.pathHashes[static_cast<std::size_t>(ply)] = nodeZobristKey;
    context.transTable.prefetch(nodeZobristKey);
    Move hashMove = {kInvalidSquare, kInvalidSquare};
    if (auto ttEntry = context.transTable.find(nodeZobristKey)) {
        int ttValue = fromTtScore(ttEntry->value, ply);
        ttEntry->value = ttValue;
        if (ttEntry->depth >= kZero) {
            if (ttEntry->flag == kExactFlag) {
                return ttValue;
            }
            if (ttEntry->flag == kUpperBoundFlag && ttValue <= alpha) {
                return ttValue;
            }
            if (ttEntry->flag == kLowerBoundFlag && ttValue >= beta) {
                return ttValue;
            }
        }
        hashMove = ttEntry->bestMove;
    }

    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    bool inCheck = isInCheck(board, currentColor);
    std::vector<Move> moves = inCheck ? GetAllMoves(board, currentColor)
                                      : generateBitboardCaptureMoves(board, currentColor);

    if (inCheck && moves.empty()) {
        int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
        storeTtEntry(context, nodeZobristKey, kZero, mateScore, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return mateScore;
    }

    const bool useFastEval = !inCheck && ply > kFastEvalQuiescencePlyThreshold;
    ScopedFastEvalMode fastEvalScope(useFastEval);
    int standPat = evaluatePosition(board, context.contempt);

    if (inCheck) {
        standPat = maximizingPlayer ? -kMateScore : kMateScore;
    }

    if (maximizingPlayer) {
        if (standPat >= beta) {
            storeTtEntry(context, nodeZobristKey, kZero, standPat, kLowerBoundFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return standPat;
        }
        alpha = std::max(alpha, standPat);
    } else {
        if (standPat <= alpha) {
            storeTtEntry(context, nodeZobristKey, kZero, standPat, kUpperBoundFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return standPat;
        }
        beta = std::min(beta, standPat);
    }

    if (!inCheck) {
        if (maximizingPlayer) {
            if (standPat + kQueenValue < alpha) {
                return alpha;
            }
            if (standPat + kBigDeltaMargin < alpha) {
                return standPat;
            }
        } else {
            if (standPat - kQueenValue > beta) {
                return beta;
            }
            if (standPat - kBigDeltaMargin > beta) {
                return standPat;
            }
        }
    }

    if (!inCheck && moves.empty()) {
        if (!hasAnyLegalMove(board, currentColor)) {
            int drawScore = -context.contempt;
            storeTtEntry(context, nodeZobristKey, kZero, drawScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return drawScore;
        }
        storeTtEntry(context, nodeZobristKey, kZero, standPat, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return standPat;
    }

    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());
    for (const auto& move : moves) {
        int score = (move == hashMove) ? EnhancedMoveOrdering::HASH_MOVE_SCORE : kZero;
        bool isCapt = isCapture(board, move.first, move.second);

        if (isCapt) {
            int victimValue = getPieceValue(board.squares[move.second].piece.PieceType);
            int attackerValue = getPieceValue(board.squares[move.first].piece.PieceType);
            score = (victimValue * kCaptureVictimScale) - attackerValue;
            int seeValue = staticExchangeEvaluation(board, move.first, move.second);
            int seeThreshold = kSeeThresholdBase - (ply * kSeeThresholdPerPly);
            if (seeValue < seeThreshold) {
                continue;
            }

            int expectedGain = std::max(seeValue, victimValue);
            if (maximizingPlayer && standPat + expectedGain + kDeltaPruneMargin < alpha) {
                continue;
            }
            if (!maximizingPlayer && standPat - expectedGain - kDeltaPruneMargin > beta) {
                continue;
            }

            score += seeValue;

            if (seeValue > victimValue) {
                score += kSeeCaptureBonus;
            }

            if (seeValue >= kZero) {
                score += kWinningSeeBonus;
            }
        } else {
            score += historyTable.get(move.first, move.second);
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
        }

        scoredMoves.emplace_back(move, score);
    }

    int bestValue = standPat;
    Move bestMove = {kInvalidSquare, kInvalidSquare};

    for (std::size_t scoredIndex = 0; scoredIndex < scoredMoves.size(); ++scoredIndex) {
        if (context.stopSearch) {
            return kZero;
        }

        const Move move = pickNextScoredMove(scoredMoves, scoredIndex).move;
        MoveApplicationData moveData{};
        if (!applySearchMoveWithData(board, move.first, move.second, true, &moveData)) {
            continue;
        }
        uint64_t childZobristKey =
            computeChildZobrist(nodeZobristKey, board, move.first, move.second, moveData);

        if (isInCheck(board, currentColor)) {
            undoSearchMoveWithData(board, moveData);
            continue;
        }
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;

        int eval = QuiescenceSearch(board, alpha, beta, !maximizingPlayer, historyTable, context,
                                    ply + kOne, childZobristKey);
        undoSearchMoveWithData(board, moveData);
        if (context.stopSearch) {
            return kZero;
        }

        if (maximizingPlayer) {
            if (eval > bestValue) {
                bestValue = eval;
                bestMove = move;
            }
            alpha = std::max(alpha, eval);
        } else {
            if (eval < bestValue) {
                bestValue = eval;
                bestMove = move;
            }
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            break;
        }
    }

    int flag = kExactFlag;
    if (bestValue <= originalAlpha) {
        flag = kUpperBoundFlag;
    } else if (bestValue >= originalBeta) {
        flag = kLowerBoundFlag;
    }
    storeTtEntry(context, nodeZobristKey, kZero, bestValue, flag, bestMove, ply);

    return bestValue;
}

int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer,
                             int ply, ThreadSafeHistory& historyTable,
                             ParallelSearchContext& context, bool isPVNode, uint64_t zobristKey) {
    if (depth < kZero || depth > kMaxPvDepth) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        return kZero;
    }

    if (board.halfmoveClock >= 100) {
        return -context.contempt;
    }

    if (depth == kZero) {
        return QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply,
                                resolveZobristKey(board, zobristKey));
    }

    if (!countNodeAndCheckTime(context)) {
        return kZero;
    }
    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    if (isRepetitionDraw(board, nodeZobristKey, context, ply)) {
        return -context.contempt;
    }
    context.pathHashes[static_cast<std::size_t>(ply)] = nodeZobristKey;
    context.transTable.prefetch(nodeZobristKey);
    auto ttEntry = context.transTable.find(nodeZobristKey);
    bool ttHit = ttEntry.has_value();
    if (ttHit) {
        ttEntry->value = fromTtScore(ttEntry->value, ply);
    }
    if (ttHit && ttEntry->depth >= depth) {
        if (ttEntry->flag == kExactFlag) {
            return ttEntry->value;
        }
        if (ttEntry->flag == kUpperBoundFlag && ttEntry->value <= alpha) {
            return ttEntry->value;
        }
        if (ttEntry->flag == kLowerBoundFlag && ttEntry->value >= beta) {
            return ttEntry->value;
        }
    }

    if (context.useSyzygy && !isPVNode && ply > kZero && Syzygy::canProbe(board)) {
        int success = kZero;
        Syzygy::ProbeResult wdl = Syzygy::probeWDL(board, &success);
        if (success) {
            context.tbHits++;
            int tbScore = kZero;
            int tbBound = kZero;
            if (wdl == Syzygy::PROBE_WIN || wdl == Syzygy::PROBE_CURSED_WIN) {
                tbScore = kSyzygyWinScore - ply;
                tbBound = kLowerBoundFlag;
            } else if (wdl == Syzygy::PROBE_LOSS || wdl == Syzygy::PROBE_BLESSED_LOSS) {
                tbScore = -kSyzygyWinScore + ply;
                tbBound = kUpperBoundFlag;
            } else {
                tbScore = kZero;
                tbBound = kZero;
            }
            if (tbBound == kZero || (tbBound == kLowerBoundFlag && tbScore >= beta) ||
                (tbBound == kUpperBoundFlag && tbScore <= alpha)) {
                storeTtEntry(context, nodeZobristKey, depth, tbScore, tbBound,
                             {kInvalidSquare, kInvalidSquare}, ply);
                return tbScore;
            }
        }
    }

    if (depth >= kLateDepthReductionThreshold && (!ttHit || ttEntry->bestMove.first < kZero)) {
        depth -= kOne;
    }
    TTEntry ttData = ttEntry.value_or(TTEntry{});

    ScopedFastEvalMode fastEvalScope(!isPVNode && depth <= kFastEvalDepthThreshold);
    int staticEval = evaluatePosition(board, context.contempt);
    int gamePhase = computeGamePhase(board);

    if (!isPVNode) {
        if (AdvancedSearch::staticNullMovePruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::futilityPruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::nullMovePruning(board, depth, alpha, beta) &&
            hasNonPawnMaterial(board, board.turn)) {
            int sideEval = staticEval;
            if (!maximizingPlayer) {
                sideEval = -sideEval;
            }
            int reduction = computeNullMoveReduction(depth, sideEval - beta);
            int nullDepth = std::max(kOne, depth - reduction);
            const chess::EnPassantSquare previousEnPassant = board.enPassantSquare;
            uint64_t nullZobristKey = nodeZobristKey ^ ZobristBlackToMove;
            if (const auto enPassant = previousEnPassant.target()) {
                nullZobristKey ^= ZobristEnPassant[*enPassant];
            }
            board.enPassantSquare = std::nullopt;
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
            int nullScore = kZero;
            if (maximizingPlayer) {
                nullScore = PrincipalVariationSearch(board, nullDepth, beta - kZeroWindowOffset,
                                                     beta, !maximizingPlayer, ply + kOne,
                                                     historyTable, context, false, nullZobristKey);
            } else {
                nullScore = PrincipalVariationSearch(
                    board, nullDepth, alpha, alpha + kZeroWindowOffset, !maximizingPlayer,
                    ply + kOne, historyTable, context, false, nullZobristKey);
            }
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
            board.enPassantSquare = previousEnPassant;

            if (maximizingPlayer) {
                if (nullScore >= beta) {
                    return beta;
                }
            } else {
                if (nullScore <= alpha) {
                    return alpha;
                }
            }
        }

        if (AdvancedSearch::multiCutPruning(board, depth, alpha, beta, kMultiCutReductionMoves)) {
            return beta;
        }
    }

    MovePicker picker(board, ttData.bestMove, context.killerMoves, ply, historyTable, context);
    int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int prevDest = board.LastMove;
    const bool sideInCheck = isInCheck(board, board.turn);
    const int originalAlpha = alpha;
    const int originalBeta = beta;
    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    Move bestMove = {kInvalidSquare, kInvalidSquare};
    int flag = kExactFlag;
    int movesSearched = kZero;
    std::vector<Move> quietMovesSearched;
    quietMovesSearched.reserve(16);
    Move move;
    while ((move = picker.next()).first >= kZero) {
        if (ply == kZero) {
            bool excluded = false;
            for (const auto& ex : context.excludedRootMoves) {
                if (ex == move) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) {
                continue;
            }
        }

        bool isCaptureMove = isCapture(board, move.first, move.second);

        if (!isPVNode && movesSearched > kZero) {
            if (depth <= kSeePruningDepthLimit) {
                int seeVal = staticExchangeEvaluation(board, move.first, move.second);
                if (seeVal < -depth * kSeePruningScale) {
                    continue;
                }
            }

            if (AdvancedSearch::historyPruning(board, depth, move, historyTable)) {
                continue;
            }

            if (AdvancedSearch::lateMovePruning(board, depth, movesSearched, sideInCheck)) {
                continue;
            }
        }

        const ChessPieceColor movingColor = board.turn;
        const bool givesCheckMove = givesCheck(board, move.first, move.second);
        const bool isPromotionMove = isPromotion(board, move.first, move.second);
        const bool isCastlingMove = isCastling(board, move.first, move.second);
        const bool basicExtension = givesCheckMove ||
                                    AdvancedSearch::recaptureExtension(board, move, depth) ||
                                    AdvancedSearch::pawnPushExtension(board, move, depth) ||
                                    AdvancedSearch::passedPawnExtension(board, move, depth);
        int extension = kZero;
        if (basicExtension) {
            extension = kOne;
        } else if (depth >= kSingularDepthThreshold && move == ttData.bestMove &&
                   ttData.depth >= depth - kSingularTtDepthMargin && ttData.value > alpha &&
                   ttData.value < beta) {
            int seMargin = kSingularMarginPerDepth * depth;
            int seBeta = ttData.value - seMargin;
            Board seBoard = board;
            int seVal = PrincipalVariationSearch(
                seBoard, depth / kSingularReducedDepthDivisor, seBeta - kZeroWindowOffset, seBeta,
                maximizingPlayer, ply + kOne, historyTable, context, false);
            if (seVal < seBeta) {
                extension = kOne;
            } else if (seVal >= beta) {
                return seVal;
            }
        }

        if (ply >= kExtensionPlyLimit) {
            extension = kZero;
        }

        MoveApplicationData moveData{};
        if (!applySearchMoveWithData(board, move.first, move.second, true, &moveData)) {
            continue;
        }
        uint64_t childZobristKey =
            computeChildZobrist(nodeZobristKey, board, move.first, move.second, moveData);
        isCaptureMove = moveData.captureSquare >= kZero;
        if (isInCheck(board, movingColor)) {
            undoSearchMoveWithData(board, moveData);
            continue;
        }

        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;

        int searchDepth = std::max(kZero, depth - kOne + extension);

        if (movesSearched >= kLateMoveCountThreshold && searchDepth >= kLateMoveCountThreshold) {
            LMREnhanced::MoveClassification mc;
            mc.isCapture = isCaptureMove;
            mc.givesCheck = givesCheckMove;
            mc.isKiller = context.killerMoves.isKiller(ply, move);
            mc.isHashMove = (move == ttData.bestMove);
            mc.isCounter = (prevDest >= kZero && prevDest < kCounterMoveArrayLimit &&
                            context.counterMoves[colorIdx][prevDest] == move);
            mc.isPromotion = isPromotionMove;
            mc.isCastling = isCastlingMove;
            mc.historyScore = historyTable.get(move.first, move.second);
            mc.moveNumber = movesSearched + kOne;
            LMREnhanced::PositionContext pc;
            pc.inCheck = sideInCheck;
            pc.isPVNode = isPVNode;
            pc.isEndgame = (gamePhase < kLmrEndgamePhaseThreshold);
            pc.gamePhase = gamePhase;
            pc.staticEval = staticEval;
            int reduction = LMREnhanced::calculateReduction(searchDepth, mc, pc);
            if (reduction > kZero) {
                searchDepth = std::max(kOne, searchDepth - reduction);
            }
        }

        int score = kZero;
        if (maximizingPlayer) {
            if (movesSearched == kZero) {
                score = PrincipalVariationSearch(board, searchDepth, alpha, beta, !maximizingPlayer,
                                                 ply + kOne, historyTable, context, isPVNode,
                                                 childZobristKey);
            } else {
                score = PrincipalVariationSearch(
                    board, searchDepth, alpha, alpha + kZeroWindowOffset, !maximizingPlayer,
                    ply + kOne, historyTable, context, false, childZobristKey);
                if (score > alpha && score < beta) {
                    score = PrincipalVariationSearch(board, searchDepth, alpha, beta,
                                                     !maximizingPlayer, ply + kOne, historyTable,
                                                     context, isPVNode, childZobristKey);
                }
            }
        } else {
            if (movesSearched == kZero) {
                score = PrincipalVariationSearch(board, searchDepth, alpha, beta, !maximizingPlayer,
                                                 ply + kOne, historyTable, context, isPVNode,
                                                 childZobristKey);
            } else {
                score = PrincipalVariationSearch(board, searchDepth, beta - kZeroWindowOffset, beta,
                                                 !maximizingPlayer, ply + kOne, historyTable,
                                                 context, false, childZobristKey);
                if (score < beta && score > alpha) {
                    score = PrincipalVariationSearch(board, searchDepth, alpha, beta,
                                                     !maximizingPlayer, ply + kOne, historyTable,
                                                     context, isPVNode, childZobristKey);
                }
            }
        }

        undoSearchMoveWithData(board, moveData);

        movesSearched++;
        if (!isCaptureMove) {
            quietMovesSearched.push_back(move);
        }

        if (maximizingPlayer) {
            if (score > bestValue) {
                bestValue = std::max(bestValue, score);
                bestMove = move;
                alpha = std::max(alpha, score);
            }
        } else {
            if (score < bestValue) {
                bestValue = std::min(bestValue, score);
                bestMove = move;
                beta = std::min(beta, score);
            }
        }

        if (alpha >= beta) {
            if (!isCaptureMove) {
                context.killerMoves.store(ply, move);
                if (prevDest >= kZero && prevDest < kCounterMoveArrayLimit) {
                    context.counterMoves[colorIdx][prevDest] = move;
                }
                updateHistoryTable(historyTable, move.first, move.second, depth);
                for (const auto& quietMove : quietMovesSearched) {
                    if (quietMove != move) {
                        penalizeHistoryTable(historyTable, quietMove.first, quietMove.second,
                                             depth);
                    }
                }
            }
            break;
        }
    }

    if (movesSearched == kZero) {
        if (sideInCheck) {
            int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
            storeTtEntry(context, nodeZobristKey, depth, mateScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return mateScore;
        }
        int drawScore = -context.contempt;
        storeTtEntry(context, nodeZobristKey, depth, drawScore, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return drawScore;
    }

    if (bestValue <= originalAlpha) {
        flag = kUpperBoundFlag;
    } else if (bestValue >= originalBeta) {
        flag = kLowerBoundFlag;
    }

    storeTtEntry(context, nodeZobristKey, depth, bestValue, flag, bestMove, ply);
    return bestValue;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply,
                    ThreadSafeHistory& historyTable, ParallelSearchContext& context,
                    uint64_t zobristKey) {
    if (depth < kZero || depth > kMaxPvDepth) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        return kZero;
    }

    if (board.halfmoveClock >= 100) {
        return -context.contempt;
    }

    if (!countNodeAndCheckTime(context)) {
        return kZero;
    }
    int extension = kZero;
    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;

    if (depth == kOne && ply < kLmrEndgamePhaseThreshold && isInCheck(board, currentColor)) {
        extension = kOne;
    } else if (depth >= kSingularDepthThreshold && ply < kLmrEndgamePhaseThreshold) {
        uint64_t hash = resolveZobristKey(board, zobristKey);
        if (auto ttEntry = context.transTable.find(hash);
            ttEntry && ttEntry->depth >= depth - kSingularTtDepthMargin) {
            ttEntry->value = fromTtScore(ttEntry->value, ply);
            if (ttEntry->bestMove.first != kInvalidSquare && ttEntry->value > alpha &&
                ttEntry->value < beta) {
                int singularMargin = kSingularMarginPerDepth * depth;
                int singularBeta = ttEntry->value - singularMargin;
                if (singularBeta > alpha) {
                    Board testBoard = board;
                    int singularValue =
                        AlphaBetaSearch(testBoard, depth / kSingularReducedDepthDivisor,
                                        singularBeta - kZeroWindowOffset, singularBeta,
                                        maximizingPlayer, ply + kOne, historyTable, context);

                    if (singularValue < singularBeta) {
                        extension = kOne;
                    } else if (singularValue >= beta) {
                        return singularValue;
                    }
                }
            }
        }
    }

    depth += extension;

    if (depth > kMaxPvDepth || ply >= kMaxSearchPly) {
        return kZero;
    }

    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    if (isRepetitionDraw(board, nodeZobristKey, context, ply)) {
        return -context.contempt;
    }
    context.pathHashes[static_cast<std::size_t>(ply)] = nodeZobristKey;
    context.transTable.prefetch(nodeZobristKey);
    Move hashMove = {kInvalidSquare, kInvalidSquare};
    if (auto entry = context.transTable.find(nodeZobristKey)) {
        entry->value = fromTtScore(entry->value, ply);
        if (entry->depth >= depth) {
            if (entry->flag == kExactFlag) {
                return entry->value;
            }
            if (entry->flag == kUpperBoundFlag && entry->value <= alpha) {
                return entry->value;
            }
            if (entry->flag == kLowerBoundFlag && entry->value >= beta) {
                return entry->value;
            }
        }

        if (entry->bestMove.first != kInvalidSquare && entry->bestMove.second != kInvalidSquare) {
            hashMove = entry->bestMove;
        }
    }

    if (context.useSyzygy && ply > kZero && Syzygy::canProbe(board)) {
        int success = kZero;
        Syzygy::ProbeResult wdl = Syzygy::probeWDL(board, &success);
        if (success) {
            context.tbHits++;
            int tbScore = kZero;
            if (wdl == Syzygy::PROBE_WIN || wdl == Syzygy::PROBE_CURSED_WIN) {
                tbScore = kSyzygyWinScore - ply;
            } else if (wdl == Syzygy::PROBE_LOSS || wdl == Syzygy::PROBE_BLESSED_LOSS) {
                tbScore = -kSyzygyWinScore + ply;
            }
            if (!maximizingPlayer) {
                tbScore = -tbScore;
            }
            storeTtEntry(context, nodeZobristKey, depth, tbScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return tbScore;
        }
    }

    if (depth >= kLateDepthReductionThreshold && hashMove.first < kZero) {
        depth -= kOne;
    }

    if (depth == kZero) {
        int eval = QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context,
                                    ply, nodeZobristKey);
        storeTtEntry(context, nodeZobristKey, depth, eval, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return eval;
    }
    std::vector<Move> moves =
        GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    if (moves.empty()) {
        if (isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
            int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
            storeTtEntry(context, nodeZobristKey, depth, mateScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return mateScore;
        }
        int drawScore = -context.contempt;
        storeTtEntry(context, nodeZobristKey, depth, drawScore, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return drawScore;
    }

    ScopedFastEvalMode fastEvalScope(depth <= kFastEvalDepthThreshold);
    const bool sideInCheck = isInCheck(board, currentColor);
    int staticEval = evaluatePosition(board, context.contempt);

    if (depth >= kNullMoveMinDepth && depth <= kNullMoveMaxDepth && ply < kNullMovePlyLimit &&
        !sideInCheck && hasNonPawnMaterial(board, currentColor)) {
        const ChessPieceColor previousTurn = board.turn;
        const chess::EnPassantSquare previousEnPassant = board.enPassantSquare;
        board.enPassantSquare = std::nullopt;
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;
        uint64_t nullZobristKey = nodeZobristKey ^ ZobristBlackToMove;
        if (const auto enPassant = previousEnPassant.target()) {
            nullZobristKey ^= ZobristEnPassant[*enPassant];
        }
        int evalMargin = maximizingPlayer ? (staticEval - beta) : (alpha - staticEval);
        int reducedDepth = std::max(kOne, depth - computeNullMoveReduction(depth, evalMargin));
        if (reducedDepth > kZero && reducedDepth <= kNullMoveMaxDepth) {
            int nullValue =
                maximizingPlayer
                    ? AlphaBetaSearch(board, reducedDepth, beta - kOne, beta, !maximizingPlayer,
                                      ply + kOne, historyTable, context, nullZobristKey)
                    : AlphaBetaSearch(board, reducedDepth, alpha, alpha + kOne, !maximizingPlayer,
                                      ply + kOne, historyTable, context, nullZobristKey);
            board.turn = previousTurn;
            board.enPassantSquare = previousEnPassant;
            if (context.stopSearch) {
                return kZero;
            }
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    return beta;
                }
            } else {
                if (nullValue <= alpha) {
                    return alpha;
                }
            }
        }
        board.turn = previousTurn;
        board.enPassantSquare = previousEnPassant;
    }
    int abColorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int abPrevDest = board.LastMove;
    Move abCounterMove = {kInvalidSquare, kInvalidSquare};
    if (abPrevDest >= kZero && abPrevDest < kCounterMoveArrayLimit) {
        abCounterMove = context.counterMoves[abColorIdx][abPrevDest];
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
        board, moves, historyTable, context.killerMoves, ply, hashMove, abCounterMove, &context);
    int origAlpha = alpha;
    int origBeta = beta;
    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    int flag = kExactFlag;
    bool foundPV = false;
    Move bestMoveFound = {kInvalidSquare, kInvalidSquare};
    std::vector<Move> quietMovesSearched;
    quietMovesSearched.reserve(16);

    if (maximizingPlayer) {
        int moveCount = kZero;
        for (std::size_t scoredIndex = 0; scoredIndex < scoredMoves.size(); ++scoredIndex) {
            if (context.stopSearch) {
                return kZero;
            }
            const Move move = pickNextScoredMove(scoredMoves, scoredIndex).move;
            const int movingPiece = pieceTypeIndex(board.squares[move.first].piece.PieceType);
            const bool isCaptureMove = isCapture(board, move.first, move.second);
            const bool isCheckMove = givesCheck(board, move.first, move.second);
            const int histScore = historyTable.get(move.first, move.second);

            if (depth <= kFutilityDepthLimit && !foundPV && !isCaptureMove && !isCheckMove &&
                !sideInCheck) {
                const int futilityValueMargin = SearchTuning::futilityMargin(depth);
                int futilityValue = staticEval + futilityValueMargin;

                if (futilityValue <= alpha) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kSeePruningDepthLimit && isCaptureMove && !isCheckMove && !sideInCheck) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                if (seeValue < -depth * kSeePruningScale) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kHistoryPruningDepthLimit && !isCaptureMove && !isCheckMove &&
                !sideInCheck && !context.killerMoves.isKiller(ply, move) &&
                moveCount > kQuietHistoryPruningMoveThreshold) {
                if (histScore < kHistoryPruningBaseThreshold * depth) {
                    moveCount++;
                    continue;
                }
            }

            MoveApplicationData moveData{};
            if (!applySearchMoveWithData(board, move.first, move.second, true, &moveData)) {
                continue;
            }
            uint64_t childZobristKey =
                computeChildZobrist(nodeZobristKey, board, move.first, move.second, moveData);

            if (isInCheck(board, currentColor)) {
                undoSearchMoveWithData(board, moveData);
                continue;
            }
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

            int eval = kZero;

            if (ply + kOne < kContinuationPlyLimit) {
                context.prevPieceAtPly[ply + kOne] = movingPiece;
                context.prevToAtPly[ply + kOne] = move.second;
            }

            int contHistScore = kZero;
            if (ply > kZero) {
                int pp = context.prevPieceAtPly[ply];
                int pt = context.prevToAtPly[ply];
                contHistScore = context.getContinuationHistory(pp, pt, movingPiece, move.second);
            }

            if (moveCount == kZero || !foundPV) {
                eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, false, ply + kOne,
                                       historyTable, context, childZobristKey);
            } else {
                LMREnhanced::MoveClassification mc;
                mc.isCapture = isCaptureMove;
                mc.givesCheck = isCheckMove;
                mc.isKiller = context.killerMoves.isKiller(ply, move);
                mc.isHashMove = (move == hashMove);
                mc.isCounter = (abCounterMove.first >= kZero && move == abCounterMove);
                mc.historyScore = histScore + contHistScore;
                mc.moveNumber = moveCount + kOne;
                LMREnhanced::PositionContext pc;
                pc.inCheck = sideInCheck;
                pc.isPVNode = false;
                pc.isEndgame = false;
                pc.gamePhase = kZero;
                pc.staticEval = staticEval;
                int reduction = LMREnhanced::calculateReduction(depth, mc, pc);
                if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                    int reducedDepth = std::max(kZero, depth - kOne - reduction);
                    int reducedEval =
                        AlphaBetaSearch(board, reducedDepth, alpha, alpha + kOne, false, ply + kOne,
                                        historyTable, context, childZobristKey);

                    if (reducedEval > alpha) {
                        eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, false, ply + kOne,
                                               historyTable, context, childZobristKey);
                    } else {
                        eval = reducedEval;
                    }
                } else {
                    eval = AlphaBetaSearch(board, depth - kOne, alpha, alpha + kOne, false,
                                           ply + kOne, historyTable, context, childZobristKey);

                    if (eval > alpha && eval < beta) {
                        eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, false, ply + kOne,
                                               historyTable, context, childZobristKey);
                    }
                }
            }

            undoSearchMoveWithData(board, moveData);

            moveCount++;
            if (context.stopSearch) {
                return kZero;
            }
            if (!isCaptureMove) {
                quietMovesSearched.push_back(move);
            }
            if (eval > bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval > alpha) {
                alpha = eval;
                foundPV = true;
            }
            if (alpha >= beta) {
                int bonus = depth * depth;
                if (!isCaptureMove) {
                    updateHistoryTable(historyTable, move.first, move.second, depth);
                    for (const auto& quietMove : quietMovesSearched) {
                        if (quietMove != move) {
                            penalizeHistoryTable(historyTable, quietMove.first, quietMove.second,
                                                 depth);
                        }
                    }
                    context.killerMoves.store(ply, move);
                    if (abPrevDest >= kZero && abPrevDest < kCounterMoveArrayLimit) {
                        context.counterMoves[abColorIdx][abPrevDest] = move;
                    }
                    if (ply > kZero) {
                        int pp = context.prevPieceAtPly[ply];
                        int pt = context.prevToAtPly[ply];
                        context.updateContinuationHistory(pp, pt, movingPiece, move.second, bonus);
                    }
                } else {
                    int victimPiece = pieceTypeIndex(moveData.capturedPiece.PieceType);
                    context.updateCaptureHistory(movingPiece, victimPiece, move.second, bonus);
                }
                break;
            }
        }
        if (bestValue >= origBeta) {
            flag = kLowerBoundFlag;
        } else if (bestValue <= origAlpha) {
            flag = kUpperBoundFlag;
        } else {
            flag = kExactFlag;
        }
    } else {
        int moveCount = kZero;
        for (std::size_t scoredIndex = 0; scoredIndex < scoredMoves.size(); ++scoredIndex) {
            if (context.stopSearch) {
                return kZero;
            }
            const Move move = pickNextScoredMove(scoredMoves, scoredIndex).move;
            const int movingPiece = pieceTypeIndex(board.squares[move.first].piece.PieceType);
            const bool isCaptureMove = isCapture(board, move.first, move.second);
            const bool isCheckMove = givesCheck(board, move.first, move.second);
            const int histScore = historyTable.get(move.first, move.second);

            if (depth <= kFutilityDepthLimit && !foundPV && !isCaptureMove && !isCheckMove &&
                !sideInCheck) {
                const int futilityValueMargin = SearchTuning::futilityMargin(depth);
                int futilityValue = staticEval - futilityValueMargin;

                if (futilityValue >= beta) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kSeePruningDepthLimit && isCaptureMove && !isCheckMove && !sideInCheck) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                if (seeValue < -depth * kSeePruningScale) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kHistoryPruningDepthLimit && !isCaptureMove && !isCheckMove &&
                !sideInCheck && !context.killerMoves.isKiller(ply, move) &&
                moveCount > kQuietHistoryPruningMoveThreshold) {
                if (histScore < kHistoryPruningBaseThreshold * depth) {
                    moveCount++;
                    continue;
                }
            }

            MoveApplicationData moveData{};
            if (!applySearchMoveWithData(board, move.first, move.second, true, &moveData)) {
                continue;
            }
            uint64_t childZobristKey =
                computeChildZobrist(nodeZobristKey, board, move.first, move.second, moveData);

            if (isInCheck(board, currentColor)) {
                undoSearchMoveWithData(board, moveData);
                continue;
            }
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

            int eval = kZero;

            if (ply + kOne < kContinuationPlyLimit) {
                context.prevPieceAtPly[ply + kOne] = movingPiece;
                context.prevToAtPly[ply + kOne] = move.second;
            }

            int contHistScore = kZero;
            if (ply > kZero) {
                int pp = context.prevPieceAtPly[ply];
                int pt = context.prevToAtPly[ply];
                contHistScore = context.getContinuationHistory(pp, pt, movingPiece, move.second);
            }

            if (moveCount == kZero || !foundPV) {
                eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, true, ply + kOne,
                                       historyTable, context, childZobristKey);
            } else {
                LMREnhanced::MoveClassification mc;
                mc.isCapture = isCaptureMove;
                mc.givesCheck = isCheckMove;
                mc.isKiller = context.killerMoves.isKiller(ply, move);
                mc.isHashMove = (move == hashMove);
                mc.isCounter = (abCounterMove.first >= kZero && move == abCounterMove);
                mc.historyScore = histScore + contHistScore;
                mc.moveNumber = moveCount + kOne;
                LMREnhanced::PositionContext pc;
                pc.inCheck = sideInCheck;
                pc.isPVNode = false;
                pc.isEndgame = false;
                pc.gamePhase = kZero;
                pc.staticEval = staticEval;
                int reduction = LMREnhanced::calculateReduction(depth, mc, pc);
                if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                    int reducedDepth = std::max(kZero, depth - kOne - reduction);
                    int reducedEval =
                        AlphaBetaSearch(board, reducedDepth, beta - kOne, beta, true, ply + kOne,
                                        historyTable, context, childZobristKey);

                    if (reducedEval < beta) {
                        eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, true, ply + kOne,
                                               historyTable, context, childZobristKey);
                    } else {
                        eval = reducedEval;
                    }
                } else {
                    eval = AlphaBetaSearch(board, depth - kOne, beta - kOne, beta, true, ply + kOne,
                                           historyTable, context, childZobristKey);

                    if (eval < beta && eval > alpha) {
                        eval = AlphaBetaSearch(board, depth - kOne, alpha, beta, true, ply + kOne,
                                               historyTable, context, childZobristKey);
                    }
                }
            }

            undoSearchMoveWithData(board, moveData);

            moveCount++;
            if (context.stopSearch) {
                return kZero;
            }
            if (!isCaptureMove) {
                quietMovesSearched.push_back(move);
            }
            if (eval < bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval < beta) {
                beta = eval;
                foundPV = true;
            }
            if (beta <= alpha) {
                int bonus = depth * depth;
                if (!isCaptureMove) {
                    updateHistoryTable(historyTable, move.first, move.second, depth);
                    for (const auto& quietMove : quietMovesSearched) {
                        if (quietMove != move) {
                            penalizeHistoryTable(historyTable, quietMove.first, quietMove.second,
                                                 depth);
                        }
                    }
                    context.killerMoves.store(ply, move);
                    if (abPrevDest >= kZero && abPrevDest < kCounterMoveArrayLimit) {
                        context.counterMoves[abColorIdx][abPrevDest] = move;
                    }
                    if (ply > kZero) {
                        int pp = context.prevPieceAtPly[ply];
                        int pt = context.prevToAtPly[ply];
                        context.updateContinuationHistory(pp, pt, movingPiece, move.second, bonus);
                    }
                } else {
                    int victimPiece = pieceTypeIndex(moveData.capturedPiece.PieceType);
                    context.updateCaptureHistory(movingPiece, victimPiece, move.second, bonus);
                }
                break;
            }
        }
        if (bestValue >= origBeta) {
            flag = kLowerBoundFlag;
        } else if (bestValue <= origAlpha) {
            flag = kUpperBoundFlag;
        } else {
            flag = kExactFlag;
        }
    }

    storeTtEntry(context, nodeZobristKey, depth, bestValue, flag, bestMoveFound, ply);
    return bestValue;
}
