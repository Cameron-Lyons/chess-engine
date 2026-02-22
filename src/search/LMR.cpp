#include "LMR.h"
#include "../evaluation/Evaluation.h"
#include "ValidMoves.h"
#include "search.h"

namespace LMREnhanced {

namespace {
constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kPawnValue = 100;
constexpr int kKnightValue = 320;
constexpr int kBishopValue = 330;
constexpr int kRookValue = 500;
constexpr int kQueenValue = 900;
constexpr int kKingValue = 20000;
constexpr int kBoardDimension = 8;
constexpr int kTopRank = kBoardDimension - 1;
constexpr int kPromotionWhiteRank = kTopRank;
constexpr int kPromotionBlackRank = kZero;
constexpr int kPassedPawnWhiteRankThreshold = 4;
constexpr int kPassedPawnBlackRankThreshold = 3;
constexpr int kCastlingFileDelta = 2;
constexpr int kEvalImprovingThreshold = 20;
constexpr int kBoardSquareCount = 64;
constexpr int kStartingMaterial = 7800;
constexpr int kGamePhaseScale = 256;
constexpr int kEndgamePhaseThreshold = 200;
constexpr int kEndgamePieceCountThreshold = 6;
constexpr int kPawnAttackFileWindow = 1;
} // namespace

static int getPieceValue(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN:
            return kPawnValue;
        case ChessPieceType::KNIGHT:
            return kKnightValue;
        case ChessPieceType::BISHOP:
            return kBishopValue;
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

ReductionTable reductionTable;

MoveClassification classifyMove(const Board& board, const std::pair<int, int>& move,
                                const KillerMoves& killers, int ply,
                                const ThreadSafeHistory& history,
                                const std::pair<int, int>& hashMove, int moveNumber) {
    MoveClassification classification;

    classification.moveNumber = moveNumber;

    classification.isHashMove = (move == hashMove);

    classification.isCapture = (board.squares[move.second].piece.PieceType != ChessPieceType::NONE);

    classification.isKiller = killers.isKiller(ply, move);

    classification.historyScore = history.get(move.first, move.second);

    ChessPieceType movingPiece = board.squares[move.first].piece.PieceType;
    if (movingPiece == ChessPieceType::PAWN) {
        int toRank = move.second / kBoardDimension;
        if ((board.turn == ChessPieceColor::WHITE && toRank == kPromotionWhiteRank) ||
            (board.turn == ChessPieceColor::BLACK && toRank == kPromotionBlackRank)) {
            classification.isPromotion = true;
        }

        int toFile = move.second % kBoardDimension;
        bool isPassed = true;

        if (board.turn == ChessPieceColor::WHITE) {
            for (int r = toRank + kOne; r < kBoardDimension; ++r) {
                for (int f = std::max(kZero, toFile - kPawnAttackFileWindow);
                     f <= std::min(kTopRank, toFile + kPawnAttackFileWindow); ++f) {
                    int sq = (r * kBoardDimension) + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::BLACK) {
                        isPassed = false;
                        break;
                    }
                }
            }
        } else {
            for (int r = toRank - kOne; r >= kZero; --r) {
                for (int f = std::max(kZero, toFile - kPawnAttackFileWindow);
                     f <= std::min(kTopRank, toFile + kPawnAttackFileWindow); ++f) {
                    int sq = (r * kBoardDimension) + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::WHITE) {
                        isPassed = false;
                        break;
                    }
                }
            }
        }

        classification.isPassed = isPassed && (board.turn == ChessPieceColor::WHITE
                                                   ? toRank >= kPassedPawnWhiteRankThreshold
                                                   : toRank <= kPassedPawnBlackRankThreshold);
    }

    if (movingPiece == ChessPieceType::KING &&
        std::abs(move.second - move.first) == kCastlingFileDelta) {
        classification.isCastling = true;
    }

    Board tempBoard = board;
    tempBoard.movePiece(move.first, move.second);
    tempBoard.turn =
        (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    classification.givesCheck = isInCheck(tempBoard, tempBoard.turn);

    if (!classification.isCapture && movingPiece != ChessPieceType::NONE) {

        std::vector<std::pair<int, int>> attacks;
        if (movingPiece == ChessPieceType::PAWN) {
            attacks = generatePawnMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::KNIGHT) {
            attacks = generateKnightMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::BISHOP) {
            attacks = generateBishopMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::ROOK) {
            attacks = generateRookMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::QUEEN) {
            attacks = generateQueenMoves(tempBoard, board.turn);
        }

        int movingValue = getPieceValue(movingPiece);
        for (auto& attack : attacks) {
            if (attack.first == move.second) {
                int attackSq = attack.second;
                ChessPieceType attackedPiece = tempBoard.squares[attackSq].piece.PieceType;
                if (attackedPiece != ChessPieceType::NONE &&
                    tempBoard.squares[attackSq].piece.PieceColor != board.turn) {
                    int attackedValue = getPieceValue(attackedPiece);
                    if (attackedValue > movingValue) {
                        classification.isTactical = true;
                        break;
                    }
                }
            }
        }
    }

    return classification;
}

PositionContext evaluatePosition(const Board& board, int staticEval, int previousEval,
                                 bool isPVNode) {
    PositionContext context;

    context.isPVNode = isPVNode;
    context.staticEval = staticEval;

    context.inCheck = isInCheck(board, board.turn);

    context.evalTrend = staticEval - previousEval;
    context.isImproving = context.evalTrend > kEvalImprovingThreshold;

    int totalMaterial = kZero;
    int pieceCount = kZero;
    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        ChessPieceType piece = board.squares[sq].piece.PieceType;
        if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING) {
            totalMaterial += getPieceValue(piece);
            if (piece != ChessPieceType::PAWN) {
                pieceCount++;
            }
        }
    }

    context.gamePhase =
        std::min(kGamePhaseScale,
                 (kGamePhaseScale * (kStartingMaterial - totalMaterial)) / kStartingMaterial);
    context.isEndgame =
        (context.gamePhase > kEndgamePhaseThreshold || pieceCount <= kEndgamePieceCountThreshold);

    context.isTactical = false;

    for (int sq = kZero; sq < kBoardSquareCount; ++sq) {
        if (board.squares[sq].piece.PieceType != ChessPieceType::NONE &&
            board.squares[sq].piece.PieceColor == board.turn) {

            ChessPieceColor enemyColor = (board.turn == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            auto enemyMoves = generateBitboardMoves(const_cast<Board&>(board), enemyColor);
            for (auto& move : enemyMoves) {
                if (move.second == sq) {

                    ChessPieceType attackedPiece = board.squares[sq].piece.PieceType;
                    ChessPieceType attackingPiece = board.squares[move.first].piece.PieceType;
                    if (getPieceValue(attackedPiece) >= getPieceValue(attackingPiece)) {
                        context.isTactical = true;
                        break;
                    }
                }
            }
            if (context.isTactical) {
                break;
            }
        }
    }

    context.isSingular = false;

    return context;
}

} // namespace LMREnhanced
