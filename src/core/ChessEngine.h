#pragma once

#include "core/CastlingConstants.h"
#include "core/ChessBoard.h"
#include "core/ChessPiece.h"
#include "search/ValidMoves.h"

#include <iostream>
#include <stack>
#include <string>
#include <utility>

extern Board ChessBoard;
extern Board PrevBoard;
extern std::stack<int> MoveHistory;

void Engine() {
    ChessBoard = Board();
    ChessBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveHistory = std::stack<int>();
}

void RegisterPiece(int col, int row, Piece piece) {
    int position = col + (row * BOARD_SIZE);
    ChessBoard.squares[position].piece = piece;
}

bool MovePiece(int srcCol, int srcRow, int destCol, int destRow, ChessPieceType promotionPiece) {
    int src = srcCol + (srcRow * BOARD_SIZE);
    int dest = destCol + (destRow * BOARD_SIZE);

    if (src < 0 || src >= NUM_SQUARES || dest < 0 || dest >= NUM_SQUARES) {
        return false;
    }

    Piece piece = ChessBoard.squares[src].piece;

    if (piece.PieceType == ChessPieceType::NONE) {
        return false;
    }

    if (piece.PieceColor != ChessBoard.turn) {
        return false;
    }

    if (!IsMoveLegal(ChessBoard, src, dest)) {
        return false;
    }

    PrevBoard = ChessBoard;
    bool promotePawn =
        (piece.PieceType == ChessPieceType::PAWN && (destRow == 0 || destRow == (BOARD_SIZE - 1)));
    ChessBoard.movePiece(src, dest);

    if (promotePawn) {
        const bool isValidPromotion =
            promotionPiece == ChessPieceType::QUEEN || promotionPiece == ChessPieceType::ROOK ||
            promotionPiece == ChessPieceType::BISHOP || promotionPiece == ChessPieceType::KNIGHT;
        const ChessPieceType finalPromotion =
            isValidPromotion ? promotionPiece : ChessPieceType::QUEEN;
        ChessBoard.squares[dest].piece.PieceType = finalPromotion;
        ChessBoard.updateBitboards();
        const char* promotedPieceName = "Queen";
        switch (finalPromotion) {
            case ChessPieceType::QUEEN:
                promotedPieceName = "Queen";
                break;
            case ChessPieceType::ROOK:
                promotedPieceName = "Rook";
                break;
            case ChessPieceType::BISHOP:
                promotedPieceName = "Bishop";
                break;
            case ChessPieceType::KNIGHT:
                promotedPieceName = "Knight";
                break;
            default:
                promotedPieceName = "Queen";
                break;
        }
        std::cout << "Pawn promoted to " << promotedPieceName << "!\n";
    }

    if (piece.PieceType == ChessPieceType::KING &&
        srcCol == CastlingConstants::kWhiteKingStartCol) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            if (destCol == CastlingConstants::kKingsideKingDestCol) {
                ChessBoard.movePiece(CastlingConstants::kWhiteKingsideRookSquare,
                                     CastlingConstants::kWhiteKingsideRookCastleDest);
                ChessBoard.whiteCanCastle = false;
            } else if (destCol == CastlingConstants::kQueensideKingDestCol) {
                ChessBoard.movePiece(CastlingConstants::kWhiteQueensideRookSquare,
                                     CastlingConstants::kWhiteQueensideRookCastleDest);
                ChessBoard.whiteCanCastle = false;
            }
        } else {
            if (destCol == CastlingConstants::kKingsideKingDestCol) {
                ChessBoard.movePiece(CastlingConstants::kBlackKingsideRookSquare,
                                     CastlingConstants::kBlackKingsideRookCastleDest);
                ChessBoard.blackCanCastle = false;
            } else if (destCol == CastlingConstants::kQueensideKingDestCol) {
                ChessBoard.movePiece(CastlingConstants::kBlackQueensideRookSquare,
                                     CastlingConstants::kBlackQueensideRookCastleDest);
                ChessBoard.blackCanCastle = false;
            }
        }
    }

    if (piece.PieceType == ChessPieceType::KING) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            WhiteKingPosition = dest;
        } else {
            BlackKingPosition = dest;
        }
    }

    GenValidMoves(ChessBoard);

    if (IsKingInCheck(ChessBoard, piece.PieceColor)) {
        ChessBoard = PrevBoard;
        GenValidMoves(ChessBoard);
        return false;
    }

    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                  : ChessPieceColor::WHITE;

    MoveHistory.push(ChessBoard.LastMove);
    return true;
}

bool MovePiece(int srcCol, int srcRow, int destCol, int destRow) {
    return MovePiece(srcCol, srcRow, destCol, destRow, ChessPieceType::QUEEN);
}
