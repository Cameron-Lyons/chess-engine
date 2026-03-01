#include "search.h"
#include "../ai/SyzygyTablebase.h"
#include "../utils/engine_globals.h"
#include "AdvancedSearch.h"
#include "BookUtils.h"
#include "LazySMP.h"
#include "LMR.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <limits>
#include <mutex>
#include <random>
#include <ranges>
#include <sstream>
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

constexpr int kCounterMoveQuietBonus = 12000;
constexpr int kMvvLvaScale = 1000;
constexpr int kCaptureHistoryDivisor = 8;
constexpr int kContinuationHistoryDivisor = 4;
constexpr int kHistoryMaxScore = 16384;
constexpr int kGoodSeeCaptureBonus = 120000;
constexpr int kBadSeeCapturePenalty = 120000;
constexpr int kPromotionQuietBonus = 15000;

constexpr int kMaxSearchPly = 50;
constexpr int kMaxPvDepth = 20;
constexpr int kMateScore = 10000;
constexpr int kMateScoreTtThreshold = kMateScore - kMaxSearchPly;
constexpr int kSyzygyWinScore = 9000;
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
constexpr int kNullMoveBaseReduction = 2;
constexpr int kNullMoveDepthDivisor = 4;
constexpr int kNullMoveEvalDivisor = 200;
constexpr int kNullMoveMaxReduction = 5;
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
constexpr int kSeeMaxExchangeDepth = 32;
constexpr Bitboard kFileAMask = 0x0101010101010101ULL;
constexpr Bitboard kFileHMask = 0x8080808080808080ULL;
constexpr Bitboard kNotFileA = ~kFileAMask;
constexpr Bitboard kNotFileH = ~kFileHMask;
constexpr int kCastlingDistance = 2;
constexpr int kKingsideRookOffset = 3;
constexpr int kKingsideRookTargetOffset = 1;
constexpr int kQueensideRookOffset = 4;
constexpr int kQueensideRookTargetOffset = 1;
constexpr int kWhiteQueensideRookSquare = 0;
constexpr int kWhiteKingsideRookSquare = 7;
constexpr int kBlackQueensideRookSquare = 56;
constexpr int kBlackKingsideRookSquare = 63;
constexpr int kPromotionWhiteRank = 7;
constexpr int kPromotionBlackRank = 0;
constexpr int kPositionalScoreDivisor = 10;

constexpr int kContinuationPlyLimit = 64;
constexpr int kZeroWindowOffset = 1;
constexpr int kFastEvalDepthThreshold = 6;
constexpr int kFastEvalQuiescencePlyThreshold = 2;
constexpr std::uint64_t kUnsetZobristKey = std::numeric_limits<std::uint64_t>::max();
constexpr int kZobristCastlingStateCount = 4;
constexpr int kNoEpSquare = -1;
constexpr int kMoveUndoMaxSquares = 5;

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
    bool previousWhiteCanCastle = false;
    bool previousBlackCanCastle = false;
    int previousEnPassantSquare = kNoEpSquare;
    bool previousWhiteChecked = false;
    bool previousBlackChecked = false;
    int previousLastMove = kZero;
    ChessTimePoint previousLastMoveTime{};
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

void recordUndoSquare(MoveApplicationData& moveData, const Board& board, int square) {
    if (square < kZero || square >= kBoardSquareCount) {
        return;
    }

    for (int i = kZero; i < moveData.changedSquareCount; ++i) {
        if (moveData.changedSquares[static_cast<std::size_t>(i)] == square) {
            return;
        }
    }

    if (moveData.changedSquareCount >= kMoveUndoMaxSquares) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(moveData.changedSquareCount);
    moveData.changedSquares[index] = square;
    moveData.previousPieces[index] = board.squares[square].piece;
    moveData.changedSquareCount++;
}

void undoSearchMoveWithData(Board& board, const MoveApplicationData& moveData);

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

void initializeBitboardsFromBoard(const Board& board, Bitboard pieces[2][kPieceTypePerColorCount],
                                  Bitboard& occupancy) {
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::PAWN)] = board.whitePawns;
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::KNIGHT)] = board.whiteKnights;
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::BISHOP)] = board.whiteBishops;
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::ROOK)] = board.whiteRooks;
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::QUEEN)] = board.whiteQueens;
    pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::KING)] = board.whiteKings;

    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::PAWN)] = board.blackPawns;
    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::KNIGHT)] = board.blackKnights;
    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::BISHOP)] = board.blackBishops;
    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::ROOK)] = board.blackRooks;
    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::QUEEN)] = board.blackQueens;
    pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::KING)] = board.blackKings;

    occupancy = board.allPieces;
}

int pieceValueFromIndex(int pieceIndex) {
    if (pieceIndex < kZero || pieceIndex >= kPieceTypePerColorCount) {
        return kZero;
    }
    return Piece::getPieceValue(static_cast<ChessPieceType>(pieceIndex));
}

Bitboard attackersToSquare(const Bitboard pieces[2][kPieceTypePerColorCount], Bitboard occupancy,
                           int targetSquare, int colorIndex) {
    if (targetSquare < kZero || targetSquare >= kBoardSquareCount || colorIndex < kWhiteIndex ||
        colorIndex > kBlackIndex) {
        return EMPTY;
    }

    const Bitboard target = (1ULL << targetSquare);
    Bitboard attackers = EMPTY;

    if (colorIndex == kWhiteIndex) {
        attackers |= (((target >> 7) & kNotFileA) | ((target >> 9) & kNotFileH)) &
                     pieces[kWhiteIndex][pieceTypeIndex(ChessPieceType::PAWN)];
    } else {
        attackers |= (((target << 7) & kNotFileH) | ((target << 9) & kNotFileA)) &
                     pieces[kBlackIndex][pieceTypeIndex(ChessPieceType::PAWN)];
    }

    attackers |= KnightAttacks[targetSquare] & pieces[colorIndex][pieceTypeIndex(ChessPieceType::KNIGHT)];
    attackers |= KingAttacks[targetSquare] & pieces[colorIndex][pieceTypeIndex(ChessPieceType::KING)];
    attackers |= fastBishopAttacks(targetSquare, occupancy) &
                 (pieces[colorIndex][pieceTypeIndex(ChessPieceType::BISHOP)] |
                  pieces[colorIndex][pieceTypeIndex(ChessPieceType::QUEEN)]);
    attackers |= fastRookAttacks(targetSquare, occupancy) &
                 (pieces[colorIndex][pieceTypeIndex(ChessPieceType::ROOK)] |
                  pieces[colorIndex][pieceTypeIndex(ChessPieceType::QUEEN)]);
    return attackers;
}

bool popLeastValuableAttacker(const Bitboard pieces[2][kPieceTypePerColorCount], Bitboard attackers,
                              int colorIndex, int& attackerSquareOut, int& attackerPieceOut) {
    for (int piece = pieceTypeIndex(ChessPieceType::PAWN);
         piece <= pieceTypeIndex(ChessPieceType::KING); ++piece) {
        Bitboard candidates = attackers & pieces[colorIndex][piece];
        if (candidates != EMPTY) {
            attackerSquareOut = lsb(candidates);
            attackerPieceOut = piece;
            return attackerSquareOut >= kZero;
        }
    }
    return false;
}

int computeGamePhase(const Board& board) {
    const int minorCount =
        popcount(board.whiteKnights | board.blackKnights) +
        popcount(board.whiteBishops | board.blackBishops);
    const int rookCount = popcount(board.whiteRooks | board.blackRooks);
    const int queenCount = popcount(board.whiteQueens | board.blackQueens);
    return minorCount + (kTwo * rookCount) + (kQueenGamePhaseIncrement * queenCount);
}

bool applyMoveToBitboards(const Board& board, int fromSquare, int toSquare, bool autoPromoteToQueen,
                          BitboardMoveState& state) {
    if (fromSquare < kZero || fromSquare >= kBoardSquareCount || toSquare < kZero ||
        toSquare >= kBoardSquareCount) {
        return false;
    }

    initializeBitboardsFromBoard(board, state.pieces, state.occupancy);

    const Piece& movingPiece = board.squares[fromSquare].piece;
    if (movingPiece.PieceType == ChessPieceType::NONE) {
        return false;
    }

    state.movingColor = (movingPiece.PieceColor == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    const int enemyColor = (state.movingColor == kWhiteIndex) ? kBlackIndex : kWhiteIndex;
    const int movingPieceIndex = pieceTypeIndex(movingPiece.PieceType);
    if (movingPieceIndex < kZero) {
        return false;
    }

    clear_bit(state.pieces[state.movingColor][movingPieceIndex], fromSquare);
    clear_bit(state.occupancy, fromSquare);

    state.capturedPiece = kInvalidSquare;
    bool isEnPassant =
        (movingPiece.PieceType == ChessPieceType::PAWN && board.enPassantSquare == toSquare &&
         board.squares[toSquare].piece.PieceType == ChessPieceType::NONE &&
         std::abs((toSquare % kBoardDimension) - (fromSquare % kBoardDimension)) == kOne);
    if (isEnPassant) {
        int captureSquare = (movingPiece.PieceColor == ChessPieceColor::WHITE)
                                ? (toSquare - kBoardDimension)
                                : (toSquare + kBoardDimension);
        if (captureSquare < kZero || captureSquare >= kBoardSquareCount) {
            return false;
        }
        const int capturedPieceIndex = pieceTypeIndex(board.squares[captureSquare].piece.PieceType);
        if (capturedPieceIndex < kZero) {
            return false;
        }
        state.capturedPiece = capturedPieceIndex;
        clear_bit(state.pieces[enemyColor][capturedPieceIndex], captureSquare);
        clear_bit(state.occupancy, captureSquare);
    } else {
        const Piece& capturedPiece = board.squares[toSquare].piece;
        const int capturedPieceIndex = pieceTypeIndex(capturedPiece.PieceType);
        if (capturedPieceIndex >= kZero) {
            const int capturedColor =
                (capturedPiece.PieceColor == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
            state.capturedPiece = capturedPieceIndex;
            clear_bit(state.pieces[capturedColor][capturedPieceIndex], toSquare);
            clear_bit(state.occupancy, toSquare);
        }
    }

    int movedPieceIndex = movingPieceIndex;
    const int destinationRow = toSquare / kBoardDimension;
    const bool isPromotion =
        movingPiece.PieceType == ChessPieceType::PAWN &&
        ((movingPiece.PieceColor == ChessPieceColor::WHITE && destinationRow == kPromotionWhiteRank) ||
         (movingPiece.PieceColor == ChessPieceColor::BLACK && destinationRow == kPromotionBlackRank));
    if (autoPromoteToQueen && isPromotion) {
        movedPieceIndex = pieceTypeIndex(ChessPieceType::QUEEN);
    }

    set_bit(state.pieces[state.movingColor][movedPieceIndex], toSquare);
    set_bit(state.occupancy, toSquare);

    if (movingPiece.PieceType == ChessPieceType::KING &&
        std::abs(toSquare - fromSquare) == kCastlingDistance) {
        const int rookFrom =
            (toSquare > fromSquare) ? (fromSquare + kKingsideRookOffset)
                                    : (fromSquare - kQueensideRookOffset);
        const int rookTo =
            (toSquare > fromSquare) ? (fromSquare + kKingsideRookTargetOffset)
                                    : (fromSquare - kQueensideRookTargetOffset);
        const int rookIndex = pieceTypeIndex(ChessPieceType::ROOK);
        clear_bit(state.pieces[state.movingColor][rookIndex], rookFrom);
        set_bit(state.pieces[state.movingColor][rookIndex], rookTo);
        clear_bit(state.occupancy, rookFrom);
        set_bit(state.occupancy, rookTo);
    }

    state.movedPiece = movedPieceIndex;
    return true;
}

uint64_t resolveZobristKey(const Board& board, uint64_t key) {
    return (key == kUnsetZobristKey) ? ComputeZobrist(board) : key;
}

int encodeCastlingState(const Board& board) {
    return (board.whiteCanCastle ? kOne : kZero) | (board.blackCanCastle ? kTwo : kZero);
}

uint64_t computeChildZobrist(uint64_t parentKey, const Board& childBoard,
                             int fromSquare, int toSquare, const MoveApplicationData& moveData) {
    uint64_t childKey = parentKey;
    int movingIdx = pieceToZobristIndex(moveData.movingPiece);
    if (movingIdx >= kZero) {
        childKey ^= ZobristTable[fromSquare][movingIdx];
    }
    int movedIdx = pieceToZobristIndex(childBoard.squares[toSquare].piece);
    if (movedIdx >= kZero) {
        childKey ^= ZobristTable[toSquare][movedIdx];
    }

    if (moveData.captureSquare >= kZero && moveData.captureSquare < kBoardSquareCount) {
        int capturedIdx = pieceToZobristIndex(moveData.capturedPiece);
        if (capturedIdx >= kZero) {
            childKey ^= ZobristTable[moveData.captureSquare][capturedIdx];
        }
    }

    if (moveData.wasCastling && moveData.rookFrom >= kZero && moveData.rookTo >= kZero) {
        int rookIdx = pieceToZobristIndex(Piece(moveData.movingPiece.PieceColor, ChessPieceType::ROOK));
        if (rookIdx >= kZero) {
            childKey ^= ZobristTable[moveData.rookFrom][rookIdx];
            childKey ^= ZobristTable[moveData.rookTo][rookIdx];
        }
    }

    const int parentCastle = (moveData.previousWhiteCanCastle ? kOne : kZero) |
                             (moveData.previousBlackCanCastle ? kTwo : kZero);
    const int childCastle = encodeCastlingState(childBoard);
    childKey ^= ZobristCastling[parentCastle];
    childKey ^= ZobristCastling[childCastle];

    if (moveData.previousEnPassantSquare >= kZero &&
        moveData.previousEnPassantSquare < kBoardSquareCount) {
        childKey ^= ZobristEnPassant[moveData.previousEnPassantSquare];
    }
    if (childBoard.enPassantSquare >= kZero && childBoard.enPassantSquare < kBoardSquareCount) {
        childKey ^= ZobristEnPassant[childBoard.enPassantSquare];
    }

    childKey ^= ZobristBlackToMove;
    return childKey;
}

bool applySearchMoveWithData(Board& board, int fromSquare, int toSquare, bool autoPromoteToQueen,
                             MoveApplicationData* moveDataOut) {
    if (fromSquare < kZero || fromSquare >= kBoardSquareCount || toSquare < kZero ||
        toSquare >= kBoardSquareCount) {
        return false;
    }

    const Piece movingPiece = board.squares[fromSquare].piece;
    if (movingPiece.PieceType == ChessPieceType::NONE) {
        return false;
    }

    MoveApplicationData moveData{};
    moveData.movingPiece = movingPiece;
    moveData.capturedPiece = board.squares[toSquare].piece;
    moveData.previousTurn = board.turn;
    moveData.previousWhiteCanCastle = board.whiteCanCastle;
    moveData.previousBlackCanCastle = board.blackCanCastle;
    moveData.previousEnPassantSquare = board.enPassantSquare;
    moveData.previousWhiteChecked = board.whiteChecked;
    moveData.previousBlackChecked = board.blackChecked;
    moveData.previousLastMove = board.LastMove;
    moveData.previousLastMoveTime = board.lastMoveTime;
    moveData.previousWhitePawns = board.whitePawns;
    moveData.previousWhiteKnights = board.whiteKnights;
    moveData.previousWhiteBishops = board.whiteBishops;
    moveData.previousWhiteRooks = board.whiteRooks;
    moveData.previousWhiteQueens = board.whiteQueens;
    moveData.previousWhiteKings = board.whiteKings;
    moveData.previousBlackPawns = board.blackPawns;
    moveData.previousBlackKnights = board.blackKnights;
    moveData.previousBlackBishops = board.blackBishops;
    moveData.previousBlackRooks = board.blackRooks;
    moveData.previousBlackQueens = board.blackQueens;
    moveData.previousBlackKings = board.blackKings;
    moveData.previousWhitePieces = board.whitePieces;
    moveData.previousBlackPieces = board.blackPieces;
    moveData.previousAllPieces = board.allPieces;

    recordUndoSquare(moveData, board, fromSquare);
    recordUndoSquare(moveData, board, toSquare);

    if (moveData.capturedPiece.PieceType != ChessPieceType::NONE) {
        moveData.captureSquare = toSquare;
    }

    const bool isCastling =
        (movingPiece.PieceType == ChessPieceType::KING) && (std::abs(toSquare - fromSquare) == 2);
    if (isCastling) {
        moveData.wasCastling = true;
        if (toSquare > fromSquare) {
            moveData.rookFrom = fromSquare + kKingsideRookOffset;
            moveData.rookTo = fromSquare + kKingsideRookTargetOffset;
        } else {
            moveData.rookFrom = fromSquare - kQueensideRookOffset;
            moveData.rookTo = fromSquare - kQueensideRookTargetOffset;
        }
        recordUndoSquare(moveData, board, moveData.rookFrom);
        recordUndoSquare(moveData, board, moveData.rookTo);
    }

    const bool isEnPassant =
        (movingPiece.PieceType == ChessPieceType::PAWN && board.enPassantSquare == toSquare &&
         board.squares[toSquare].piece.PieceType == ChessPieceType::NONE &&
         std::abs((toSquare % kBoardDimension) - (fromSquare % kBoardDimension)) == kOne);
    if (isEnPassant) {
        int captureSquare =
            (movingPiece.PieceColor == ChessPieceColor::WHITE) ? (toSquare - kBoardDimension)
                                                                : (toSquare + kBoardDimension);
        if (captureSquare < kZero || captureSquare >= kBoardSquareCount) {
            return false;
        }
        const Piece epCaptured = board.squares[captureSquare].piece;
        if (epCaptured.PieceType != ChessPieceType::PAWN ||
            epCaptured.PieceColor == movingPiece.PieceColor) {
            return false;
        }
        moveData.captureSquare = captureSquare;
        moveData.capturedPiece = epCaptured;
        recordUndoSquare(moveData, board, captureSquare);
    }

    if (!board.movePiece(fromSquare, toSquare)) {
        return false;
    }

    if (isEnPassant && moveData.captureSquare >= kZero) {
        board.squares[moveData.captureSquare].piece = Piece();
        if (movingPiece.PieceColor == ChessPieceColor::WHITE) {
            clear_bit(board.blackPawns, moveData.captureSquare);
            clear_bit(board.blackPieces, moveData.captureSquare);
        } else {
            clear_bit(board.whitePawns, moveData.captureSquare);
            clear_bit(board.whitePieces, moveData.captureSquare);
        }
        clear_bit(board.allPieces, moveData.captureSquare);
    }

    if (isCastling && moveData.rookFrom >= kZero && moveData.rookTo >= kZero) {
        if (!board.movePiece(moveData.rookFrom, moveData.rookTo)) {
            undoSearchMoveWithData(board, moveData);
            return false;
        }
        board.LastMove = toSquare;
    }

    const int destinationRow = toSquare / kBoardDimension;
    const bool isPromotion =
        movingPiece.PieceType == ChessPieceType::PAWN &&
        ((movingPiece.PieceColor == ChessPieceColor::WHITE && destinationRow == kPromotionWhiteRank) ||
         (movingPiece.PieceColor == ChessPieceColor::BLACK && destinationRow == kPromotionBlackRank));
    if (autoPromoteToQueen && isPromotion) {
        if (movingPiece.PieceColor == ChessPieceColor::WHITE) {
            clear_bit(board.whitePawns, toSquare);
            set_bit(board.whiteQueens, toSquare);
        } else {
            clear_bit(board.blackPawns, toSquare);
            set_bit(board.blackQueens, toSquare);
        }
        board.squares[toSquare].piece = Piece(movingPiece.PieceColor, ChessPieceType::QUEEN);
        board.squares[toSquare].piece.moved = true;
    }

    if (movingPiece.PieceType == ChessPieceType::KING) {
        if (movingPiece.PieceColor == ChessPieceColor::WHITE) {
            board.whiteCanCastle = false;
        } else {
            board.blackCanCastle = false;
        }
    }

    if (movingPiece.PieceType == ChessPieceType::ROOK) {
        if (fromSquare == kWhiteQueensideRookSquare || fromSquare == kWhiteKingsideRookSquare) {
            board.whiteCanCastle = false;
        } else if (fromSquare == kBlackQueensideRookSquare ||
                   fromSquare == kBlackKingsideRookSquare) {
            board.blackCanCastle = false;
        }
    }

    if (moveData.capturedPiece.PieceType == ChessPieceType::ROOK) {
        if (moveData.captureSquare == kWhiteQueensideRookSquare ||
            moveData.captureSquare == kWhiteKingsideRookSquare) {
            board.whiteCanCastle = false;
        } else if (moveData.captureSquare == kBlackQueensideRookSquare ||
                   moveData.captureSquare == kBlackKingsideRookSquare) {
            board.blackCanCastle = false;
        }
    }

    board.enPassantSquare = kNoEpSquare;
    if (movingPiece.PieceType == ChessPieceType::PAWN &&
        std::abs(toSquare - fromSquare) == (2 * kBoardDimension)) {
        board.enPassantSquare = (fromSquare + toSquare) / kTwo;
    }

    if (moveDataOut != nullptr) {
        *moveDataOut = moveData;
    }
    return true;
}

void undoSearchMoveWithData(Board& board, const MoveApplicationData& moveData) {
    for (int i = kZero; i < moveData.changedSquareCount; ++i) {
        const int square = moveData.changedSquares[static_cast<std::size_t>(i)];
        board.squares[square].piece = moveData.previousPieces[static_cast<std::size_t>(i)];
    }

    board.turn = moveData.previousTurn;
    board.whiteCanCastle = moveData.previousWhiteCanCastle;
    board.blackCanCastle = moveData.previousBlackCanCastle;
    board.enPassantSquare = moveData.previousEnPassantSquare;
    board.whiteChecked = moveData.previousWhiteChecked;
    board.blackChecked = moveData.previousBlackChecked;
    board.LastMove = moveData.previousLastMove;
    board.lastMoveTime = moveData.previousLastMoveTime;
    board.whitePawns = moveData.previousWhitePawns;
    board.whiteKnights = moveData.previousWhiteKnights;
    board.whiteBishops = moveData.previousWhiteBishops;
    board.whiteRooks = moveData.previousWhiteRooks;
    board.whiteQueens = moveData.previousWhiteQueens;
    board.whiteKings = moveData.previousWhiteKings;
    board.blackPawns = moveData.previousBlackPawns;
    board.blackKnights = moveData.previousBlackKnights;
    board.blackBishops = moveData.previousBlackBishops;
    board.blackRooks = moveData.previousBlackRooks;
    board.blackQueens = moveData.previousBlackQueens;
    board.blackKings = moveData.previousBlackKings;
    board.whitePieces = moveData.previousWhitePieces;
    board.blackPieces = moveData.previousBlackPieces;
    board.allPieces = moveData.previousAllPieces;
}
} // namespace

bool applySearchMove(Board& board, int fromSquare, int toSquare, bool autoPromoteToQueen) {
    return applySearchMoveWithData(board, fromSquare, toSquare, autoPromoteToQueen, nullptr);
}

ThreadSafeHistory::ThreadSafeHistory() {
    table.fill(kZero);
}
void ThreadSafeHistory::update(int srcPos, int destPos, int bonus) {
    if (srcPos < kZero || srcPos >= kBoardSquareCount || destPos < kZero ||
        destPos >= kBoardSquareCount) {
        return;
    }
    int& entry = table[static_cast<std::size_t>(index(srcPos, destPos))];
    int absBonus = std::min(kHistoryMaxScore, std::abs(bonus));
    entry += bonus - ((entry * absBonus) / kHistoryMaxScore);
    entry = std::clamp(entry, -kHistoryMaxScore, kHistoryMaxScore);
}
int ThreadSafeHistory::get(int srcPos, int destPos) const {
    if (srcPos < kZero || srcPos >= kBoardSquareCount || destPos < kZero ||
        destPos >= kBoardSquareCount) {
        return kZero;
    }
    return table[static_cast<std::size_t>(index(srcPos, destPos))];
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
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        std::mt19937_64 rng(kZobristSeed);
        std::uniform_int_distribution<uint64_t> dist;
        for (auto& squareKeys : ZobristTable) {
            for (auto& pieceKey : squareKeys) {
                pieceKey = dist(rng);
            }
        }
        ZobristBlackToMove = dist(rng);
        for (auto& castlingKey : ZobristCastling) {
            castlingKey = dist(rng);
        }
        for (auto& epKey : ZobristEnPassant) {
            epKey = dist(rng);
        }
    });
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
    int castlingState = encodeCastlingState(board);
    if (castlingState >= kZero && castlingState < kZobristCastlingStateCount) {
        h ^= ZobristCastling[castlingState];
    }
    if (board.enPassantSquare >= kZero && board.enPassantSquare < kBoardSquareCount) {
        h ^= ZobristEnPassant[board.enPassantSquare];
    }
    return h;
}

bool isCapture(const Board& board, int srcPos, int destPos) {
    if (board.squares[destPos].piece.PieceType != ChessPieceType::NONE) {
        return true;
    }
    const Piece& movingPiece = board.squares[srcPos].piece;
    if (movingPiece.PieceType == ChessPieceType::PAWN && board.enPassantSquare == destPos &&
        std::abs((destPos % kBoardDimension) - (srcPos % kBoardDimension)) == kOne) {
        return true;
    }
    return false;
}

bool givesCheck(const Board& board, int srcPos, int destPos) {
    BitboardMoveState state;
    if (!applyMoveToBitboards(board, srcPos, destPos, true, state)) {
        return false;
    }

    const int enemyColor = (state.movingColor == kWhiteIndex) ? kBlackIndex : kWhiteIndex;
    const Bitboard enemyKing = state.pieces[enemyColor][pieceTypeIndex(ChessPieceType::KING)];
    if (enemyKing == EMPTY) {
        return false;
    }
    const int enemyKingSquare = lsb(enemyKing);
    return attackersToSquare(state.pieces, state.occupancy, enemyKingSquare, state.movingColor) !=
           EMPTY;
}

bool isInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

bool hasNonPawnMaterial(const Board& board, ChessPieceColor side) {
    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceColor != side) {
            continue;
        }
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING &&
            piece.PieceType != ChessPieceType::PAWN) {
            return true;
        }
    }
    return false;
}

int computeNullMoveReduction(int depth, int evalMargin) {
    int reduction = kNullMoveBaseReduction + (depth / kNullMoveDepthDivisor);
    if (evalMargin > kZero) {
        reduction += std::min(kTwo, evalMargin / kNullMoveEvalDivisor);
    }
    return std::clamp(reduction, kNullMoveBaseReduction, kNullMoveMaxReduction);
}

int toTtScore(int score, int ply) {
    if (score > kMateScoreTtThreshold) {
        return score + ply;
    }
    if (score < -kMateScoreTtThreshold) {
        return score - ply;
    }
    return score;
}

int fromTtScore(int score, int ply) {
    if (score > kMateScoreTtThreshold) {
        return score - ply;
    }
    if (score < -kMateScoreTtThreshold) {
        return score + ply;
    }
    return score;
}

void storeTtEntry(ParallelSearchContext& context, uint64_t key, int depth, int value, int flag,
                  const std::pair<int, int>& bestMove, int ply) {
    context.transTable.insert(key, TTEntry(depth, toTtScore(value, ply), flag, bestMove, key));
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
                                            const std::pair<int, int>& counterMove,
                                            const ParallelSearchContext* context) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());

    for (const auto& move : moves) {
        int score = kZero;
        const bool isCap = board.squares[move.second].piece.PieceType != ChessPieceType::NONE;

        if (move == ttMove) {
            score = EnhancedMoveOrdering::HASH_MOVE_SCORE;
        } else if (isCap) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            score = EnhancedMoveOrdering::CAPTURE_SCORE_BASE + (mvvLva * kMvvLvaScale) + see;
            int movingPieceIdx = pieceTypeIndex(board.squares[move.first].piece.PieceType);
            int victimPieceIdx = pieceTypeIndex(board.squares[move.second].piece.PieceType);
            if (context != nullptr) {
                score += context->getCaptureHistory(movingPieceIdx, victimPieceIdx, move.second) /
                         kCaptureHistoryDivisor;
            }
            if (see >= kZero) {
                score += kGoodSeeCaptureBonus;
            } else {
                score -= kBadSeeCapturePenalty;
            }
        } else if (killerMoves.isKiller(ply, move)) {
            score = EnhancedMoveOrdering::KILLER_SCORE +
                    EnhancedMoveOrdering::getKillerScore(killerMoves, ply, move.first, move.second);
            score +=
                EnhancedMoveOrdering::getHistoryScore(historyTable, move.first, move.second) / kTwo;
        } else {
            score = EnhancedMoveOrdering::HISTORY_SCORE_BASE +
                    EnhancedMoveOrdering::getHistoryScore(historyTable, move.first, move.second);
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
            if (context != nullptr && ply > kZero) {
                int prevPly = std::min(ply, kBoardSquareCount - kOne);
                int pp = context->prevPieceAtPly[prevPly];
                int pt = context->prevToAtPly[prevPly];
                int movingPieceIdx = pieceTypeIndex(board.squares[move.first].piece.PieceType);
                score += context->getContinuationHistory(pp, pt, movingPieceIdx, move.second) /
                         kContinuationHistoryDivisor;
            }
            if (counterMove.first >= kZero && move == counterMove) {
                score += kCounterMoveQuietBonus;
            }
            if (isPromotion(board, move.first, move.second)) {
                score += kPromotionQuietBonus;
            }
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
                score += kGoodSeeCaptureBonus;
            } else {
                score -= kBadSeeCapturePenalty;
            }
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
            if (isPromotion(board, move.first, move.second)) {
                score += kPromotionQuietBonus;
            }
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
    int bonus = std::max(kOne, depth * depth);
    historyTable.updateScore(fromSquare, toSquare, bonus);
}

void penalizeHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare,
                          int depth) {
    int malus = std::max(kOne, (depth * depth) / kTwo);
    historyTable.updateScore(fromSquare, toSquare, -malus);
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
    return lookupBookMoveString(fen, OpeningBookOptions, OpeningBook, true);
}

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply,
                     uint64_t zobristKey) {
    if (context.stopSearch.load()) {
        return kZero;
    }

    if (ply < kZero || ply >= kMaxSearchPly) {
        ScopedFastEvalMode fastEvalScope(true);
        return evaluatePosition(board, context.contempt);
    }

    const int originalAlpha = alpha;
    const int originalBeta = beta;
    context.nodeCount.fetch_add(kOne);

    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    context.transTable.prefetch(nodeZobristKey);
    TTEntry ttEntry;
    std::pair<int, int> hashMove = {kInvalidSquare, kInvalidSquare};
    if (context.transTable.find(nodeZobristKey, ttEntry)) {
        int ttValue = fromTtScore(ttEntry.value, ply);
        ttEntry.value = ttValue;
        if (ttEntry.depth >= kZero) {
            if (ttEntry.flag == kExactFlag) {
                return ttValue;
            }
            if (ttEntry.flag == kUpperBoundFlag && ttValue <= alpha) {
                return ttValue;
            }
            if (ttEntry.flag == kLowerBoundFlag && ttValue >= beta) {
                return ttValue;
            }
        }
        hashMove = ttEntry.bestMove;
    }

    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    bool inCheck = isInCheck(board, currentColor);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, currentColor);

    if (moves.empty()) {
        if (inCheck) {
            int mateScore = maximizingPlayer ? -kMateScore + ply : kMateScore - ply;
            storeTtEntry(context, nodeZobristKey, kZero, mateScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return mateScore;
        }
        int drawScore = -context.contempt;
        storeTtEntry(context, nodeZobristKey, kZero, drawScore, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return drawScore;
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
        storeTtEntry(context, nodeZobristKey, kZero, standPat, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return standPat;
    }

    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(tacticalMoves.size());
    for (const auto& move : tacticalMoves) {
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

    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
    int bestValue = standPat;
    std::pair<int, int> bestMove = {kInvalidSquare, kInvalidSquare};

    for (const auto& scoredMove : scoredMoves) {
        if (context.stopSearch.load()) {
            return kZero;
        }

        const auto& move = scoredMove.move;
        MoveApplicationData moveData{};
        Board newBoard = board;
        if (!applySearchMoveWithData(newBoard, move.first, move.second, true, &moveData)) {
            continue;
        }
        uint64_t childZobristKey =
            computeChildZobrist(nodeZobristKey, newBoard, move.first, move.second, moveData);

        if (isInCheck(newBoard, currentColor)) {
            continue;
        }
        newBoard.turn = (newBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                   : ChessPieceColor::WHITE;

        int eval = QuiescenceSearch(newBoard, alpha, beta, !maximizingPlayer, historyTable, context,
                                    ply + kOne, childZobristKey);
        if (context.stopSearch.load()) {
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

    if (isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch = true;
        return kZero;
    }

    if (depth == kZero) {
        return QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply,
                                resolveZobristKey(board, zobristKey));
    }

    context.nodeCount++;
    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    context.transTable.prefetch(nodeZobristKey);
    TTEntry ttEntry;
    bool ttHit = context.transTable.find(nodeZobristKey, ttEntry);
    if (ttHit) {
        ttEntry.value = fromTtScore(ttEntry.value, ply);
    }
    if (ttHit && ttEntry.depth >= depth) {
        if (ttEntry.flag == kExactFlag) {
            return ttEntry.value;
        }
        if (ttEntry.flag == kUpperBoundFlag && ttEntry.value <= alpha) {
            return ttEntry.value;
        }
        if (ttEntry.flag == kLowerBoundFlag && ttEntry.value >= beta) {
            return ttEntry.value;
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

    if (depth >= kLateDepthReductionThreshold && (!ttHit || ttEntry.bestMove.first < kZero)) {
        depth -= kOne;
    }

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
            int previousEnPassant = board.enPassantSquare;
            uint64_t nullZobristKey = nodeZobristKey ^ ZobristBlackToMove;
            if (previousEnPassant >= kZero && previousEnPassant < kBoardSquareCount) {
                nullZobristKey ^= ZobristEnPassant[previousEnPassant];
            }
            board.enPassantSquare = kNoEpSquare;
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
            int nullScore = kZero;
            if (maximizingPlayer) {
                nullScore = PrincipalVariationSearch(
                    board, nullDepth, beta - kZeroWindowOffset, beta, !maximizingPlayer,
                    ply + kOne, historyTable, context, false, nullZobristKey);
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

    MovePicker picker(board, ttEntry.bestMove, context.killerMoves, ply, historyTable, context);
    int colorIdx = (board.turn == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
    int prevDest = board.LastMove;
    const bool sideInCheck = isInCheck(board, board.turn);
    const int originalAlpha = alpha;
    const int originalBeta = beta;
    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    std::pair<int, int> bestMove = {kInvalidSquare, kInvalidSquare};
    int flag = kExactFlag;
    int movesSearched = kZero;
    std::vector<std::pair<int, int>> quietMovesSearched;
    quietMovesSearched.reserve(16);
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
        MoveApplicationData moveData{};
        if (!applySearchMoveWithData(tempBoard, move.first, move.second, true, &moveData)) {
            continue;
        }
        uint64_t childZobristKey =
            computeChildZobrist(nodeZobristKey, tempBoard, move.first, move.second, moveData);
        isCaptureMove = moveData.captureSquare >= kZero;
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
            mc.isCounter = (prevDest >= kZero && prevDest < kCounterMoveArrayLimit &&
                            context.counterMoves[colorIdx][prevDest] == move);
            mc.isPromotion = isPromotion(board, move.first, move.second);
            mc.isCastling = isCastling(board, move.first, move.second);
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
                score = PrincipalVariationSearch(tempBoard, searchDepth, alpha, beta,
                                                 !maximizingPlayer, ply + kOne, historyTable,
                                                 context, isPVNode, childZobristKey);
            } else {
                score = PrincipalVariationSearch(tempBoard, searchDepth, alpha,
                                                 alpha + kZeroWindowOffset, !maximizingPlayer,
                                                 ply + kOne, historyTable, context, false,
                                                 childZobristKey);
                if (score > alpha && score < beta) {
                    score = PrincipalVariationSearch(tempBoard, searchDepth, alpha, beta,
                                                     !maximizingPlayer, ply + kOne, historyTable,
                                                     context, isPVNode, childZobristKey);
                }
            }
        } else {
            if (movesSearched == kZero) {
                score = PrincipalVariationSearch(tempBoard, searchDepth, alpha, beta,
                                                 !maximizingPlayer, ply + kOne, historyTable,
                                                 context, isPVNode, childZobristKey);
            } else {
                score = PrincipalVariationSearch(tempBoard, searchDepth,
                                                 beta - kZeroWindowOffset, beta,
                                                 !maximizingPlayer, ply + kOne, historyTable,
                                                 context, false, childZobristKey);
                if (score < beta && score > alpha) {
                    score = PrincipalVariationSearch(tempBoard, searchDepth, alpha, beta,
                                                     !maximizingPlayer, ply + kOne, historyTable,
                                                     context, isPVNode, childZobristKey);
                }
            }
        }

        movesSearched++;
        if (!isCaptureMove) {
            quietMovesSearched.push_back(move);
        }

        if (maximizingPlayer) {
            if (score > bestValue) {
                bestValue = std::max(bestValue, score);
                bestMove = move;
                if (score > alpha) {
                    alpha = score;
                }
            }
        } else {
            if (score < bestValue) {
                bestValue = std::min(bestValue, score);
                bestMove = move;
                if (score < beta) {
                    beta = score;
                }
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
        } else {
            int drawScore = -context.contempt;
            storeTtEntry(context, nodeZobristKey, depth, drawScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return drawScore;
        }
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
        uint64_t hash = resolveZobristKey(board, zobristKey);
        if (context.transTable.find(hash, ttEntry) &&
            ttEntry.depth >= depth - kSingularTtDepthMargin) {
            ttEntry.value = fromTtScore(ttEntry.value, ply);
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

    const uint64_t nodeZobristKey = resolveZobristKey(board, zobristKey);
    context.transTable.prefetch(nodeZobristKey);
    TTEntry entry;
    std::pair<int, int> hashMove = {kInvalidSquare, kInvalidSquare};
    if (context.transTable.find(nodeZobristKey, entry)) {
        entry.value = fromTtScore(entry.value, ply);
        if (entry.depth >= depth) {
            if (entry.flag == kExactFlag) {
                return entry.value;
            }
            if (entry.flag == kUpperBoundFlag && entry.value <= alpha) {
                return entry.value;
            }
            if (entry.flag == kLowerBoundFlag && entry.value >= beta) {
                return entry.value;
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
            storeTtEntry(context, nodeZobristKey, depth, tbScore, kExactFlag,
                         {kInvalidSquare, kInvalidSquare}, ply);
            return tbScore;
        }
    }

    if (depth >= kLateDepthReductionThreshold && hashMove.first < kZero) {
        depth -= kOne;
    }

    if (depth == kZero) {
        int eval =
            QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply,
                             nodeZobristKey);
        storeTtEntry(context, nodeZobristKey, depth, eval, kExactFlag,
                     {kInvalidSquare, kInvalidSquare}, ply);
        return eval;
    }
    std::vector<std::pair<int, int>> moves =
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
        const int previousEnPassant = board.enPassantSquare;
        board.enPassantSquare = kNoEpSquare;
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                            : ChessPieceColor::WHITE;
        uint64_t nullZobristKey = nodeZobristKey ^ ZobristBlackToMove;
        if (previousEnPassant >= kZero && previousEnPassant < kBoardSquareCount) {
            nullZobristKey ^= ZobristEnPassant[previousEnPassant];
        }
        int evalMargin = maximizingPlayer ? (staticEval - beta) : (alpha - staticEval);
        int reducedDepth = std::max(kOne, depth - computeNullMoveReduction(depth, evalMargin));
        if (reducedDepth > kZero && reducedDepth <= kNullMoveMaxDepth) {
            int nullValue =
                maximizingPlayer
                    ? AlphaBetaSearch(board, reducedDepth, beta - kOne, beta, !maximizingPlayer,
                                      ply + kOne, historyTable, context, nullZobristKey)
                    : AlphaBetaSearch(board, reducedDepth, alpha, alpha + kOne,
                                      !maximizingPlayer, ply + kOne, historyTable, context,
                                      nullZobristKey);
            board.turn = previousTurn;
            board.enPassantSquare = previousEnPassant;
            if (context.stopSearch.load()) {
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
    std::pair<int, int> abCounterMove = {kInvalidSquare, kInvalidSquare};
    if (abPrevDest >= kZero && abPrevDest < kCounterMoveArrayLimit) {
        abCounterMove = context.counterMoves[abColorIdx][abPrevDest];
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(
        board, moves, historyTable, context.killerMoves, ply, hashMove, abCounterMove, &context);
    std::ranges::sort(scoredMoves, std::greater<ScoredMove>());
    int origAlpha = alpha;
    int origBeta = beta;
    int bestValue = maximizingPlayer ? -kMateScore : kMateScore;
    int flag = kExactFlag;
    bool foundPV = false;
    std::pair<int, int> bestMoveFound = {kInvalidSquare, kInvalidSquare};
    std::vector<std::pair<int, int>> quietMovesSearched;
    quietMovesSearched.reserve(16);

    if (maximizingPlayer) {
        int moveCount = kZero;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) {
                return kZero;
            }
            const auto& move = scoredMove.move;
            Board newBoard = board;
            MoveApplicationData moveData{};
            if (!applySearchMoveWithData(newBoard, move.first, move.second, true, &moveData)) {
                continue;
            }
            uint64_t childZobristKey = computeChildZobrist(nodeZobristKey, newBoard,
                                                           move.first, move.second, moveData);

            if (isInCheck(newBoard, currentColor)) {
                continue;
            }
            newBoard.turn = (newBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                       : ChessPieceColor::WHITE;

            int eval = kZero;
            bool isCaptureMove = moveData.captureSquare >= kZero;
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= kFutilityDepthLimit && !foundPV && !isCaptureMove && !isCheckMove &&
                !sideInCheck) {

                const int futilityMargins[kFutilityMarginTableSize] = {
                    kZero, kFutilityMarginDepth1, kFutilityMarginDepth2, kFutilityMarginDepth3};
                int futilityValue = staticEval + futilityMargins[depth];

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

            if (moveCount == kZero || !foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false, ply + kOne,
                                       historyTable, context, childZobristKey);
            } else {

                {
                    LMREnhanced::MoveClassification mc;
                    mc.isCapture = isCaptureMove;
                    mc.givesCheck = isCheckMove;
                    mc.isKiller = context.killerMoves.isKiller(ply, move);
                    mc.isHashMove = (move == hashMove);
                    mc.isCounter = (abCounterMove.first >= kZero && move == abCounterMove);
                    mc.historyScore = historyTable.get(move.first, move.second) + contHistScore;
                    mc.moveNumber = moveCount + kOne;
                    LMREnhanced::PositionContext pc;
                    pc.inCheck = sideInCheck;
                    int reduction = LMREnhanced::calculateReduction(depth, mc, pc);

                    if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                        int reducedDepth = std::max(kZero, depth - kOne - reduction);
                        int reducedEval =
                            AlphaBetaSearch(newBoard, reducedDepth, alpha, alpha + kOne, false,
                                            ply + kOne, historyTable, context, childZobristKey);

                        if (reducedEval > alpha) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false,
                                                   ply + kOne, historyTable, context,
                                                   childZobristKey);
                        } else {
                            eval = reducedEval;
                        }
                    } else {
                        eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, alpha + kOne, false,
                                               ply + kOne, historyTable, context, childZobristKey);

                        if (eval > alpha && eval < beta) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, false,
                                                   ply + kOne, historyTable, context,
                                                   childZobristKey);
                        }
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load()) {
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
        if (bestValue <= origAlpha) {
            flag = kUpperBoundFlag;
        } else if (bestValue >= origBeta) {
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
            MoveApplicationData moveData{};
            if (!applySearchMoveWithData(newBoard, move.first, move.second, true, &moveData)) {
                continue;
            }
            uint64_t childZobristKey = computeChildZobrist(nodeZobristKey, newBoard,
                                                           move.first, move.second, moveData);

            if (isInCheck(newBoard, currentColor)) {
                continue;
            }
            newBoard.turn = (newBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                       : ChessPieceColor::WHITE;

            int eval = kZero;
            bool isCaptureMove = moveData.captureSquare >= kZero;
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= kFutilityDepthLimit && !foundPV && !isCaptureMove && !isCheckMove &&
                !sideInCheck) {

                const int futilityMargins[kFutilityMarginTableSize] = {
                    kZero, kFutilityMarginDepth1, kFutilityMarginDepth2, kFutilityMarginDepth3};
                int futilityValue = staticEval - futilityMargins[depth];

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

            if (moveCount == kZero || !foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true, ply + kOne,
                                       historyTable, context, childZobristKey);
            } else {

                {
                    LMREnhanced::MoveClassification mc;
                    mc.isCapture = isCaptureMove;
                    mc.givesCheck = isCheckMove;
                    mc.isKiller = context.killerMoves.isKiller(ply, move);
                    mc.isHashMove = (move == hashMove);
                    mc.isCounter = (abCounterMove.first >= kZero && move == abCounterMove);
                    mc.historyScore = historyTable.get(move.first, move.second) + contHistScore;
                    mc.moveNumber = moveCount + kOne;
                    LMREnhanced::PositionContext pc;
                    pc.inCheck = sideInCheck;
                    int reduction = LMREnhanced::calculateReduction(depth, mc, pc);

                    if (reduction > kZero && !isCaptureMove && !isCheckMove) {
                        int reducedDepth = std::max(kZero, depth - kOne - reduction);
                        int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, beta - kOne, beta,
                                                          true, ply + kOne, historyTable, context,
                                                          childZobristKey);

                        if (reducedEval < beta) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true,
                                                   ply + kOne, historyTable, context,
                                                   childZobristKey);
                        } else {
                            eval = reducedEval;
                        }
                    } else {
                        eval = AlphaBetaSearch(newBoard, depth - kOne, beta - kOne, beta, true,
                                               ply + kOne, historyTable, context, childZobristKey);

                        if (eval < beta && eval > alpha) {
                            eval = AlphaBetaSearch(newBoard, depth - kOne, alpha, beta, true,
                                                   ply + kOne, historyTable, context,
                                                   childZobristKey);
                        }
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load()) {
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

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs, int numThreads,
                                        int contempt, int multiPV, int optimalTimeMs,
                                        int maxTimeMs, int hashSizeMb) {
    if (numThreads > kOne && multiPV == kOne) {
        auto smpStart = std::chrono::steady_clock::now();
        LazySMP smp(numThreads, hashSizeMb, contempt);
        SearchResult smpResult = smp.search(board, maxDepth, timeLimitMs);
        const auto smpElapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                  smpStart)
                .count();
        smpResult.timeMs =
            (smpElapsedMs > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
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
    context.optimalTimeMs = optimalTimeMs;
    context.maxTimeMs = maxTimeMs;
    context.useSyzygy = !Syzygy::getPath().empty();
    context.transTable.resize(std::max(SearchConstants::kOne, hashSizeMb));
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
            const uint64_t rootZobristKey = ComputeZobrist(board);

            do {
                searchScore = PrincipalVariationSearch(board, depth, alpha, beta,
                                                       board.turn == ChessPieceColor::WHITE, kZero,
                                                       context.historyTable, context, true,
                                                       rootZobristKey);
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
            if (!applySearchMove(newBoard, move.first, move.second)) {
                continue;
            }

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

    BitboardMoveState state;
    if (!applyMoveToBitboards(board, fromSquare, toSquare, true, state) ||
        state.capturedPiece < kZero) {
        return kZero;
    }

    int gain[kSeeMaxExchangeDepth]{};
    gain[kZero] = pieceValueFromIndex(state.capturedPiece);
    int depth = kZero;

    int sideToMove = (state.movingColor == kWhiteIndex) ? kBlackIndex : kWhiteIndex;
    int occupiedSide = state.movingColor;
    int occupiedPiece = state.movedPiece;

    while (depth + kOne < kSeeMaxExchangeDepth) {
        Bitboard attackers = attackersToSquare(state.pieces, state.occupancy, toSquare, sideToMove);
        int attackerSquare = kInvalidSquare;
        int attackerPiece = kInvalidSquare;
        if (!popLeastValuableAttacker(state.pieces, attackers, sideToMove, attackerSquare,
                                      attackerPiece)) {
            break;
        }

        depth++;
        gain[depth] = pieceValueFromIndex(occupiedPiece) - gain[depth - kOne];
        if (std::max(-gain[depth - kOne], gain[depth]) < kZero) {
            break;
        }

        clear_bit(state.pieces[sideToMove][attackerPiece], attackerSquare);
        clear_bit(state.pieces[occupiedSide][occupiedPiece], toSquare);
        clear_bit(state.occupancy, attackerSquare);
        set_bit(state.pieces[sideToMove][attackerPiece], toSquare);
        set_bit(state.occupancy, toSquare);

        occupiedSide = sideToMove;
        occupiedPiece = attackerPiece;
        sideToMove = (sideToMove == kWhiteIndex) ? kBlackIndex : kWhiteIndex;
    }

    while (depth > kZero) {
        gain[depth - kOne] = -std::max(-gain[depth - kOne], gain[depth]);
        depth--;
    }

    return gain[kZero];
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
