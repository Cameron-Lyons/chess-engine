#include "LMREnhanced.h"
#include "../evaluation/Evaluation.h"
#include "ValidMoves.h"
#include "search.h"

namespace LMREnhanced {

static int getPieceValue(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN:
            return 100;
        case ChessPieceType::KNIGHT:
            return 320;
        case ChessPieceType::BISHOP:
            return 330;
        case ChessPieceType::ROOK:
            return 500;
        case ChessPieceType::QUEEN:
            return 900;
        case ChessPieceType::KING:
            return 20000;
        default:
            return 0;
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
        int toRank = move.second / 8;
        if ((board.turn == ChessPieceColor::WHITE && toRank == 7) ||
            (board.turn == ChessPieceColor::BLACK && toRank == 0)) {
            classification.isPromotion = true;
        }

        int toFile = move.second % 8;
        bool isPassed = true;

        if (board.turn == ChessPieceColor::WHITE) {
            for (int r = toRank + 1; r < 8; ++r) {
                for (int f = std::max(0, toFile - 1); f <= std::min(7, toFile + 1); ++f) {
                    int sq = r * 8 + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::BLACK) {
                        isPassed = false;
                        break;
                    }
                }
            }
        } else {
            for (int r = toRank - 1; r >= 0; --r) {
                for (int f = std::max(0, toFile - 1); f <= std::min(7, toFile + 1); ++f) {
                    int sq = r * 8 + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::WHITE) {
                        isPassed = false;
                        break;
                    }
                }
            }
        }

        classification.isPassed =
            isPassed && (board.turn == ChessPieceColor::WHITE ? toRank >= 4 : toRank <= 3);
    }

    if (movingPiece == ChessPieceType::KING && std::abs(move.second - move.first) == 2) {
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
    context.isImproving = context.evalTrend > 20;

    int totalMaterial = 0;
    int pieceCount = 0;
    for (int sq = 0; sq < 64; ++sq) {
        ChessPieceType piece = board.squares[sq].piece.PieceType;
        if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING) {
            totalMaterial += getPieceValue(piece);
            if (piece != ChessPieceType::PAWN) {
                pieceCount++;
            }
        }
    }

    const int startingMaterial = 7800;
    context.gamePhase =
        std::min(256, (256 * (startingMaterial - totalMaterial)) / startingMaterial);
    context.isEndgame = (context.gamePhase > 200 || pieceCount <= 6);

    context.isTactical = false;

    for (int sq = 0; sq < 64; ++sq) {
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
            if (context.isTactical)
                break;
        }
    }

    context.isSingular = false;

    return context;
}

} // namespace LMREnhanced