#pragma once

#include <compare>

#include "../core/SquareSentinel.h"
#include "AdvancedSearch.h"
#include "LMR.h"
#include "ZobristKeys.h"
#include "search.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <span>
#include <thread>

namespace SearchInternal {
inline constexpr int kZero = SearchConstants::kZero;
inline constexpr int kOne = SearchConstants::kOne;
inline constexpr int kTwo = SearchConstants::kTwo;
inline constexpr int kThree = 3;
inline constexpr int kFour = 4;
inline constexpr int kNegativeOne = SearchConstants::kInvalidSquare;
inline constexpr int kWhiteIndex = SearchConstants::kZero;
inline constexpr int kBlackIndex = SearchConstants::kOne;
inline constexpr int kExactFlag = SearchConstants::kZero;
inline constexpr int kUpperBoundFlag = SearchConstants::kInvalidSquare;
inline constexpr int kLowerBoundFlag = SearchConstants::kOne;
inline constexpr int kMvvLvaVictimPawn = 50;
inline constexpr int kMvvLvaVictimKnight = 52;
inline constexpr int kMvvLvaVictimBishop = 54;
inline constexpr int kMvvLvaVictimRook = 53;

inline constexpr int kBoardDimension = 8;
inline constexpr int kBoardSquareCount = kBoardDimension * kBoardDimension;
inline constexpr int kPieceTypePerColorCount = 6;
inline constexpr int kInvalidSquare = chess::kInvalidSquare;

inline constexpr int kKillerBaseScore = 5000;
inline constexpr int kKillerSlotPenalty = 100;
inline constexpr int kDefaultThreadFallback = 4;

inline constexpr int kPawnValue = 100;
inline constexpr int kMinorPieceValue = 300;
inline constexpr int kRookValue = 500;
inline constexpr int kQueenValue = 975;
inline constexpr int kKingValue = 10000;

inline constexpr int kCounterMoveQuietBonus = 12000;
inline constexpr int kMvvLvaScale = 1000;
inline constexpr int kCaptureHistoryDivisor = 8;
inline constexpr int kContinuationHistoryDivisor = 4;
inline constexpr int kHistoryMaxScore = 16384;
inline constexpr int kGoodSeeCaptureBonus = 120000;
inline constexpr int kBadSeeCapturePenalty = 120000;
inline constexpr int kPromotionQuietBonus = 15000;

inline constexpr int kMaxSearchPly = 50;
inline constexpr int kMaxPvDepth = 20;
inline constexpr int kMateScore = 10000;
inline constexpr int kMateScoreTtThreshold = kMateScore - kMaxSearchPly;
inline constexpr int kSyzygyWinScore = 9000;
inline constexpr int kMultiCutReductionMoves = 3;
inline constexpr int kSingularDepthThreshold = 8;
inline constexpr int kSingularTtDepthMargin = 3;
inline constexpr int kSingularMarginPerDepth = 2;
inline constexpr int kSingularReducedDepthDivisor = 2;
inline constexpr int kExtensionPlyLimit = 16;
inline constexpr int kLateMoveCountThreshold = 2;
inline constexpr int kLmrEndgamePhaseThreshold = 10;
inline constexpr int kCounterMoveArrayLimit = kBoardSquareCount;

inline constexpr int kSeePruningDepthLimit = 6;
inline constexpr int kSeePruningScale = 80;
inline constexpr int kHistoryPruningDepthLimit = 4;
inline constexpr int kHistoryPruningBaseThreshold = -1000;
inline constexpr int kQuietHistoryPruningMoveThreshold = 0;
inline constexpr int kFutilityDepthLimit = 3;
inline constexpr int kFutilityMarginDepth1 = 200;
inline constexpr int kFutilityMarginDepth2 = 400;
inline constexpr int kFutilityMarginDepth3 = 600;

inline constexpr int kNullMoveMinDepth = 3;
inline constexpr int kNullMoveMaxDepth = 10;
inline constexpr int kNullMovePlyLimit = 30;
inline constexpr int kNullMoveBaseReduction = 2;
inline constexpr int kNullMoveDepthDivisor = 4;
inline constexpr int kNullMoveEvalDivisor = 200;
inline constexpr int kNullMoveMaxReduction = 5;
inline constexpr int kLateDepthReductionThreshold = 4;

inline constexpr int kAspirationWindow = 50;
inline constexpr int kMaxSearchAttempts = 4;
inline constexpr int kEarlyDepthNoAspirationLimit = 3;
inline constexpr int kTimeCheckInterval = 1024;
inline constexpr int kTimeCheckMask = kTimeCheckInterval - 1;
inline constexpr long long kNpsMillisScaleLl = 1000LL;
inline constexpr int kStabilityChangeThreshold = 2;
inline constexpr int kStabilityLowThreshold = 2;
inline constexpr int kStabilityHighThreshold = 4;
inline constexpr double kUnstableStabilityFactor = 1.5;
inline constexpr double kStableStabilityFactor = 0.6;
inline constexpr int kScoreInstabilityDepth = 5;
inline constexpr int kScoreRiseThreshold = 50;
inline constexpr int kScoreDropThreshold = 20;
inline constexpr double kScoreRiseFactor = 1.5;
inline constexpr double kScoreDropFactor = 0.7;
inline constexpr int kSoftLimitDepthThreshold = 5;

inline constexpr int kSeeThresholdBase = -25;
inline constexpr int kSeeThresholdPerPly = 15;
inline constexpr int kDeltaPruneMargin = 150;
inline constexpr int kBigDeltaMargin = 1200;
inline constexpr int kSeeCaptureBonus = 50;
inline constexpr int kWinningSeeBonus = 200;
inline constexpr int kCaptureVictimScale = 100;
inline constexpr int kQueenGamePhaseIncrement = 4;
inline constexpr int kFutilityMarginTableSize = 4;
inline constexpr int kSeeMaxExchangeDepth = 32;
inline constexpr Bitboard kFileAMask = 0x0101010101010101ULL;
inline constexpr Bitboard kFileHMask = 0x8080808080808080ULL;
inline constexpr Bitboard kNotFileA = ~kFileAMask;
inline constexpr Bitboard kNotFileH = ~kFileHMask;
inline constexpr int kCastlingDistance = 2;
inline constexpr int kKingsideRookOffset = 3;
inline constexpr int kKingsideRookTargetOffset = 1;
inline constexpr int kQueensideRookOffset = 4;
inline constexpr int kQueensideRookTargetOffset = 1;
inline constexpr int kWhiteQueensideRookSquare = 0;
inline constexpr int kWhiteKingsideRookSquare = 7;
inline constexpr int kBlackQueensideRookSquare = 56;
inline constexpr int kBlackKingsideRookSquare = 63;
inline constexpr int kPromotionWhiteRank = 7;
inline constexpr int kPromotionBlackRank = 0;
inline constexpr int kPositionalScoreDivisor = 10;

inline constexpr int kContinuationPlyLimit = 64;
inline constexpr int kZeroWindowOffset = 1;
inline constexpr int kFastEvalDepthThreshold = 6;
inline constexpr int kFastEvalQuiescencePlyThreshold = 2;
inline constexpr std::uint64_t kUnsetZobristKey = std::numeric_limits<std::uint64_t>::max();
inline constexpr int kZobristCastlingStateCount = 16;
inline constexpr int kNoEpSquare = chess::kNoEnPassantSquare;
inline constexpr int kMoveUndoMaxSquares = 5;
inline constexpr std::size_t kRootSplitThreadStackBytes = 8ULL * 1024ULL * 1024ULL;
inline constexpr int kRootSplitMinDepth = 4;
inline constexpr int kRootSplitMinMoves = 3;
inline constexpr int kRootSplitLowDepthThreshold = 6;
inline constexpr int kRootSplitLowDepthMaxWorkers = 2;
inline constexpr int kRootSplitMaxAspirationSplitAttempt = 0;

struct ScoredMove {
    Move move;
    int score;
    ScoredMove(Move m, int s) : move(m), score(s) {}
    std::strong_ordering operator<=>(const ScoredMove& other) const {
        if (const auto scoreCmp = score <=> other.score; scoreCmp != 0) {
            return scoreCmp;
        }
        return move <=> other.move;
    }
    bool operator==(const ScoredMove& other) const = default;
};

struct MoveApplicationData {
    Piece movingPiece;
    Piece capturedPiece;
    int captureSquare = kInvalidSquare;
    bool wasCastling = false;
    int rookFrom = kInvalidSquare;
    int rookTo = kInvalidSquare;
    std::array<int, kMoveUndoMaxSquares> changedSquares{};
    std::array<Piece, kMoveUndoMaxSquares> previousPieces{};
    int changedSquareCount = kZero;
    ChessPieceColor previousTurn = ChessPieceColor::WHITE;
    std::uint8_t previousCastlingRights = 0;
    chess::EnPassantSquare previousEnPassantSquare{};
    int previousHalfmoveClock = 0;
    int previousFullmoveNumber = 1;
    bool previousWhiteChecked = false;
    bool previousBlackChecked = false;
    int previousLastMove = kZero;
    ChessTimePoint previousLastMoveTime;
    Bitboard previousWhitePawns = EMPTY;
    Bitboard previousWhiteKnights = EMPTY;
    Bitboard previousWhiteBishops = EMPTY;
    Bitboard previousWhiteRooks = EMPTY;
    Bitboard previousWhiteQueens = EMPTY;
    Bitboard previousWhiteKings = EMPTY;
    Bitboard previousBlackPawns = EMPTY;
    Bitboard previousBlackKnights = EMPTY;
    Bitboard previousBlackBishops = EMPTY;
    Bitboard previousBlackRooks = EMPTY;
    Bitboard previousBlackQueens = EMPTY;
    Bitboard previousBlackKings = EMPTY;
    Bitboard previousWhitePieces = EMPTY;
    Bitboard previousBlackPieces = EMPTY;
    Bitboard previousAllPieces = EMPTY;
};

class ScopedFastEvalMode {
public:
    explicit ScopedFastEvalMode(bool enableFast) : previousMode(isFastEvaluationMode()) {
        setFastEvaluationMode(enableFast);
    }

    ~ScopedFastEvalMode() {
        setFastEvaluationMode(previousMode);
    }

private:
    bool previousMode;
};

struct BitboardMoveState {
    Bitboard pieces[2][kPieceTypePerColorCount]{};
    Bitboard occupancy = EMPTY;
    int movingColor = kWhiteIndex;
    int movedPiece = kInvalidSquare;
    int capturedPiece = kInvalidSquare;
};

struct RootSplitResult {
    int score = kZero;
    Move bestMove = {kInvalidSquare, kInvalidSquare};
    int nodes = kZero;
    bool hasLegalMove = false;
    bool timeExpired = false;
};

std::vector<ScoredMove> scoreMovesOptimized(
    const Board& board, std::span<const Move> moves, const ThreadSafeHistory& historyTable,
    const KillerMoves& killerMoves, int ply,
    const Move& ttMove = {SearchConstants::kInvalidSquare, SearchConstants::kInvalidSquare},
    const Move& counterMove = {SearchConstants::kInvalidSquare, SearchConstants::kInvalidSquare},
    const ParallelSearchContext* context = nullptr);

enum class MovePickerStage : std::uint8_t {
    HASH_MOVE,
    GEN_CAPTURES,
    GOOD_CAPTURES,
    KILLERS,
    COUNTERMOVE,
    GEN_QUIETS,
    QUIETS,
    BAD_CAPTURES,
    DONE
};

class MovePicker {
    Board& board;
    Move hashMove;
    const KillerMoves& killerMoves;
    int ply;
    const ThreadSafeHistory& history;
    ParallelSearchContext& context;
    MovePickerStage stage;
    std::vector<ScoredMove> goodCaptures;
    std::vector<ScoredMove> badCaptures;
    std::vector<ScoredMove> quietMoves;
    size_t captureIdx = 0;
    size_t quietIdx = 0;
    size_t badCaptureIdx = 0;
    bool movesGenerated = false;
    static constexpr std::size_t MOVE_MASK_SIZE = SearchConstants::kMoveMaskSize;
    std::array<bool, MOVE_MASK_SIZE> availableMask{};
    std::array<bool, MOVE_MASK_SIZE> returnedMask{};

    static constexpr std::size_t moveIndex(const Move& m) {
        return (static_cast<std::size_t>(m.first) *
                static_cast<std::size_t>(SearchConstants::kMoveMaskDimension)) +
               static_cast<std::size_t>(m.second);
    }

    bool alreadyReturned(const Move& m) const {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return false;
        }
        return returnedMask[moveIndex(m)];
    }

    bool isAvailable(const Move& m) const {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return false;
        }
        return availableMask[moveIndex(m)];
    }

    void markAvailable(const Move& m) {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return;
        }
        availableMask[moveIndex(m)] = true;
    }

    void markReturned(const Move& m) {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return;
        }
        returnedMask[moveIndex(m)] = true;
    }

    void generateAndPartitionMoves();

public:
    MovePicker(Board& b, Move hm, const KillerMoves& km, int p, const ThreadSafeHistory& h,
               ParallelSearchContext& ctx);

    Move next();
};

namespace EnhancedMoveOrdering {
inline constexpr int HASH_MOVE_SCORE = 1000000;
inline constexpr int CAPTURE_SCORE_BASE = 900000;
inline constexpr int KILLER_SCORE = 800000;
inline constexpr int HISTORY_SCORE_BASE = 0;
inline constexpr int QUIET_SCORE_BASE = -1000000;

using MvvLvaTable = std::array<std::array<int, kPieceTypePerColorCount>, kPieceTypePerColorCount>;
extern const MvvLvaTable MVV_LVA_SCORES;

int getMVVLVA_Score(const Board& board, int fromSquare, int toSquare);
int getHistoryScore(const ThreadSafeHistory& history, int fromSquare, int toSquare);
int getKillerScore(const KillerMoves& killers, int ply, int fromSquare, int toSquare);
int getPositionalScore(const Board& board, int fromSquare, int toSquare);
} // namespace EnhancedMoveOrdering

struct RootSplitSharedState {
    const Board* board = nullptr;
    const std::vector<ScoredMove>* rootMoves = nullptr;
    uint64_t rootZobristKey = kUnsetZobristKey;
    int depth = kZero;
    bool maximizingPlayer = true;
    int hashSizeMb = SearchConstants::kDefaultTranspositionTableMb;
    ThreadSafeHistory historySeed;
    std::vector<uint64_t> repetitionHistory;
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs = kZero;
    int contempt = kZero;
    int optimalTimeMs = kZero;
    int maxTimeMs = kZero;
    bool useSyzygy = false;
    std::atomic<int> nextMoveIndex{0};
    std::atomic<long long> nodes{0};
    std::atomic<bool> stop{false};
    std::atomic<bool> timeExpired{false};
    std::mutex bestMutex;
    int alpha = -kMateScore;
    int beta = kMateScore;
    int bestScore = kZero;
    Move bestMove = {kInvalidSquare, kInvalidSquare};
    bool hasBestMove = false;
};

void recordUndoSquare(MoveApplicationData& moveData, const Board& board, int square);
void initializeBitboardsFromBoard(const Board& board, Bitboard pieces[2][kPieceTypePerColorCount],
                                  Bitboard& occupancy);
int pieceValueFromIndex(int pieceIndex);
Bitboard attackersToSquare(const Bitboard pieces[2][kPieceTypePerColorCount], Bitboard occupancy,
                           int targetSquare, int colorIndex);
bool popLeastValuableAttacker(const Bitboard pieces[2][kPieceTypePerColorCount], Bitboard attackers,
                              int colorIndex, int& attackerSquareOut, int& attackerPieceOut);
int computeGamePhase(const Board& board);
bool applyMoveToBitboards(const Board& board, int fromSquare, int toSquare, bool autoPromoteToQueen,
                          BitboardMoveState& state);
uint64_t resolveZobristKey(const Board& board, uint64_t key);
int encodeCastlingState(const Board& board);
uint64_t computeChildZobrist(uint64_t parentKey, const Board& childBoard, int fromSquare,
                             int toSquare, const MoveApplicationData& moveData);
bool applySearchMoveWithData(Board& board, int fromSquare, int toSquare, bool autoPromoteToQueen,
                             MoveApplicationData* moveDataOut = nullptr);
void undoSearchMoveWithData(Board& board, const MoveApplicationData& moveData);
bool hasNonPawnMaterial(const Board& board, ChessPieceColor side);
int computeNullMoveReduction(int depth, int evalMargin);
int toTtScore(int score, int ply);
int fromTtScore(int score, int ply);
void storeTtEntry(ParallelSearchContext& context, uint64_t key, int depth, int value, int flag,
                  const Move& bestMove, int ply);
int getPieceValue(ChessPieceType pieceType);
bool isBetterScoredMove(const ScoredMove& lhs, const ScoredMove& rhs);
const ScoredMove& pickNextScoredMove(std::vector<ScoredMove>& scoredMoves, std::size_t nextIndex);
bool moveMatches(const Move& lhs, const Move& rhs);
bool moveExistsInList(std::span<const Move> moves, const Move& candidate);
int chooseRootSplitWorkerCount(int depth, int numThreads, int remainingMoves);
bool isPreferredRootMove(Move lhs, Move rhs);
bool checkRootSplitTimeLimit(RootSplitSharedState& shared);
bool evaluateRootSplitMove(const RootSplitSharedState& shared, Move move, int alphaWindow,
                           int betaWindow, int& evalOut, int& nodesOut);
void commitRootSplitResult(RootSplitSharedState& shared, int eval, Move move);
void rootSplitWorker(RootSplitSharedState& shared);
RootSplitResult searchRootMovesYBWC(const Board& board, int depth, int alpha, int beta,
                                    bool maximizingPlayer, const ThreadSafeHistory& historyTable,
                                    const ParallelSearchContext& context, int numThreads,
                                    int hashSizeMb);
int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare);
bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
void updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth);
bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs);
void penalizeHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth);
bool countNodeAndCheckTime(ParallelSearchContext& context);
} // namespace SearchInternal
