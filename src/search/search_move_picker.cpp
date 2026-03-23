#include "search_internal.h"

SearchInternal::MovePicker::MovePicker(Board& b, Move hm, const KillerMoves& km, int p,
                                       const ThreadSafeHistory& h, ParallelSearchContext& ctx)
    : board(b), hashMove(hm), killerMoves(km), ply(p), history(h), context(ctx),
      stage(MovePickerStage::HASH_MOVE) {}

void SearchInternal::MovePicker::generateAndPartitionMoves() {
    if (movesGenerated) {
        return;
    }
    movesGenerated = true;
    stage = MovePickerStage::GOOD_CAPTURES;

    std::vector<Move> moves = GetAllMoves(board, board.turn);
    if (moves.empty()) {
        stage = MovePickerStage::DONE;
        return;
    }

    Move counterMove = {kInvalidSquare, kInvalidSquare};
    int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int prevDest = board.LastMove;
    if (prevDest >= kZero && prevDest < kCounterMoveArrayLimit) {
        counterMove = context.counterMoves[colorIdx][prevDest];
    }

    for (const auto& move : moves) {
        markAvailable(move);
        if (move == hashMove) {
            continue;
        }

        int score = kZero;
        bool isCap = isCapture(board, move.first, move.second);

        if (isCap) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            score = EnhancedMoveOrdering::CAPTURE_SCORE_BASE + (mvvLva * kMvvLvaScale) + see;
            int movingPieceIdx = pieceTypeIndex(board.squares[move.first].piece.PieceType);
            int victimPieceIdx = pieceTypeIndex(board.squares[move.second].piece.PieceType);
            score += context.getCaptureHistory(movingPieceIdx, victimPieceIdx, move.second) /
                     kCaptureHistoryDivisor;
            if (see >= kZero) {
                score += kGoodSeeCaptureBonus;
                goodCaptures.emplace_back(move, score);
            } else {
                score -= kBadSeeCapturePenalty;
                badCaptures.emplace_back(move, score);
            }
        } else if (killerMoves.isKiller(ply, move)) {
            score = EnhancedMoveOrdering::KILLER_SCORE +
                    EnhancedMoveOrdering::getKillerScore(killerMoves, ply, move.first, move.second);
            quietMoves.emplace_back(move, score);
        } else {
            score = EnhancedMoveOrdering::HISTORY_SCORE_BASE + history.get(move.first, move.second);
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
            if (ply > kZero) {
                int prevPly = std::min(ply, kBoardSquareCount - kOne);
                int pp = context.prevPieceAtPly[prevPly];
                int pt = context.prevToAtPly[prevPly];
                int movingPieceIdx = pieceTypeIndex(board.squares[move.first].piece.PieceType);
                score += context.getContinuationHistory(pp, pt, movingPieceIdx, move.second) /
                         kContinuationHistoryDivisor;
            }
            if (counterMove.first >= kZero && move == counterMove) {
                score += kCounterMoveQuietBonus;
            }
            if (isPromotion(board, move.first, move.second)) {
                score += kPromotionQuietBonus;
            }
            quietMoves.emplace_back(move, score);
        }
    }
}

Move SearchInternal::MovePicker::next() {
    while (stage != MovePickerStage::DONE) {
        switch (stage) {
            case MovePickerStage::HASH_MOVE:
                stage = MovePickerStage::GEN_CAPTURES;
                if (hashMove.first >= kZero && hashMove.second >= kZero && isAvailable(hashMove)) {
                    markReturned(hashMove);
                    return hashMove;
                }
                break;

            case MovePickerStage::GEN_CAPTURES:
                generateAndPartitionMoves();
                break;

            case MovePickerStage::GOOD_CAPTURES:
                if (captureIdx < goodCaptures.size()) {
                    auto m = pickNextScoredMove(goodCaptures, captureIdx++).move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    continue;
                }
                stage = MovePickerStage::KILLERS;
                break;

            case MovePickerStage::KILLERS:
                stage = MovePickerStage::COUNTERMOVE;
                for (int i = kZero; i < KillerMoves::MAX_KILLER_MOVES; ++i) {
                    auto k = killerMoves.killers[ply][i];
                    if (k.first >= kZero && isAvailable(k) && !alreadyReturned(k)) {
                        markReturned(k);
                        stage = MovePickerStage::KILLERS;
                        return k;
                    }
                }
                break;

            case MovePickerStage::COUNTERMOVE: {
                stage = MovePickerStage::GEN_QUIETS;
                int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
                int prevDest = board.LastMove;
                if (prevDest >= kZero && prevDest < kCounterMoveArrayLimit) {
                    auto cm = context.counterMoves[colorIdx][prevDest];
                    if (cm.first >= kZero && isAvailable(cm) && !alreadyReturned(cm)) {
                        markReturned(cm);
                        return cm;
                    }
                }
                break;
            }

            case MovePickerStage::GEN_QUIETS:
                stage = MovePickerStage::QUIETS;
                break;

            case MovePickerStage::QUIETS:
                if (quietIdx < quietMoves.size()) {
                    auto m = pickNextScoredMove(quietMoves, quietIdx++).move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    continue;
                }
                stage = MovePickerStage::BAD_CAPTURES;
                break;

            case MovePickerStage::BAD_CAPTURES:
                if (badCaptureIdx < badCaptures.size()) {
                    auto m = pickNextScoredMove(badCaptures, badCaptureIdx++).move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    continue;
                }
                stage = MovePickerStage::DONE;
                break;

            case MovePickerStage::DONE:
                break;
        }
    }

    return {kInvalidSquare, kInvalidSquare};
}
