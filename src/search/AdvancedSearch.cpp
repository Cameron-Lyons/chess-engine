#include "AdvancedSearch.h"
#include "../core/BitboardMoves.h"
#include "../evaluation/Evaluation.h"
#include "../utils/engine_globals.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <ranges>
#include <sstream>
#include <unordered_map>

namespace {
constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kBoardSquareCount = 64;
constexpr int kBoardDimension = 8;
constexpr int kInvalidSquare = -1;
constexpr int kCenterStart = 2;
constexpr int kCenterEnd = 5;
constexpr int kRookPromotionRank = 6;
constexpr int kRookDevelopmentRank = 1;

constexpr int kFutilityMarginPerDepth = 150;
constexpr int kShallowDepthLimit = 3;
constexpr int kTacticalBonusPerMissingPly = 50;
constexpr int kDeltaMarginBase = 975;
constexpr int kDeltaMarginPerDepth = 50;

constexpr int kStaticNullDepthLimit = 4;
constexpr int kStaticNullMarginBase = 900;
constexpr int kStaticNullMarginPerDepth = 50;

constexpr int kNullMoveMinDepth = 3;
constexpr int kNullMoveMinMaterialCount = 3;
constexpr int kNullMoveMinTotalMaterial = 300;
constexpr int kNullMoveMaxMaterialCount = 4;

constexpr int kLmrMinDepth = 3;
constexpr int kLmrMinMoveNumber = 4;
constexpr int kLmrSecondReductionMove = 8;
constexpr int kLmrThirdReductionMove = 12;
constexpr int kLmrSecondReduction = 2;
constexpr int kLmrThirdReduction = 3;

constexpr int kMultiCutMinDepth = 4;
constexpr int kMultiCutMinMoves = 8;
constexpr int kMultiCutDepthReduction = 3;
constexpr int kMultiCutCutoffCount = 2;

constexpr int kIidMinDepth = 4;
constexpr int kIidDepthReduction = 2;

constexpr int kSingularMinDepth = 6;
constexpr int kSingularDepthReduction = 1;
constexpr int kSingularOtherMoveReduction = 2;
constexpr int kSingularBetterMoveCutoff = 2;

constexpr int kHistoryPruningDepthLimit = 3;
constexpr int kHistoryPruningThreshold = -1000;
constexpr int kLateMovePruningMoveThreshold = 4;
constexpr int kRecaptureMaxLastMove = kBoardSquareCount - 1;
constexpr int kPawnPushWhiteAdvancedRank = 5;
constexpr int kPawnPushBlackAdvancedRank = 2;

constexpr int kHashMoveScore = 10000;
constexpr int kSeeVictimScale = 10;
constexpr int kSeeUndervaluedCaptureBonus = 50;
constexpr int kSeeCenterBonus = 10;
constexpr int kThreatValueDivisor = 10;
constexpr int kMobilityScorePerMove = 2;
constexpr int kPawnAdvanceBonus = 5;
constexpr int kCenterMinorBonus = 10;
constexpr int kRookRankBonus = 15;
constexpr int kCenterQueenBonus = 5;

constexpr int kDefaultTotalMoves = 30;
constexpr double kInCheckTimeFactor = 1.5;
constexpr int kLowMobilityMoveThreshold = 10;
constexpr int kHighMobilityMoveThreshold = 25;
constexpr double kLowMobilityTimeFactor = 1.2;
constexpr double kHighMobilityTimeFactor = 0.8;
constexpr int kMinAllocatedTimeMs = 50;
constexpr int kMaxBaseTimeMultiplier = 2;
constexpr int kInfiniteBaseTimeMs = 30000;
constexpr int kMinMovesRemaining = 10;
constexpr int kMaxDepthBeforeStop = 20;
constexpr double kEmergencyTimeFraction = 0.8;
constexpr int kDeepSearchDepth = 10;
constexpr double kDeepSearchTimeFactor = 1.2;
constexpr double kShallowSearchTimeFactor = 0.8;
constexpr int kLargeNodeThreshold = 1000000;
constexpr double kLargeNodeTimeFactor = 1.1;

constexpr int kQueenMaterialValue = 900;
constexpr int kRookMaterialValue = 500;
constexpr int kMinorMaterialValue = 300;
constexpr int kPawnMaterialValue = 100;
constexpr int kOpeningMaterialThreshold = 6000;
constexpr int kOpeningPieceCountThreshold = 20;
constexpr int kEndgameMaterialThreshold = 2000;
constexpr int kEndgamePieceCountThreshold = 10;
constexpr int kEndgameRookCountThreshold = 1;
constexpr int kEndgameMinorPieceCountThreshold = 2;
constexpr double kOpeningPhaseTimeFactor = 0.7;
constexpr double kMiddlegamePhaseTimeFactor = 1.0;
constexpr double kEndgamePhaseTimeFactor = 1.3;

constexpr int kMinBookMoveLength = 4;
constexpr int kSingleThreadContextCount = 1;
constexpr int kRootPly = 0;
constexpr int kFirstMoveIndex = 0;
constexpr int kNarrowWindowWidth = 1;
constexpr int kMinReducedDepth = 1;
constexpr int kNoBetterMoves = 0;
constexpr int kMoveSourceFileIndex = 0;
constexpr int kMoveSourceRankIndex = 1;
constexpr int kMoveDestFileIndex = 2;
constexpr int kMoveDestRankIndex = 3;
constexpr char kBookFileOffset = 'a';
constexpr char kBookRankOffset = '1';
constexpr int kOpeningMinQueenCount = 1;
constexpr int kNormalizedWeightTotal = 1000;
constexpr int kFallbackWeight = 1;
constexpr double kDefaultTimeFactor = 1.0;
constexpr float kZeroFloat = 0.0F;
} // namespace

bool AdvancedSearch::futilityPruning(const Board& board, int depth, int alpha, int beta,
                                     int staticEval) {

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    int futilityMargin = kFutilityMarginPerDepth * depth;

    if (depth <= kShallowDepthLimit) {
        int tacticalBonus = kTacticalBonusPerMissingPly * (kShallowDepthLimit - depth);
        futilityMargin += tacticalBonus;
    }

    if (staticEval - futilityMargin >= beta) {
        return true;
    }

    if (depth <= kShallowDepthLimit) {
        int deltaMargin = kDeltaMarginBase + (kDeltaMarginPerDepth * depth);
        if (staticEval + deltaMargin < alpha) {
            return true;
        }
    }

    return false;
}

bool AdvancedSearch::staticNullMovePruning(const Board& board, int depth, int alpha, int beta,
                                           int staticEval) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    if (depth <= kStaticNullDepthLimit) {

        int margin = kStaticNullMarginBase + (kStaticNullMarginPerDepth * depth);
        int eval = staticEval - margin;

        if (eval >= beta) {
            return true;
        }
    }

    return false;
}

bool AdvancedSearch::nullMovePruning(const Board& board, int depth, int alpha, int beta) {
    (void)alpha;
    (void)beta;

    if (isInCheck(board, board.turn) || depth < kNullMoveMinDepth) {
        return false;
    }

    int materialCount = kZero;
    int totalMaterial = kZero;

    for (int i = kZero; i < kBoardSquareCount; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            materialCount++;
            totalMaterial += piece.PieceValue;
        }
    }

    if (materialCount < kNullMoveMinMaterialCount || totalMaterial < kNullMoveMinTotalMaterial) {
        return false;
    }

    if (materialCount <= kNullMoveMaxMaterialCount) {
        return false;
    }

    return true;
}

bool AdvancedSearch::lateMoveReduction(const Board& board, int depth, int moveNumber, int alpha,
                                       int beta) {
    (void)alpha;
    (void)beta;

    if (isInCheck(board, board.turn) || depth < kLmrMinDepth || moveNumber < kLmrMinMoveNumber) {
        return false;
    }

    if (depth <= kShallowDepthLimit) {
        return false;
    }

    int reduction = kMinReducedDepth;
    if (moveNumber >= kLmrSecondReductionMove) {
        reduction = kLmrSecondReduction;
    }
    if (moveNumber >= kLmrThirdReductionMove) {
        reduction = kLmrThirdReduction;
    }

    if (depth - reduction < kMinReducedDepth) {
        return false;
    }

    return true;
}

bool AdvancedSearch::multiCutPruning(Board& board, int depth, int alpha, int beta, int r) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth < kMultiCutMinDepth) {
        return false;
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.size() < kMultiCutMinMoves) {
        return false;
    }

    int nullMoveCount = kZero;
    for (int i = kZero; i < r && i < static_cast<int>(moves.size()); i++) {

        Board tempBoard = board;
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;

        ThreadSafeHistory historyTable;
        ParallelSearchContext context(kSingleThreadContextCount);
        int nullScore = -AlphaBetaSearch(
            tempBoard, depth - kMultiCutDepthReduction, -beta, -beta + kNarrowWindowWidth,
            !(board.turn == ChessPieceColor::WHITE), kRootPly, historyTable, context);

        if (nullScore >= beta) {
            nullMoveCount++;
        }
    }

    return nullMoveCount >= kMultiCutCutoffCount;
}

std::pair<int, int> AdvancedSearch::internalIterativeDeepening(Board& board, int depth, int alpha,
                                                               int beta) {

    if (depth < kIidMinDepth) {
        return {kInvalidSquare, kInvalidSquare};
    }

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(kSingleThreadContextCount);
    int reducedDepth = std::max(kMinReducedDepth, depth - kIidDepthReduction);
    (void)AlphaBetaSearch(board, reducedDepth, alpha, beta, board.turn == ChessPieceColor::WHITE,
                          kRootPly, historyTable, context);

    return {kInvalidSquare, kInvalidSquare};
}

bool AdvancedSearch::singularExtension(Board& board, int depth, const std::pair<int, int>& move,
                                       int alpha, int beta) {

    if (depth < kSingularMinDepth) {
        return false;
    }

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(kSingleThreadContextCount);
    int reducedDepth = depth - kSingularDepthReduction;
    int score =
        AlphaBetaSearch(tempBoard, reducedDepth, alpha, beta,
                        !(board.turn == ChessPieceColor::WHITE), kRootPly, historyTable, context);

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    int betterMoves = kZero;
    for (const auto& otherMove : moves) {
        if (otherMove == move) {
            continue;
        }

        Board otherBoard = board;
        if (!otherBoard.movePiece(otherMove.first, otherMove.second)) {
            continue;
        }
        otherBoard.turn = (otherBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                      : ChessPieceColor::WHITE;

        int otherScore = AlphaBetaSearch(otherBoard, reducedDepth - kSingularOtherMoveReduction,
                                         alpha, beta, !(board.turn == ChessPieceColor::WHITE),
                                         kRootPly, historyTable, context);

        if (otherScore >= score) {
            betterMoves++;
            if (betterMoves >= kSingularBetterMoveCutoff) {
                break;
            }
        }
    }

    return betterMoves == kNoBetterMoves;
}

bool AdvancedSearch::historyPruning(const Board& board, int depth, const std::pair<int, int>& move,
                                    const ThreadSafeHistory& history) {

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int historyScore = history.getScore(move.first, move.second);
    return depth <= kHistoryPruningDepthLimit && historyScore < kHistoryPruningThreshold;
}

bool AdvancedSearch::lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck) {
    (void)board;

    if (inCheck) {
        return false;
    }

    if (depth == kZero || depth > kShallowDepthLimit) {
        return false;
    }

    if (moveNumber >= kLateMovePruningMoveThreshold && depth <= kShallowDepthLimit) {
        return true;
    }

    return false;
}

bool AdvancedSearch::recaptureExtension(const Board& board, const std::pair<int, int>& move,
                                        int depth) {
    (void)depth;

    if (board.LastMove < kZero || board.LastMove > kRecaptureMaxLastMove) {
        return false;
    }

    if (move.second == board.LastMove) {

        return true;
    }

    return false;
}

bool AdvancedSearch::checkExtension(const Board& board, const std::pair<int, int>& move,
                                    int depth) {
    (void)depth;
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    return isInCheck(tempBoard, tempBoard.turn);
}

bool AdvancedSearch::pawnPushExtension(const Board& board, const std::pair<int, int>& move,
                                       int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int destRow = move.second / kBoardDimension;
    if (piece.PieceColor == ChessPieceColor::WHITE && destRow >= kPawnPushWhiteAdvancedRank) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && destRow <= kPawnPushBlackAdvancedRank) {
        return true;
    }

    return false;
}

bool AdvancedSearch::passedPawnExtension(const Board& board, const std::pair<int, int>& move,
                                         int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    int srcRow = move.first / kBoardDimension;
    int destRow = move.second / kBoardDimension;

    if (piece.PieceColor == ChessPieceColor::WHITE && destRow - srcRow > kZero) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && srcRow - destRow > kZero) {
        return true;
    }

    return false;
}

std::vector<EnhancedMoveOrdering::MoveScore> EnhancedMoveOrdering::scoreMoves(
    const Board& board, const std::vector<std::pair<int, int>>& moves,
    const ThreadSafeHistory& history, const KillerMoves& killers, int ply,
    const std::pair<int, int>& hashMove) {

    std::vector<MoveScore> scoredMoves;

    for (const auto& move : moves) {
        int score = kZero;

        if (move == hashMove) {
            score = kHashMoveScore;
        }

        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            score = getSEEScore(board, move);
        }

        else if (killers.isKiller(ply, move)) {
            score = killers.getKillerScore(ply, move);
        }

        else {
            score = history.get(move.first, move.second);
        }

        score += getPositionalScore(board, move);
        score += getMobilityScore(board, move);
        score += getThreatScore(board, move);
        scoredMoves.emplace_back(move, score);
    }

    return scoredMoves;
}

int EnhancedMoveOrdering::getSEEScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& attacker = board.squares[move.first].piece;
    const Piece& victim = board.squares[move.second].piece;

    if (victim.PieceType == ChessPieceType::NONE) {
        return kZero;
    }

    int score = (victim.PieceValue * kSeeVictimScale) - attacker.PieceValue;

    if (attacker.PieceValue < victim.PieceValue) {
        score += kSeeUndervaluedCaptureBonus;
    }

    int destCol = move.second % kBoardDimension;
    int destRow = move.second / kBoardDimension;
    if ((destCol >= kCenterStart && destCol <= kCenterEnd) &&
        (destRow >= kCenterStart && destRow <= kCenterEnd)) {
        score += kSeeCenterBonus;
    }

    return score;
}

int EnhancedMoveOrdering::getThreatScore(const Board& board, const std::pair<int, int>& move) {
    int score = kZero;
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return kZero;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> opponentMoves = GetAllMoves(tempBoard, tempBoard.turn);

    for (const auto& oppMove : opponentMoves) {
        const Piece& threatenedPiece = board.squares[oppMove.second].piece;
        if (threatenedPiece.PieceType != ChessPieceType::NONE &&
            threatenedPiece.PieceColor == board.turn) {
            score += threatenedPiece.PieceValue / kThreatValueDivisor;
        }
    }

    return score;
}

int EnhancedMoveOrdering::getMobilityScore(const Board& board, const std::pair<int, int>& move) {

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return kZero;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> moves = GetAllMoves(tempBoard, tempBoard.turn);
    return static_cast<int>(moves.size()) * kMobilityScorePerMove;
}

int EnhancedMoveOrdering::getPositionalScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& piece = board.squares[move.first].piece;
    int destCol = move.second % kBoardDimension;
    int destRow = move.second / kBoardDimension;
    int score = kZero;

    switch (piece.PieceType) {
        case ChessPieceType::PAWN:

            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += destRow * kPawnAdvanceBonus;
            } else {
                score += ((kBoardDimension - kOne) - destRow) * kPawnAdvanceBonus;
            }
            break;

        case ChessPieceType::KNIGHT:
        case ChessPieceType::BISHOP:

            if ((destCol >= kCenterStart && destCol <= kCenterEnd) &&
                (destRow >= kCenterStart && destRow <= kCenterEnd)) {
                score += kCenterMinorBonus;
            }
            break;

        case ChessPieceType::ROOK:

            if (destRow == kRookPromotionRank || destRow == kRookDevelopmentRank) {
                score += kRookRankBonus;
            }
            break;

        case ChessPieceType::QUEEN:

            if ((destCol >= kCenterStart && destCol <= kCenterEnd) &&
                (destRow >= kCenterStart && destRow <= kCenterEnd)) {
                score += kCenterQueenBonus;
            }
            break;

        default:
            break;
    }

    return score;
}

TimeManager::TimeManager(const TimeControl& tc)
    : timeControl(tc), moveNumber(kZero), totalMoves(kDefaultTotalMoves) {}

int TimeManager::allocateTime(Board& board, int depth, int nodes, bool isInCheck) {

    int baseTime = calculateBaseTime();
    int increment = calculateIncrement();
    double factor = getTimeFactor(depth, nodes);
    GamePhase phase = getGamePhase(board);
    factor *= getPhaseTimeFactor(phase);

    if (isInCheck) {
        factor *= kInCheckTimeFactor;
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.size() < kLowMobilityMoveThreshold) {
        factor *= kLowMobilityTimeFactor;
    } else if (moves.size() > kHighMobilityMoveThreshold) {
        factor *= kHighMobilityTimeFactor;
    }

    int allocatedTime = static_cast<int>((baseTime + increment) * factor);

    allocatedTime =
        std::max(kMinAllocatedTimeMs, std::min(allocatedTime, baseTime * kMaxBaseTimeMultiplier));

    return allocatedTime;
}

int TimeManager::calculateBaseTime() const {
    if (timeControl.isInfinite) {
        return kInfiniteBaseTimeMs;
    }

    if (timeControl.movesToGo > kZero) {
        return timeControl.baseTime / timeControl.movesToGo;
    }

    int movesRemaining = std::max(kMinMovesRemaining, totalMoves - moveNumber);
    return timeControl.baseTime / movesRemaining;
}

int TimeManager::calculateIncrement() const {
    return timeControl.increment;
}

bool TimeManager::shouldStop(int elapsedTime, int allocatedTime, int depth, int nodes) {
    (void)nodes;

    if (elapsedTime >= allocatedTime) {
        return true;
    }

    if (isEmergencyTime(timeControl.baseTime, allocatedTime)) {
        return true;
    }

    if (depth >= kMaxDepthBeforeStop) {
        return true;
    }

    return false;
}

void TimeManager::updateGameProgress(int moveNumber, int totalMoves) {
    this->moveNumber = moveNumber;
    this->totalMoves = totalMoves;
}

bool TimeManager::isEmergencyTime(int remainingTime, int allocatedTime) {

    return allocatedTime > remainingTime * kEmergencyTimeFraction;
}

double TimeManager::getTimeFactor(int depth, int nodes) {

    double factor = kDefaultTimeFactor;

    if (depth >= kDeepSearchDepth) {
        factor *= kDeepSearchTimeFactor;
    } else if (depth <= kShallowDepthLimit) {
        factor *= kShallowSearchTimeFactor;
    }

    if (nodes > kLargeNodeThreshold) {
        factor *= kLargeNodeTimeFactor;
    }

    return factor;
}

GamePhase TimeManager::getGamePhase(const Board& board) const {
    int totalMaterial = kZero;
    int pieceCount = kZero;
    int queenCount = kZero;
    int rookCount = kZero;
    int minorPieceCount = kZero;

    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            pieceCount++;
            switch (piece.PieceType) {
                case ChessPieceType::QUEEN:
                    totalMaterial += kQueenMaterialValue;
                    queenCount++;
                    break;
                case ChessPieceType::ROOK:
                    totalMaterial += kRookMaterialValue;
                    rookCount++;
                    break;
                case ChessPieceType::BISHOP:
                case ChessPieceType::KNIGHT:
                    totalMaterial += kMinorMaterialValue;
                    minorPieceCount++;
                    break;
                case ChessPieceType::PAWN:
                    totalMaterial += kPawnMaterialValue;
                    break;
                default:
                    break;
            }
        }
    }

    if (totalMaterial > kOpeningMaterialThreshold && queenCount >= kOpeningMinQueenCount &&
        pieceCount > kOpeningPieceCountThreshold) {
        return GamePhase::OPENING;
    }

    if (totalMaterial < kEndgameMaterialThreshold || pieceCount <= kEndgamePieceCountThreshold ||
        (queenCount == kZero && rookCount <= kEndgameRookCountThreshold &&
         minorPieceCount <= kEndgameMinorPieceCountThreshold)) {
        return GamePhase::ENDGAME;
    }

    return GamePhase::MIDDLEGAME;
}

double TimeManager::getPhaseTimeFactor(GamePhase phase) const {
    switch (phase) {
        case GamePhase::OPENING:
            return kOpeningPhaseTimeFactor;
        case GamePhase::MIDDLEGAME:
            return kMiddlegamePhaseTimeFactor;
        case GamePhase::ENDGAME:
            return kEndgamePhaseTimeFactor;
        default:
            return kMiddlegamePhaseTimeFactor;
    }
}

EnhancedOpeningBook::EnhancedOpeningBook(const std::string& bookPath) : bookPath(bookPath) {
    loadBook(bookPath);
}

std::vector<EnhancedOpeningBook::BookEntry> EnhancedOpeningBook::getBookMoves(const Board& board) {
    auto it = book.find(boardToKey(board));
    if (it != book.end()) {
        return it->second;
    }
    return {};
}

std::pair<int, int> EnhancedOpeningBook::getBestMove(const Board& board, bool randomize) {
    auto it = book.find(boardToKey(board));
    if (it != book.end() && !it->second.empty()) {
        const auto& entries = it->second;
            if (randomize) {
                int totalWeight = kZero;
                for (const auto& entry : entries) {
                    totalWeight += entry.weight;
                }
                if (totalWeight > kZero) {
                    static std::random_device rd;
                    static std::mt19937 gen(static_cast<std::mt19937::result_type>(rd()));
                std::uniform_int_distribution<> dis(kZero, totalWeight - kOne);
                int random = dis(gen);
                int cumulative = kZero;
                for (const auto& entry : entries) {
                    cumulative += entry.weight;
                    if (random < cumulative) {
                        return entry.move;
                    }
                }
            }
            return entries[kFirstMoveIndex].move;
        } else {
            auto bestIt =
                std::ranges::max_element(entries, [](const BookEntry& a, const BookEntry& b) {
                    if (a.weight != b.weight) {
                        return a.weight < b.weight;
                    }
                    return a.winRate < b.winRate;
                });
            return bestIt->move;
        }
    }

    std::string fen = getFEN(board);
    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            if (randomize) {
                static std::random_device rd;
                static std::mt19937 gen(static_cast<std::mt19937::result_type>(rd()));
                std::uniform_int_distribution<int> dis(kZero,
                                                       static_cast<int>(options.size() - kOne));
                std::string moveStr = options[dis(gen)];
                return parseMove(moveStr);
            } else {
                return parseMove(options[kFirstMoveIndex]);
            }
        }
    }

    auto legacyIt = OpeningBook.find(fen);
    if (legacyIt != OpeningBook.end()) {
        return parseMove(legacyIt->second);
    }

    return {kInvalidSquare, kInvalidSquare};
}

std::pair<int, int> EnhancedOpeningBook::parseMove(const std::string& move) {

    if (move.length() < kMinBookMoveLength) {
        return {kInvalidSquare, kInvalidSquare};
    }

    int srcCol{move[kMoveSourceFileIndex] - kBookFileOffset};
    int srcRow{move[kMoveSourceRankIndex] - kBookRankOffset};
    int destCol{move[kMoveDestFileIndex] - kBookFileOffset};
    int destRow{move[kMoveDestRankIndex] - kBookRankOffset};

    if (srcCol < kZero || srcCol >= kBoardDimension || srcRow < kZero ||
        srcRow >= kBoardDimension || destCol < kZero || destCol >= kBoardDimension ||
        destRow < kZero || destRow >= kBoardDimension) {
        return {kInvalidSquare, kInvalidSquare};
    }

    return {(srcRow * kBoardDimension) + srcCol, (destRow * kBoardDimension) + destCol};
}

bool EnhancedOpeningBook::isInBook(const Board& board) {
    return book.contains(boardToKey(board));
}

void EnhancedOpeningBook::addMove(const Board& board, const BookEntry& entry) {
    std::string key = boardToKey(board);
    auto it = book.find(key);
    if (it != book.end()) {
        bool found = false;
        for (auto& existingEntry : it->second) {
            if (existingEntry.move == entry.move) {
                existingEntry.weight = std::max(existingEntry.weight, entry.weight);
                const auto existingGames = static_cast<float>(existingEntry.games);
                const auto incomingGames = static_cast<float>(entry.games);
                const float totalGames = existingGames + incomingGames;
                if (totalGames > kZeroFloat) {
                    existingEntry.winRate = ((existingEntry.winRate * existingGames) +
                                             (entry.winRate * incomingGames)) /
                                            totalGames;
                    existingEntry.averageRating = static_cast<int>(
                        ((static_cast<float>(existingEntry.averageRating) * existingGames) +
                         (static_cast<float>(entry.averageRating) * incomingGames)) /
                        totalGames);
                }
                existingEntry.games += entry.games;
                found = true;
                break;
            }
        }
        if (!found) {
            it->second.push_back(entry);
        }
        normalizeWeights(it->second);
    } else {
        book[key] = {entry};
    }
}

void EnhancedOpeningBook::saveBook(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    size_t numPositions = book.size();
    file.write(reinterpret_cast<const char*>(&numPositions), sizeof(numPositions));

    for (const auto& [key, entries] : book) {
        size_t keyLen = key.length();
        file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        file.write(key.c_str(), static_cast<std::streamsize>(keyLen));
        size_t numMoves = entries.size();
        file.write(reinterpret_cast<const char*>(&numMoves), sizeof(numMoves));

        for (const auto& entry : entries) {
            file.write(reinterpret_cast<const char*>(&entry.move.first), sizeof(entry.move.first));
            file.write(reinterpret_cast<const char*>(&entry.move.second),
                       sizeof(entry.move.second));
            file.write(reinterpret_cast<const char*>(&entry.weight), sizeof(entry.weight));
            file.write(reinterpret_cast<const char*>(&entry.games), sizeof(entry.games));
            file.write(reinterpret_cast<const char*>(&entry.winRate), sizeof(entry.winRate));
            file.write(reinterpret_cast<const char*>(&entry.averageRating),
                       sizeof(entry.averageRating));
        }
    }

    file.close();
}

void EnhancedOpeningBook::loadBook(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    book.clear();
    size_t numPositions{static_cast<size_t>(kZero)};
    file.read(reinterpret_cast<char*>(&numPositions), sizeof(numPositions));

    for (auto i = static_cast<size_t>(kZero); i < numPositions; ++i) {
        auto keyLen = static_cast<size_t>(kZero);
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        std::string key(keyLen, '\0');
        file.read(key.data(), static_cast<std::streamsize>(keyLen));
        auto numMoves = static_cast<size_t>(kZero);
        file.read(reinterpret_cast<char*>(&numMoves), sizeof(numMoves));
        std::vector<BookEntry> entries;
        entries.reserve(numMoves);

        for (auto j = static_cast<size_t>(kZero); j < numMoves; ++j) {
            BookEntry entry;
            file.read(reinterpret_cast<char*>(&entry.move.first), sizeof(entry.move.first));
            file.read(reinterpret_cast<char*>(&entry.move.second), sizeof(entry.move.second));
            file.read(reinterpret_cast<char*>(&entry.weight), sizeof(entry.weight));
            file.read(reinterpret_cast<char*>(&entry.games), sizeof(entry.games));
            file.read(reinterpret_cast<char*>(&entry.winRate), sizeof(entry.winRate));
            file.read(reinterpret_cast<char*>(&entry.averageRating), sizeof(entry.averageRating));
            entries.push_back(entry);
        }

        book[key] = entries;
    }

    file.close();
    bookPath = path;
}

std::string EnhancedOpeningBook::boardToKey(const Board& board) {
    std::string fen = getFEN(board);
    std::istringstream iss(fen);
    std::string position;
    std::string activeColor;
    std::string castling;
    std::string enPassant;
    iss >> position >> activeColor >> castling >> enPassant;
    std::ostringstream keyStream;
    keyStream << position << " " << activeColor << " " << castling << " " << enPassant;
    return keyStream.str();
}

void EnhancedOpeningBook::normalizeWeights(std::vector<BookEntry>& entries) {
    if (entries.empty()) {
        return;
    }

    int totalWeight = kZero;
    for (const auto& entry : entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight == kZero) {
        for (auto& entry : entries) {
            entry.weight = entry.games > kZero ? entry.games : kFallbackWeight;
        }
        totalWeight = kZero;
        for (const auto& entry : entries) {
            totalWeight += entry.weight;
        }
    }

    if (totalWeight > kZero) {
        for (auto& entry : entries) {
            entry.weight = (entry.weight * kNormalizedWeightTotal) / totalWeight;
        }
    }
}

EnhancedOpeningBook::BookStats EnhancedOpeningBook::getStats() const {
    BookStats stats{};
    stats.totalPositions = book.size();
    stats.totalMoves = kZero;
    stats.totalGames = kZero;
    float totalWinRate = kZeroFloat;
    float totalRating = kZeroFloat;
    int moveCount = kZero;

    for (const auto& [key, entries] : book) {
        stats.totalMoves += entries.size();
        for (const auto& entry : entries) {
            stats.totalGames += entry.games;
            totalWinRate += entry.winRate;
            totalRating += static_cast<float>(entry.averageRating);
            moveCount++;
        }
    }

    if (moveCount > kZero) {
        stats.averageWinRate = totalWinRate / static_cast<float>(moveCount);
        stats.averageRating = totalRating / static_cast<float>(moveCount);
    }

    return stats;
}

void EnhancedOpeningBook::analyzeBook() const {
    (void)getStats();
}
