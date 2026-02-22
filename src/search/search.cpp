#include "search.h"
#include "../ai/SyzygyTablebase.h"
#include "../utils/engine_globals.h"
#include "AdvancedSearch.h"
#include "LMR.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <limits>
#include <random>
#include <ranges>
#include <thread>

namespace {
constexpr int kZero = SearchConstants::kZero;
constexpr int kOne = SearchConstants::kOne;
constexpr int kTwo = SearchConstants::kTwo;
constexpr int kThree = 3;
constexpr int kFour = 4;
constexpr int kNegativeOne = SearchConstants::kInvalidSquare;
constexpr int kWhiteIndex = SearchConstants::kZero;
constexpr int kBlackIndex = SearchConstants::kOne;
constexpr int kExactFlag = SearchConstants::kZero;
constexpr int kUpperBoundFlag = SearchConstants::kInvalidSquare;
constexpr int kLowerBoundFlag = SearchConstants::kOne;
constexpr int kMvvLvaVictimPawn = 50;
constexpr int kMvvLvaVictimKnight = 52;
constexpr int kMvvLvaVictimBishop = 54;
constexpr int kMvvLvaVictimRook = 53;

constexpr int kBoardDimension = 8;
constexpr int kBoardSquareCount = kBoardDimension * kBoardDimension;
constexpr int kPieceTypePerColorCount = 6;
constexpr int kInvalidSquare = -1;

constexpr int kKillerBaseScore = 5000;
constexpr int kKillerSlotPenalty = 100;
constexpr int kDefaultThreadFallback = 4;
constexpr std::uint64_t kZobristSeed = 202406ULL;

constexpr int kPawnValue = 100;
constexpr int kMinorPieceValue = 300;
constexpr int kRookValue = 500;
constexpr int kQueenValue = 975;
constexpr int kKingValue = 10000;

constexpr int kCounterMoveScore = 850000;
constexpr int kMvvLvaScale = 1000;
constexpr int kCaptureHistoryDivisor = 8;
constexpr int kContinuationHistoryDivisor = 4;

constexpr int kMaxSearchPly = 50;
constexpr int kMaxPvDepth = 20;
constexpr int kMateScore = 10000;
constexpr int kSyzygyWinScore = 9000;
constexpr int kNullMoveDepthReduction = 3;
constexpr int kMultiCutReductionMoves = 3;
constexpr int kSingularDepthThreshold = 8;
constexpr int kSingularTtDepthMargin = 3;
constexpr int kSingularMarginPerDepth = 2;
constexpr int kSingularReducedDepthDivisor = 2;
constexpr int kExtensionPlyLimit = 16;
constexpr int kLateMoveCountThreshold = 2;
constexpr int kLmrEndgamePhaseThreshold = 10;
constexpr int kCounterMoveArrayLimit = kBoardSquareCount;

constexpr int kSeePruningDepthLimit = 6;
constexpr int kSeePruningScale = 80;
constexpr int kHistoryPruningDepthLimit = 4;
constexpr int kHistoryPruningBaseThreshold = -1000;
constexpr int kQuietHistoryPruningMoveThreshold = 0;
constexpr int kFutilityDepthLimit = 3;
constexpr int kFutilityMarginDepth1 = 200;
constexpr int kFutilityMarginDepth2 = 400;
constexpr int kFutilityMarginDepth3 = 600;

constexpr int kNullMoveMinDepth = 3;
constexpr int kNullMoveMaxDepth = 10;
constexpr int kNullMovePlyLimit = 30;
constexpr int kNullMoveReduction = 3;
constexpr int kNullMoveFailMargin = 300;
constexpr int kLateDepthReductionThreshold = 4;

constexpr int kAspirationWindow = 50;
constexpr int kMaxSearchAttempts = 4;
constexpr int kEarlyDepthNoAspirationLimit = 3;
constexpr long long kNpsMillisScaleLl = 1000LL;
constexpr int kStabilityChangeThreshold = 2;
constexpr int kStabilityLowThreshold = 2;
constexpr int kStabilityHighThreshold = 4;
constexpr double kUnstableStabilityFactor = 1.5;
constexpr double kStableStabilityFactor = 0.6;
constexpr int kScoreInstabilityDepth = 5;
constexpr int kScoreRiseThreshold = 50;
constexpr int kScoreDropThreshold = 20;
constexpr double kScoreRiseFactor = 1.5;
constexpr double kScoreDropFactor = 0.7;
constexpr int kSoftLimitDepthThreshold = 5;
constexpr int kDefaultFindBestMoveTimeLimitMs = 30000;
constexpr int kInitialAspirationWindow = 50;
constexpr int kMaxAspirationWindow = 400;
constexpr int kAspirationShrinkDivisor = 2;

constexpr int kSeeThresholdBase = -25;
constexpr int kSeeThresholdPerPly = 15;
constexpr int kDeltaPruneMargin = 150;
constexpr int kBigDeltaMargin = 1200;
constexpr int kSeeCaptureBonus = 50;
constexpr int kWinningSeeBonus = 200;
constexpr int kCaptureVictimScale = 100;
constexpr int kQueenGamePhaseIncrement = 4;
constexpr int kFutilityMarginTableSize = 4;

constexpr int kAttackerInitValue = 10000;
constexpr int kCastlingDistance = 2;
constexpr int kPromotionWhiteRank = 7;
constexpr int kPromotionBlackRank = 0;
constexpr int kPositionalScoreDivisor = 10;

constexpr int kContinuationPlyLimit = 64;
constexpr int kZeroWindowOffset = 1;
} // namespace

ThreadSafeHistory::ThreadSafeHistory()
    : table(kBoardSquareCount, std::vector<int>(kBoardSquareCount, kZero)) {}
void ThreadSafeHistory::update(int srcPos, int destPos, int depth) {
    std::lock_guard<std::mutex> lock(mutex);
    table[srcPos][destPos] += depth * depth;
}
int ThreadSafeHistory::get(int srcPos, int destPos) const {
    std::lock_guard<std::mutex> lock(mutex);
    return table[srcPos][destPos];
}

KillerMoves::KillerMoves() {
    for (auto& killersForPly : killers) {
        for (auto& killerMove : killersForPly) {
            killerMove = {kInvalidSquare, kInvalidSquare};
        }
    }
}

void KillerMoves::store(int ply, std::pair<int, int> move) {
    if (ply < kZero || ply >= MAX_PLY) {
        return;
    }

    if (isKiller(ply, move)) {
        return;
    }

    for (int i = MAX_KILLER_MOVES - kOne; i > kZero; i--) {
        killers[ply][i] = killers[ply][i - kOne];
    }
    killers[ply][kZero] = move;
}

bool KillerMoves::isKiller(int ply, std::pair<int, int> move) const {
    if (ply < kZero || ply >= MAX_PLY) {
        return false;
    }

    for (int i = kZero; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return true;
        }
    }
    return false;
}

int KillerMoves::getKillerScore(int ply, std::pair<int, int> move) const {
    if (ply < kZero || ply >= MAX_PLY) {
        return kZero;
    }

    for (int i = kZero; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return kKillerBaseScore - (i * kKillerSlotPenalty);
        }
    }
    return kZero;
}

ParallelSearchContext::ParallelSearchContext(int threads)
    : stopSearch(false), nodeCount(kZero), numThreads(threads), ply(kZero), contempt(kZero),
      multiPV(kOne), continuationHistory{}, captureHistory{}, prevPieceAtPly{}, prevToAtPly{} {
    if (numThreads == kZero) {
        numThreads = static_cast<int>(std::thread::hardware_concurrency());
    }
    if (numThreads == kZero) {
        numThreads = kDefaultThreadFallback;
    }
    for (auto& movesBySquare : counterMoves) {
        for (auto& counterMove : movesBySquare) {
            counterMove = {kInvalidSquare, kInvalidSquare};
        }
    }
}

SearchResult::SearchResult()
    : bestMove({kInvalidSquare, kInvalidSquare}), score(kZero), depth(kZero), nodes(kZero),
      timeMs(kZero) {}

void InitZobrist() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;
    std::mt19937_64 rng(kZobristSeed);
    std::uniform_int_distribution<uint64_t> dist;
    for (auto& squareKeys : ZobristTable) {
        for (auto& pieceKey : squareKeys) {
            pieceKey = dist(rng);
        }
    }
    ZobristBlackToMove = dist(rng);
}

int pieceToZobristIndex(const Piece& p) {
    if (p.PieceType == ChessPieceType::NONE) {
        return kNegativeOne;
    }
    return (static_cast<int>(p.PieceType) - kOne) +
           (p.PieceColor == ChessPieceColor::BLACK ? kPieceTypePerColorCount : kZero);
}

uint64_t ComputeZobrist(const Board& board) {
    uint64_t h = kZero;
    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        int idx = pieceToZobristIndex(board.squares[sq].piece);
        if (idx >= kZero) {
            h ^= ZobristTable[sq][idx];
        }
    }
    if (board.turn == ChessPieceColor::BLACK) {
        h ^= ZobristBlackToMove;
    }
    return h;
}

bool isCapture(const Board& board, int srcPos, int destPos) {
    (void)srcPos;
    return board.squares[destPos].piece.PieceType != ChessPieceType::NONE;
}

bool givesCheck(const Board& board, int srcPos, int destPos) {
    Board tempBoard = board;
    tempBoard.movePiece(srcPos, destPos);
    tempBoard.updateBitboards();
    ChessPieceColor kingColor =
        (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    return IsKingInCheck(tempBoard, kingColor);
}

bool isInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

int getPieceValue(ChessPieceType pieceType) {
    switch (pieceType) {
        case ChessPieceType::PAWN:
            return kPawnValue;
        case ChessPieceType::KNIGHT:
        case ChessPieceType::BISHOP:
            return kMinorPieceValue;
        case ChessPieceType::ROOK:
            return kRookValue;
        case ChessPieceType::QUEEN:
            return kQueenValue;
        case ChessPieceType::KING:
            return kKingValue;
        default:
            return kZero;
    }
}

std::vector<ScoredMove> scoreMovesOptimized(const Board& board,
                                            const std::vector<std::pair<int, int>>& moves,
                                            const ThreadSafeHistory& historyTable,
                                            const KillerMoves& killerMoves, int ply,
                                            const std::pair<int, int>& ttMove,
                                            const std::pair<int, int>& counterMove) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());

    for (const auto& move : moves) {
        int score = kZero;

        if (move == ttMove) {
            score = EnhancedMoveOrdering::HASH_MOVE_SCORE;
        }

        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            score = EnhancedMoveOrdering::CAPTURE_SCORE_BASE + (mvvLva * kMvvLvaScale) + see;
        }

        else if (killerMoves.isKiller(ply, move)) {
            score = EnhancedMoveOrdering::KILLER_SCORE +
                    EnhancedMoveOrdering::getKillerScore(killerMoves, ply, move.first, move.second);
        }

        else if (counterMove.first >= kZero && move == counterMove) {
            score = kCounterMoveScore;
        }

        else {
            score = EnhancedMoveOrdering::HISTORY_SCORE_BASE +
                    EnhancedMoveOrdering::getHistoryScore(historyTable, move.first, move.second);

            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
        }

        scoredMoves.emplace_back(move, score);
    }

    return scoredMoves;
}

MovePicker::MovePicker(Board& b, std::pair<int, int> hm, const KillerMoves& km, int p,
                       const ThreadSafeHistory& h, ParallelSearchContext& ctx)
    : board(b), hashMove(hm), killerMoves(km), ply(p), history(h), context(ctx),
      stage(MovePickerStage::HASH_MOVE) {}

void MovePicker::generateAndPartitionMoves() {
    if (movesGenerated) {
        return;
    }
    movesGenerated = true;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    goodCaptures.reserve(moves.size());
    badCaptures.reserve(moves.size());
    quietMoves.reserve(moves.size());

    const int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    const int prevDest = board.LastMove;
    std::pair<int, int> counterMove = {kInvalidSquare, kInvalidSquare};
    if (prevDest >= kZero && prevDest < kCounterMoveArrayLimit) {
        counterMove = context.counterMoves[colorIdx][prevDest];
    }

    for (const auto& move : moves) {
        if (move == hashMove) {
            continue;
        }
        if (killerMoves.isKiller(ply, move)) {
            continue;
        }
        if (counterMove.first >= kZero && move == counterMove) {
            continue;
        }

        bool isCap = isCapture(board, move.first, move.second);
        int movingPieceIdx = pieceTypeIndex(board.squares[move.first].piece.PieceType);
        if (isCap) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            int victimIdx = pieceTypeIndex(board.squares[move.second].piece.PieceType);
            int capHist = context.getCaptureHistory(movingPieceIdx, victimIdx, move.second);
            int score = (mvvLva * kMvvLvaScale) + see + (capHist / kCaptureHistoryDivisor);
            if (see >= kZero) {
                goodCaptures.emplace_back(move, score);
            } else {
                badCaptures.emplace_back(move, score);
            }
        } else {
            int score = EnhancedMoveOrdering::getHistoryScore(history, move.first, move.second);
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
            int pp = context.prevPieceAtPly[std::min(ply, kBoardSquareCount - kOne)];
            int pt = context.prevToAtPly[std::min(ply, kBoardSquareCount - kOne)];
            score += context.getContinuationHistory(pp, pt, movingPieceIdx, move.second) /
                     kContinuationHistoryDivisor;
            quietMoves.emplace_back(move, score);
        }
    }

    std::ranges::sort(goodCaptures, std::greater<ScoredMove>());
    std::ranges::sort(quietMoves, std::greater<ScoredMove>());
    std::ranges::sort(badCaptures, std::greater<ScoredMove>());
}

std::pair<int, int> MovePicker::next() {
    while (true) {
        switch (stage) {
            case MovePickerStage::HASH_MOVE:
                stage = MovePickerStage::GEN_CAPTURES;
                if (hashMove.first >= kZero && hashMove.second >= kZero) {
                    markReturned(hashMove);
                    return hashMove;
                }
                break;

            case MovePickerStage::GEN_CAPTURES:
                generateAndPartitionMoves();
                captureIdx = kZero;
                stage = MovePickerStage::GOOD_CAPTURES;
                break;

            case MovePickerStage::GOOD_CAPTURES:
                if (captureIdx < goodCaptures.size()) {
                    auto m = goodCaptures[captureIdx++].move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    break;
                }
                stage = MovePickerStage::KILLERS;
                break;

            case MovePickerStage::KILLERS:
                stage = MovePickerStage::COUNTERMOVE;
                for (int i = kZero; i < KillerMoves::MAX_KILLER_MOVES; ++i) {
                    auto k = killerMoves.killers[ply][i];
                    if (k.first >= kZero && !alreadyReturned(k)) {
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
                    if (cm.first >= kZero && !alreadyReturned(cm)) {
                        markReturned(cm);
                        return cm;
                    }
                }
                break;
            }

            case MovePickerStage::GEN_QUIETS:
                quietIdx = kZero;
                stage = MovePickerStage::QUIETS;
                break;

            case MovePickerStage::QUIETS:
                if (quietIdx < quietMoves.size()) {
                    auto m = quietMoves[quietIdx++].move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    break;
                }
                badCaptureIdx = kZero;
                stage = MovePickerStage::BAD_CAPTURES;
                break;

            case MovePickerStage::BAD_CAPTURES:
                if (badCaptureIdx < badCaptures.size()) {
                    auto m = badCaptures[badCaptureIdx++].move;
                    if (!alreadyReturned(m)) {
                        markReturned(m);
                        return m;
                    }
                    break;
                }
                stage = MovePickerStage::DONE;
                break;

            case MovePickerStage::DONE:
                return {kInvalidSquare, kInvalidSquare};
        }
    }
}

void updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth) {
    historyTable.updateScore(fromSquare, toSquare, depth * depth);
}

bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    if (timeLimitMs <= kZero) {
        return false;
    }

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

std::string getBookMove(const std::string& fen) {

    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<std::size_t> dis(kZero, options.size() - kOne);
            return options[dis(gen)];
        }
    }

    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) {
        return it->second;
    }
    return "";
}

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply) {
    if (context.stopSearch.load()) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        return evaluatePosition(board, context.contempt);
    }

    context.nodeCount.fetch_add(kOne);

    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    bool inCheck = isInCheck(board, currentColor);

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, currentColor);

    if (moves.empty()) {
        if (inCheck) {
            int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
            return mateScore;
        }
        return -context.contempt;
    }

    int standPat = evaluatePosition(board, context.contempt);

    if (inCheck) {
        standPat = maximizingPlayer ? -kMateScore : kMateScore;
    }

    if (maximizingPlayer) {
        if (standPat >= beta) {
            return beta;
        }
        alpha = std::max(alpha, standPat);
    } else {
        if (standPat <= alpha) {
            return alpha;
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

    std::vector<std::pair<int, int>> tacticalMoves;
    if (inCheck) {
        tacticalMoves = moves;
    } else {
        for (const auto& move : moves) {
            if (isCapture(board, move.first, move.second)) {
                tacticalMoves.push_back(move);
            }
        }
    }

    if (tacticalMoves.empty()) {
        return standPat;
    }

    std::vector<ScoredMove> scoredMoves;
    for (const auto& move : tacticalMoves) {
        int score = kZero;
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
        }

        scoredMoves.emplace_back(move, score);
    }

    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());

    int bestValue = standPat;

    for (const auto& scoredMove : scoredMoves) {
        if (context.stopSearch.load()) {
            return kZero;
        }

        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.movePiece(move.first, move.second);

        if (isInCheck(newBoard, currentColor)) {
            continue;
        }

        int eval = QuiescenceSearch(newBoard, alpha, beta, !maximizingPlayer, historyTable, context,
                                    ply + kOne);
        if (context.stopSearch.load()) {
            return kZero;
        }

        if (maximizingPlayer) {
            bestValue = std::max(bestValue, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestValue = std::min(bestValue, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            break;
        }
    }

    return bestValue;
}

int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer,
                             int ply, ThreadSafeHistory& historyTable,
                             ParallelSearchContext& context, bool isPVNode) {

    if (depth < kZero || depth > kMaxPvDepth) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        return kZero;
    }

    if (isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch = true;
        return kZero;
    }

    if (depth == kZero) {
        return QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
    }

    context.nodeCount++;

    uint64_t zobristKey = ComputeZobrist(board);
    context.transTable.prefetch(zobristKey);
    TTEntry ttEntry;
    bool ttHit = context.transTable.find(zobristKey, ttEntry);
    if (ttHit && ttEntry.depth >= depth) {
        if (ttEntry.flag == kExactFlag) {
            return ttEntry.value;
        } else if (ttEntry.flag == kUpperBoundFlag && ttEntry.value <= alpha) {
            return alpha;
        } else if (ttEntry.flag == kLowerBoundFlag && ttEntry.value >= beta) {
            return beta;
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
                context.transTable.insert(
                    zobristKey,
                    TTEntry(depth, tbScore, tbBound, {kInvalidSquare, kInvalidSquare}, zobristKey));
                return tbScore;
            }
        }
    }

    if (depth >= kLateDepthReductionThreshold && (!ttHit || ttEntry.bestMove.first < kZero)) {
        depth -= kOne;
    }

    int staticEval = evaluatePosition(board, context.contempt);

    int gamePhase = kZero;
    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        switch (board.squares[sq].piece.PieceType) {
            case ChessPieceType::KNIGHT:
            case ChessPieceType::BISHOP:
                gamePhase += kOne;
                break;
            case ChessPieceType::ROOK:
                gamePhase += kTwo;
                break;
            case ChessPieceType::QUEEN:
                gamePhase += kQueenGamePhaseIncrement;
                break;
            default:
                break;
        }
    }

    if (!isPVNode) {

        if (AdvancedSearch::staticNullMovePruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::futilityPruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::nullMovePruning(board, depth, alpha, beta)) {
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
            int nullScore = -PrincipalVariationSearch(board, depth - kNullMoveDepthReduction, -beta,
                                                      -beta + kZeroWindowOffset, !maximizingPlayer,
                                                      ply + kOne, historyTable, context, false);
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

            if (nullScore >= beta) {
                return beta;
            }
        }

        if (AdvancedSearch::multiCutPruning(board, depth, alpha, beta, kMultiCutReductionMoves)) {
            return beta;
        }
    }

    GenValidMoves(board);
    MovePicker picker(board, ttEntry.bestMove, context.killerMoves, ply, historyTable, context);

    int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int prevDest = board.LastMove;
    const bool sideInCheck = isInCheck(board, board.turn);

    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    std::pair<int, int> bestMove = {kInvalidSquare, kInvalidSquare};
    int flag = kUpperBoundFlag;
    int movesSearched = kZero;

    std::pair<int, int> move;
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

        Board tempBoard = board;
        tempBoard.movePiece(move.first, move.second);

        ChessPieceColor movingColor = board.turn;
        if (isInCheck(tempBoard, movingColor)) {
            continue;
        }

        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;

        int extension = kZero;
        const bool basicExtension = AdvancedSearch::checkExtension(board, move, depth) ||
                                    AdvancedSearch::recaptureExtension(board, move, depth) ||
                                    AdvancedSearch::pawnPushExtension(board, move, depth) ||
                                    AdvancedSearch::passedPawnExtension(board, move, depth);
        if (basicExtension) {
            extension = kOne;
        } else if (depth >= kSingularDepthThreshold && move == ttEntry.bestMove &&
                   ttEntry.depth >= depth - kSingularTtDepthMargin && ttEntry.value > alpha &&
                   ttEntry.value < beta) {
            int seMargin = kSingularMarginPerDepth * depth;
            int seBeta = ttEntry.value - seMargin;
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

        int searchDepth = std::max(kZero, depth - kOne + extension);

        if (movesSearched >= kLateMoveCountThreshold && searchDepth >= kLateMoveCountThreshold) {
            LMREnhanced::MoveClassification mc;
            mc.isCapture = isCaptureMove;
            mc.givesCheck = givesCheck(board, move.first, move.second);
            mc.isKiller = context.killerMoves.isKiller(ply, move);
            mc.isHashMove = (move == ttEntry.bestMove);
            mc.isPromotion = isPromotion(board, move.first, move.second);
            mc.isCastling = isCastling(board, move.first, move.second);
            mc.historyScore = historyTable.get(move.first, move.second);
            mc.moveNumber = movesSearched;

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
        if (movesSearched == kZero || isPVNode) {
            score =
                -PrincipalVariationSearch(tempBoard, searchDepth, -beta, -alpha, !maximizingPlayer,
                                          ply + kOne, historyTable, context, isPVNode);
        } else {
            score = -PrincipalVariationSearch(tempBoard, searchDepth, -alpha - kOne, -alpha,
                                              !maximizingPlayer, ply + kOne, historyTable, context,
                                              false);
            if (score > alpha && score < beta) {
                score = -PrincipalVariationSearch(tempBoard, searchDepth, -beta, -alpha,
                                                  !maximizingPlayer, ply + kOne, historyTable,
                                                  context, true);
            }
        }

        movesSearched++;

        if (maximizingPlayer) {
            if (score > bestValue) {
                bestValue = score;
                bestMove = move;
                if (score > alpha) {
                    alpha = score;
                    flag = kExactFlag;
                }
            }
        } else {
            if (score < bestValue) {
                bestValue = score;
                bestMove = move;
                if (score < beta) {
                    beta = score;
                    flag = kExactFlag;
                }
            }
        }

        if (alpha >= beta) {
            flag = kLowerBoundFlag;
            if (!isCaptureMove) {
                context.killerMoves.store(ply, move);
                if (prevDest >= kZero && prevDest < kCounterMoveArrayLimit) {
                    context.counterMoves[colorIdx][prevDest] = move;
                }
            }
            updateHistoryTable(historyTable, move.first, move.second, depth);
            break;
        }
    }

    if (movesSearched == kZero) {
        if (sideInCheck) {
            return maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
        } else {
            return -context.contempt;
        }
    }

    context.transTable.insert(zobristKey, TTEntry(depth, bestValue, flag, bestMove, zobristKey));

    return bestValue;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply,
                    ThreadSafeHistory& historyTable, ParallelSearchContext& context) {

    if (depth < kZero || depth > kMaxPvDepth) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        return kZero;
    }

    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return kZero;
    }
    context.nodeCount.fetch_add(kOne);

    int extension = kZero;
    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;

    if (depth == kOne && ply < kLmrEndgamePhaseThreshold && isInCheck(board, currentColor)) {
        extension = kOne;
    }

    else if (depth >= kSingularDepthThreshold && ply < kLmrEndgamePhaseThreshold) {
        TTEntry ttEntry;
        uint64_t hash = ComputeZobrist(board);
        if (context.transTable.find(hash, ttEntry) &&
            ttEntry.depth >= depth - kSingularTtDepthMargin) {
            if (ttEntry.bestMove.first != kInvalidSquare && ttEntry.value > alpha &&
                ttEntry.value < beta) {

                int singularMargin = kSingularMarginPerDepth * depth;
                int singularBeta = ttEntry.value - singularMargin;
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

    uint64_t hash = ComputeZobrist(board);
    context.transTable.prefetch(hash);
    TTEntry entry;
    std::pair<int, int> hashMove = {kInvalidSquare, kInvalidSquare};
    if (context.transTable.find(hash, entry)) {
        if (entry.depth >= depth) {
            if (entry.flag == kExactFlag) {
                return entry.value;
            }
            if (entry.flag == kUpperBoundFlag && entry.value <= alpha) {
                return alpha;
            }
            if (entry.flag == kLowerBoundFlag && entry.value >= beta) {
                return beta;
            }
        }

        if (entry.bestMove.first != kInvalidSquare && entry.bestMove.second != kInvalidSquare) {
            hashMove = entry.bestMove;
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
            context.transTable.insert(
                hash, TTEntry(depth, tbScore, kExactFlag, {kInvalidSquare, kInvalidSquare}, hash));
            return tbScore;
        }
    }

    if (depth >= kLateDepthReductionThreshold && hashMove.first < kZero) {
        depth -= kOne;
    }

    if (depth == kZero) {
        int eval =
            QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
        context.transTable.insert(
            hash, TTEntry(depth, eval, kExactFlag, {kInvalidSquare, kInvalidSquare}, hash));
        return eval;
    }
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves =
        GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    if (moves.empty()) {
        if (isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
            int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
            context.transTable.insert(hash, TTEntry(depth, mateScore, kExactFlag,
                                                    {kInvalidSquare, kInvalidSquare}, hash));
            return mateScore;
        }
        int drawScore = -context.contempt;
        context.transTable.insert(
            hash, TTEntry(depth, drawScore, kExactFlag, {kInvalidSquare, kInvalidSquare}, hash));
        return drawScore;
    }

    if (depth >= kNullMoveMinDepth && depth <= kNullMoveMaxDepth && ply < kNullMovePlyLimit &&
        !isInCheck(board, currentColor)) {
        Board nullBoard = board;
        nullBoard.turn = (nullBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;
        int reducedDepth = depth - kOne - kNullMoveReduction;
        if (reducedDepth > kZero && reducedDepth <= kNullMoveMaxDepth) {
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer,
                                            ply + kOne, historyTable, context);
            if (context.stopSearch.load()) {
                return kZero;
            }
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + kNullMoveFailMargin) {
                        return beta;
                    }

                    if (depth - kOne <= kNullMoveMaxDepth && ply + kOne < kNullMovePlyLimit) {
                        int verificationValue =
                            AlphaBetaSearch(nullBoard, depth - kOne, alpha, beta, !maximizingPlayer,
                                            ply + kOne, historyTable, context);
                        if (context.stopSearch.load()) {
                            return kZero;
                        }
                        if (verificationValue >= beta) {
                            return beta;
                        }
                    }
                }
            } else {
                if (nullValue <= alpha) {
                    if (nullValue <= alpha - kNullMoveFailMargin) {
                        return alpha;
                    }

                    if (depth - kOne <= kNullMoveMaxDepth && ply + kOne < kNullMovePlyLimit) {
                        int verificationValue =
                            AlphaBetaSearch(nullBoard, depth - kOne, alpha, beta, !maximizingPlayer,
                                            ply + kOne, historyTable, context);
                        if (context.stopSearch.load()) {
                            return kZero;
                        }
                        if (verificationValue <= alpha) {
                            return alpha;
                        }
                    }
                }
            }
        }
    }
    int abColorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int abPrevDest = board.LastMove;
    std::pair<int, int> abCounterMove = {kInvalidSquare, kInvalidSquare};
    if (abPrevDest >= kZero && abPrevDest < kCounterMoveArrayLimit) {
        abCounterMove = context.counterMoves[abColorIdx][abPrevDest];
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
        board, moves, historyTable, context.killerMoves, ply, hashMove, abCounterMove);
    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    int flag = kExactFlag;
    bool foundPV = false;
    std::pair<int, int> bestMoveFound = {kInvalidSquare, kInvalidSquare};

    if (maximizingPlayer) {
        int moveCount = kZero;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) {
                return kZero;
            }
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);

            if (isInCheck(newBoard, currentColor)) {
                continue;
            }

            int eval = kZero;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= kFutilityDepthLimit && !foundPV && !isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor)) {

                int staticEval = evaluatePosition(board, context.contempt);

                const int futilityMargins[kFutilityMarginTableSize] = {
                    kZero, kFutilityMarginDepth1, kFutilityMarginDepth2, kFutilityMarginDepth3};
                int futilityValue = staticEval + futilityMargins[depth];

                if (futilityValue <= alpha) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kSeePruningDepthLimit && isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                if (seeValue < -depth * kSeePruningScale) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kHistoryPruningDepthLimit && !isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor) && !context.killerMoves.isKiller(ply, move) &&
                moveCount > kQuietHistoryPruningMoveThreshold) {
                int histScore = historyTable.get(move.first, move.second);
                if (histScore < kHistoryPruningBaseThreshold * depth) {
                    moveCount++;
                    continue;
                }
            }

            int movingPiece = pieceTypeIndex(board.squares[move.first].piece.PieceType);
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

            if (moveCount == kZero || foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false, ply + kOne,
                                       historyTable, context);
            } else {

                {
                    LMREnhanced::MoveClassification mc;
                    mc.isCapture = isCaptureMove;
                    mc.givesCheck = isCheckMove;
                    mc.isKiller = context.killerMoves.isKiller(ply, move);
                    mc.isHashMove = (move == hashMove);
                    mc.historyScore = historyTable.get(move.first, move.second) + contHistScore;
                    mc.moveNumber = moveCount;

                    LMREnhanced::PositionContext pc;
                    pc.inCheck = isInCheck(board, currentColor);

                    int reduction = LMREnhanced::calculateReduction(depth, mc, pc);

                    if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                        int reducedDepth = std::max(kZero, depth - kOne - reduction);
                        int reducedEval =
                            AlphaBetaSearch(newBoard, reducedDepth, alpha, alpha + kOne, false,
                                            ply + kOne, historyTable, context);

                        if (reducedEval > alpha) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false,
                                                   ply + kOne, historyTable, context);
                        } else {
                            eval = reducedEval;
                        }
                    } else {
                        eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, alpha + kOne, false,
                                               ply + kOne, historyTable, context);

                        if (eval > alpha && eval < beta) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false,
                                                   ply + kOne, historyTable, context);
                        }
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load()) {
                return kZero;
            }
            if (eval > bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval > alpha) {
                alpha = eval;
                foundPV = true;
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {
                int bonus = depth * depth;
                if (!isCaptureMove) {
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
                    int victimPiece = pieceTypeIndex(board.squares[move.second].piece.PieceType);
                    context.updateCaptureHistory(movingPiece, victimPiece, move.second, bonus);
                }
                break;
            }
        }
        if (bestValue <= origAlpha) {
            flag = kUpperBoundFlag;
        } else if (bestValue >= beta) {
            flag = kLowerBoundFlag;
        } else {
            flag = kExactFlag;
        }
    } else {
        int moveCount = kZero;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) {
                return kZero;
            }
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);

            if (isInCheck(newBoard, currentColor)) {
                continue;
            }

            int eval = kZero;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= kSeePruningDepthLimit && isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                if (seeValue < -depth * kSeePruningScale) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= kHistoryPruningDepthLimit && !isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor) && !context.killerMoves.isKiller(ply, move) &&
                moveCount > kQuietHistoryPruningMoveThreshold) {
                int histScore = historyTable.get(move.first, move.second);
                if (histScore < kHistoryPruningBaseThreshold * depth) {
                    moveCount++;
                    continue;
                }
            }

            int movingPiece = pieceTypeIndex(board.squares[move.first].piece.PieceType);
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

            if (moveCount == kZero || foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true, ply + kOne,
                                       historyTable, context);
            } else {

                {
                    LMREnhanced::MoveClassification mc;
                    mc.isCapture = isCaptureMove;
                    mc.givesCheck = isCheckMove;
                    mc.isKiller = context.killerMoves.isKiller(ply, move);
                    mc.isHashMove = (move == hashMove);
                    mc.historyScore = historyTable.get(move.first, move.second) + contHistScore;
                    mc.moveNumber = moveCount;

                    LMREnhanced::PositionContext pc;
                    pc.inCheck = isInCheck(board, currentColor);

                    int reduction = LMREnhanced::calculateReduction(depth, mc, pc);

                    if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                        int reducedDepth = std::max(kZero, depth - kOne - reduction);
                        int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, beta - kOne, beta,
                                                          true, ply + kOne, historyTable, context);

                        if (reducedEval < beta) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true,
                                                   ply + kOne, historyTable, context);
                        } else {
                            eval = reducedEval;
                        }
                    } else {
                        eval = AlphaBetaSearch(newBoard, depth - kOne, beta - kOne, beta, true,
                                               ply + kOne, historyTable, context);

                        if (eval < beta && eval > alpha) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true,
                                                   ply + kOne, historyTable, context);
                        }
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load()) {
                return kZero;
            }
            if (eval < bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval < beta) {
                beta = eval;
                foundPV = true;
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {
                int bonus = depth * depth;
                if (!isCaptureMove) {
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
                    int victimPiece = pieceTypeIndex(board.squares[move.second].piece.PieceType);
                    context.updateCaptureHistory(movingPiece, victimPiece, move.second, bonus);
                }
                break;
            }
        }
        if (bestValue >= beta) {
            flag = kLowerBoundFlag;
        } else if (bestValue <= origAlpha) {
            flag = kUpperBoundFlag;
        } else {
            flag = kExactFlag;
        }
    }

    context.transTable.insert(hash, TTEntry(depth, bestValue, flag, bestMoveFound, hash));
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs, int numThreads,
                                        int contempt, int multiPV, int optimalTimeMs,
                                        int maxTimeMs) {
    SearchResult result;
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    context.contempt = contempt;
    context.multiPV = multiPV;
    context.optimalTimeMs = optimalTimeMs;
    context.maxTimeMs = maxTimeMs;
    context.useSyzygy = !Syzygy::getPath().empty();
    context.transTable.newSearch();

    int lastScore = kZero;
    const int aspirationWindow = kAspirationWindow;

    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        int srcCol = kZero;
        int srcRow = kZero;
        int destCol = kZero;
        int destRow = kZero;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            result.bestMove = {(srcRow * kBoardDimension) + srcCol,
                               (destRow * kBoardDimension) + destCol};
            result.score = kZero;
            result.depth = kOne;
            result.nodes = kOne;
            result.timeMs = kZero;
            return result;
        }
    }

    std::pair<int, int> previousBestMove = {kInvalidSquare, kInvalidSquare};
    int bestMoveStableCount = kZero;
    int bestMoveChangeCount = kZero;

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

            do {
                searchScore = PrincipalVariationSearch(board, depth, alpha, beta,
                                                       board.turn == ChessPieceColor::WHITE, kZero,
                                                       context.historyTable, context, true);
                searchAttempts++;

                if (context.stopSearch.load()) {
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
                     !context.stopSearch.load());

            if (context.stopSearch.load()) {
                break;
            }

            lastScore = searchScore;
            result.score = searchScore;
            result.depth = depth;
            result.nodes = context.nodeCount.load();

            uint64_t rootHash = ComputeZobrist(board);
            TTEntry rootEntry;
            if (context.transTable.find(rootHash, rootEntry) && rootEntry.bestMove.first >= kZero) {
                result.bestMove = rootEntry.bestMove;
            } else {
                GenValidMoves(board);
                std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
                if (!moves.empty()) {
                    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
                        board, moves, context.historyTable, context.killerMoves, kZero);
                    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
                    result.bestMove = scoredMoves[kZero].move;
                }
            }

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
                if (context.tbHits.load() > kZero) {
                    std::cout << " tbhits " << context.tbHits.load();
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

        if (result.bestMove.first < kZero) {
            uint64_t rootHash = ComputeZobrist(board);
            TTEntry rootEntry;
            if (context.transTable.find(rootHash, rootEntry) && rootEntry.bestMove.first >= kZero) {
                result.bestMove = rootEntry.bestMove;
            } else {
                GenValidMoves(board);
                std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
                if (!moves.empty()) {
                    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
                        board, moves, context.historyTable, context.killerMoves, kZero);
                    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
                    result.bestMove = scoredMoves[kZero].move;
                }
            }
        }

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

std::pair<int, int> findBestMove(Board& board, int depth) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(kOne);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = kDefaultFindBestMoveTimeLimitMs;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.empty()) {
        return {kInvalidSquare, kInvalidSquare};
    }

    int previousScore = kZero;
    int aspirationWindow = kInitialAspirationWindow;
    int maxAspirationWindow = kMaxAspirationWindow;

    std::vector<ScoredMove> scoredMoves =
        scoreMovesOptimized(board, moves, historyTable, context.killerMoves, kZero);
    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());

    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -kMateScore : kMateScore;
    std::pair<int, int> bestMove = {kInvalidSquare, kInvalidSquare};

    for (int currentDepth = kOne; currentDepth <= depth; currentDepth++) {

        int alpha = -kMateScore;
        int beta = kMateScore;

        if (currentDepth == kOne) {

            alpha = -kMateScore;
            beta = kMateScore;
        } else {

            alpha = previousScore - aspirationWindow;
            beta = previousScore + aspirationWindow;
        }

        bool searchFailed = false;
        int currentBestEval = bestEval;
        std::pair<int, int> currentBestMove = bestMove;

        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);

            int eval = AlphaBetaSearch(newBoard, currentDepth - kOne, alpha, beta,
                                       (board.turn == ChessPieceColor::BLACK), kZero, historyTable,
                                       context);

            if (board.turn == ChessPieceColor::WHITE) {
                if (eval > currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;

                    alpha = std::max(eval, alpha);

                    if (eval >= beta) {
                        searchFailed = true;
                        break;
                    }
                }
            } else {
                if (eval < currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;

                    beta = std::min(eval, beta);

                    if (eval <= alpha) {
                        searchFailed = true;
                        break;
                    }
                }
            }
        }

        if (searchFailed && currentDepth > kOne) {

            aspirationWindow =
                std::min(aspirationWindow * kAspirationShrinkDivisor, maxAspirationWindow);

            if (board.turn == ChessPieceColor::WHITE && currentBestEval >= beta) {

                beta = kMateScore;
            } else if (board.turn == ChessPieceColor::BLACK && currentBestEval <= alpha) {

                alpha = -kMateScore;
            } else {

                alpha = -kMateScore;
                beta = kMateScore;
            }

            currentDepth--;
            continue;
        }

        bestEval = currentBestEval;
        bestMove = currentBestMove;
        previousScore = bestEval;

        aspirationWindow =
            std::max(kInitialAspirationWindow, aspirationWindow / kAspirationShrinkDivisor);
    }

    return bestMove;
}

int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare) {
    if (fromSquare < kZero || fromSquare >= kBoardSquareCount || toSquare < kZero ||
        toSquare >= kBoardSquareCount) {
        return kZero;
    }

    const Piece& victim = board.squares[toSquare].piece;
    const Piece& attacker = board.squares[fromSquare].piece;

    if (victim.PieceType == ChessPieceType::NONE || attacker.PieceType == ChessPieceType::NONE) {
        return kZero;
    }

    int score = victim.PieceValue;
    ChessPieceColor sideToMove = attacker.PieceColor;

    Board tempBoard = board;
    tempBoard.squares[toSquare].piece = attacker;
    tempBoard.squares[fromSquare].piece = Piece();
    tempBoard.updateBitboards();

    ChessPieceColor currentSide =
        (sideToMove == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    while (true) {
        int smallestAttacker = getSmallestAttacker(tempBoard, toSquare, currentSide);
        if (smallestAttacker == kInvalidSquare) {
            break;
        }

        const Piece& currentAttacker = tempBoard.squares[smallestAttacker].piece;

        tempBoard.squares[toSquare].piece = currentAttacker;
        tempBoard.squares[smallestAttacker].piece = Piece();
        tempBoard.updateBitboards();

        score = currentAttacker.PieceValue - score;

        currentSide = (currentSide == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                              : ChessPieceColor::WHITE;
    }

    return score;
}

int getSmallestAttacker(const Board& board, int targetSquare, ChessPieceColor color) {
    int smallestValue = kAttackerInitValue;
    int smallestAttacker = kInvalidSquare;

    for (int i = kZero; i < kBoardSquareCount; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }

        if (canPieceAttackSquare(board, i, targetSquare)) {
            if (piece.PieceValue < smallestValue) {
                smallestValue = piece.PieceValue;
                smallestAttacker = i;
            }
        }
    }

    return smallestAttacker;
}

bool isPromotion(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    ChessPieceColor color = board.squares[from].piece.PieceColor;

    if (piece != ChessPieceType::PAWN) {
        return false;
    }

    int destRow = to / kBoardDimension;
    return (color == ChessPieceColor::WHITE && destRow == kPromotionWhiteRank) ||
           (color == ChessPieceColor::BLACK && destRow == kPromotionBlackRank);
}

bool isCastling(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    if (piece != ChessPieceType::KING) {
        return false;
    }

    return std::abs(to - from) == kCastlingDistance;
}

const int EnhancedMoveOrdering::MVV_LVA_SCORES[6][6] = {
    {kZero, kZero, kZero, kZero, kZero, kZero},
    {kMvvLvaVictimPawn, kZero, kZero, kZero, kZero, kZero},
    {kMvvLvaVictimPawn, kZero, kZero, kZero, kZero, kZero},
    {kMvvLvaVictimKnight, kTwo, kTwo, kZero, kZero, kZero},
    {kMvvLvaVictimBishop, kFour, kFour, kTwo, kZero, kZero},
    {kMvvLvaVictimRook, kThree, kThree, kOne, kOne, kZero}};

int EnhancedMoveOrdering::getMVVLVA_Score(const Board& board, int fromSquare, int toSquare) {
    const Piece& attacker = board.squares[fromSquare].piece;
    const Piece& victim = board.squares[toSquare].piece;

    if (victim.PieceType == ChessPieceType::NONE) {
        return kZero;
    }

    int attackerIndex = static_cast<int>(attacker.PieceType) - kOne;
    int victimIndex = static_cast<int>(victim.PieceType) - kOne;

    if (attackerIndex < kZero || attackerIndex >= kPieceTypePerColorCount || victimIndex < kZero ||
        victimIndex >= kPieceTypePerColorCount) {
        return kZero;
    }

    return MVV_LVA_SCORES[attackerIndex][victimIndex];
}

int EnhancedMoveOrdering::getHistoryScore(const ThreadSafeHistory& history, int fromSquare,
                                          int toSquare) {
    return history.get(fromSquare, toSquare);
}

int EnhancedMoveOrdering::getKillerScore(const KillerMoves& killers, int ply, int fromSquare,
                                         int toSquare) {
    std::pair<int, int> move = {fromSquare, toSquare};
    if (killers.isKiller(ply, move)) {
        return killers.getKillerScore(ply, move);
    }
    return kZero;
}

int EnhancedMoveOrdering::getPositionalScore(const Board& board, int fromSquare, int toSquare) {
    const Piece& piece = board.squares[fromSquare].piece;
    if (piece.PieceType == ChessPieceType::NONE) {
        return kZero;
    }

    int toSquareAdjusted = (piece.PieceColor == ChessPieceColor::WHITE)
                               ? toSquare
                               : (kBoardSquareCount - kOne) - toSquare;
    return getPieceSquareValue(piece.PieceType, toSquareAdjusted, piece.PieceColor) /
           kPositionalScoreDivisor;
}
