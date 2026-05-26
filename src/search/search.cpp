#include "../ai/SyzygyTablebase.h"
#include "../utils/engine_globals.h"
#include "AdvancedSearch.h"
#include "Bitboard.h"
#include "BitboardMoves.h"
#include "BookUtils.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "LMR.h"
#include "LazySMP.h"
#include "evaluation/Evaluation.h"
#include "search/ValidMoves.h"
#include "search_internal.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace SearchInternal {
uint64_t ZobristTable[kBoardSquareCount][12];
uint64_t ZobristBlackToMove = 0;
uint64_t ZobristCastling[kZobristCastlingStateCount];
uint64_t ZobristEnPassant[kBoardSquareCount];

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

    const auto index = static_cast<std::size_t>(moveData.changedSquareCount);
    moveData.changedSquares[index] = square;
    moveData.previousPieces[index] = board.squares[square].piece;
    moveData.changedSquareCount++;
}

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

    attackers |=
        KnightAttacks[targetSquare] & pieces[colorIndex][pieceTypeIndex(ChessPieceType::KNIGHT)];
    attackers |=
        KingAttacks[targetSquare] & pieces[colorIndex][pieceTypeIndex(ChessPieceType::KING)];
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
    const int minorCount = popcount(board.whiteKnights | board.blackKnights) +
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

    state.movingColor =
        (movingPiece.PieceColor == ChessPieceColor::WHITE) ? kWhiteIndex : kBlackIndex;
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
    const bool isPromotion = movingPiece.PieceType == ChessPieceType::PAWN &&
                             ((movingPiece.PieceColor == ChessPieceColor::WHITE &&
                               destinationRow == kPromotionWhiteRank) ||
                              (movingPiece.PieceColor == ChessPieceColor::BLACK &&
                               destinationRow == kPromotionBlackRank));
    if (autoPromoteToQueen && isPromotion) {
        movedPieceIndex = pieceTypeIndex(ChessPieceType::QUEEN);
    }

    set_bit(state.pieces[state.movingColor][movedPieceIndex], toSquare);
    set_bit(state.occupancy, toSquare);

    if (movingPiece.PieceType == ChessPieceType::KING &&
        std::abs(toSquare - fromSquare) == kCastlingDistance) {
        const int rookFrom = (toSquare > fromSquare) ? (fromSquare + kKingsideRookOffset)
                                                     : (fromSquare - kQueensideRookOffset);
        const int rookTo = (toSquare > fromSquare) ? (fromSquare + kKingsideRookTargetOffset)
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
    return static_cast<int>(board.castlingRights() & CastlingConstants::kAllCastlingRightsMask);
}

uint64_t computeChildZobrist(uint64_t parentKey, const Board& childBoard, int fromSquare,
                             int toSquare, const MoveApplicationData& moveData) {
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
        int rookIdx =
            pieceToZobristIndex(Piece(moveData.movingPiece.PieceColor, ChessPieceType::ROOK));
        if (rookIdx >= kZero) {
            childKey ^= ZobristTable[moveData.rookFrom][rookIdx];
            childKey ^= ZobristTable[moveData.rookTo][rookIdx];
        }
    }

    const int parentCastle = static_cast<int>(moveData.previousCastlingRights &
                                              CastlingConstants::kAllCastlingRightsMask);
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
    moveData.previousCastlingRights = board.castlingRights();
    moveData.previousEnPassantSquare = board.enPassantSquare;
    moveData.previousHalfmoveClock = board.halfmoveClock;
    moveData.previousFullmoveNumber = board.fullmoveNumber;
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
        int captureSquare = (movingPiece.PieceColor == ChessPieceColor::WHITE)
                                ? (toSquare - kBoardDimension)
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
    const bool isPromotion = movingPiece.PieceType == ChessPieceType::PAWN &&
                             ((movingPiece.PieceColor == ChessPieceColor::WHITE &&
                               destinationRow == kPromotionWhiteRank) ||
                              (movingPiece.PieceColor == ChessPieceColor::BLACK &&
                               destinationRow == kPromotionBlackRank));
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
            board.clearCastlingRights(CastlingConstants::kWhiteCastlingRightsMask);
        } else {
            board.clearCastlingRights(CastlingConstants::kBlackCastlingRightsMask);
        }
    }

    if (movingPiece.PieceType == ChessPieceType::ROOK) {
        if (fromSquare == kWhiteQueensideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kWhiteQueensideCastlingRight);
        } else if (fromSquare == kWhiteKingsideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kWhiteKingsideCastlingRight);
        } else if (fromSquare == kBlackQueensideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kBlackQueensideCastlingRight);
        } else if (fromSquare == kBlackKingsideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kBlackKingsideCastlingRight);
        }
    }

    if (moveData.capturedPiece.PieceType == ChessPieceType::ROOK) {
        if (moveData.captureSquare == kWhiteQueensideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kWhiteQueensideCastlingRight);
        } else if (moveData.captureSquare == kWhiteKingsideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kWhiteKingsideCastlingRight);
        } else if (moveData.captureSquare == kBlackQueensideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kBlackQueensideCastlingRight);
        } else if (moveData.captureSquare == kBlackKingsideRookSquare) {
            board.clearCastlingRights(CastlingConstants::kBlackKingsideCastlingRight);
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
    board.state.castlingRights = moveData.previousCastlingRights;
    board.enPassantSquare = moveData.previousEnPassantSquare;
    board.halfmoveClock = moveData.previousHalfmoveClock;
    board.fullmoveNumber = moveData.previousFullmoveNumber;
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
} // namespace SearchInternal

using namespace SearchInternal;

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

void KillerMoves::store(int ply, Move move) {
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

bool KillerMoves::isKiller(int ply, Move move) const {
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

int KillerMoves::getKillerScore(int ply, Move move) const {
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
      multiPV(kOne), tbHits(kZero), continuationHistory{}, captureHistory{}, prevPieceAtPly{},
      prevToAtPly{} {
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
        std::mt19937_64 rng(kZobristSeed); // NOLINT(bugprone-random-generator-seed)
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
    return movingPiece.PieceType == ChessPieceType::PAWN && board.enPassantSquare == destPos &&
           std::abs((destPos % kBoardDimension) - (srcPos % kBoardDimension)) == kOne;
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

bool SearchInternal::hasNonPawnMaterial(const Board& board, ChessPieceColor side) {
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

int SearchInternal::computeNullMoveReduction(int depth, int evalMargin) {
    int reduction = kNullMoveBaseReduction + (depth / kNullMoveDepthDivisor);
    if (evalMargin > kZero) {
        reduction += std::min(kTwo, evalMargin / kNullMoveEvalDivisor);
    }
    return std::clamp(reduction, kNullMoveBaseReduction, kNullMoveMaxReduction);
}

int SearchInternal::toTtScore(int score, int ply) {
    if (score > kMateScoreTtThreshold) {
        return score + ply;
    }
    if (score < -kMateScoreTtThreshold) {
        return score - ply;
    }
    return score;
}

int SearchInternal::fromTtScore(int score, int ply) {
    if (score > kMateScoreTtThreshold) {
        return score - ply;
    }
    if (score < -kMateScoreTtThreshold) {
        return score + ply;
    }
    return score;
}

void SearchInternal::storeTtEntry(ParallelSearchContext& context, uint64_t key, int depth,
                                  int value, int flag, const Move& bestMove, int ply) {
    context.transTable.insert(key, TTEntry(depth, toTtScore(value, ply), flag, bestMove, key));
}

int SearchInternal::getPieceValue(ChessPieceType pieceType) {
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

std::vector<ScoredMove> SearchInternal::scoreMovesOptimized(
    const Board& board, std::span<const Move> moves, const ThreadSafeHistory& historyTable,
    const KillerMoves& killerMoves, int ply, const Move& ttMove, const Move& counterMove,
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

bool SearchInternal::isBetterScoredMove(const ScoredMove& lhs, const ScoredMove& rhs) {
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    if (lhs.move.first != rhs.move.first) {
        return lhs.move.first < rhs.move.first;
    }
    return lhs.move.second < rhs.move.second;
}

const ScoredMove& SearchInternal::pickNextScoredMove(std::vector<ScoredMove>& scoredMoves,
                                                     std::size_t nextIndex) {
    std::size_t bestIndex = nextIndex;
    for (std::size_t i = nextIndex + 1; i < scoredMoves.size(); ++i) {
        if (isBetterScoredMove(scoredMoves[i], scoredMoves[bestIndex])) {
            bestIndex = i;
        }
    }

    if (bestIndex != nextIndex) {
        std::swap(scoredMoves[nextIndex], scoredMoves[bestIndex]);
    }

    return scoredMoves[nextIndex];
}

bool SearchInternal::moveMatches(const Move& lhs, const Move& rhs) {
    return lhs.first == rhs.first && lhs.second == rhs.second;
}

bool SearchInternal::moveExistsInList(std::span<const Move> moves, const Move& candidate) {
    return std::ranges::any_of(moves,
                               [&](const Move& move) { return moveMatches(move, candidate); });
}

int SearchInternal::staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare) {
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

bool SearchInternal::isPromotion(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    ChessPieceColor color = board.squares[from].piece.PieceColor;

    if (piece != ChessPieceType::PAWN) {
        return false;
    }

    int destRow = to / kBoardDimension;
    return (color == ChessPieceColor::WHITE && destRow == kPromotionWhiteRank) ||
           (color == ChessPieceColor::BLACK && destRow == kPromotionBlackRank);
}

bool SearchInternal::isCastling(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    if (piece != ChessPieceType::KING) {
        return false;
    }

    return std::abs(to - from) == kCastlingDistance;
}

const SearchInternal::EnhancedMoveOrdering::MvvLvaTable
    SearchInternal::EnhancedMoveOrdering::MVV_LVA_SCORES = {
        {{kZero, kZero, kZero, kZero, kZero, kZero},
         {kMvvLvaVictimPawn, kZero, kZero, kZero, kZero, kZero},
         {kMvvLvaVictimPawn, kZero, kZero, kZero, kZero, kZero},
         {kMvvLvaVictimKnight, kTwo, kTwo, kZero, kZero, kZero},
         {kMvvLvaVictimBishop, kFour, kFour, kTwo, kZero, kZero},
         {kMvvLvaVictimRook, kThree, kThree, kOne, kOne, kZero}}};

int SearchInternal::EnhancedMoveOrdering::getMVVLVA_Score(const Board& board, int fromSquare,
                                                          int toSquare) {
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

int SearchInternal::EnhancedMoveOrdering::getHistoryScore(const ThreadSafeHistory& history,
                                                          int fromSquare, int toSquare) {
    return history.get(fromSquare, toSquare);
}

int SearchInternal::EnhancedMoveOrdering::getKillerScore(const KillerMoves& killers, int ply,
                                                         int fromSquare, int toSquare) {
    Move move = {fromSquare, toSquare};
    if (killers.isKiller(ply, move)) {
        return killers.getKillerScore(ply, move);
    }
    return kZero;
}

int SearchInternal::EnhancedMoveOrdering::getPositionalScore(const Board& board, int fromSquare,
                                                             int toSquare) {
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
