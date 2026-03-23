#include "ValidMoves_internal.h"

#include "Bitboard.h"

using namespace ValidMovesInternal;

thread_local bool BlackAttackBoard[kValidMovesBoardSquareCount] = {false};
thread_local bool WhiteAttackBoard[kValidMovesBoardSquareCount] = {false};
thread_local int BlackKingPosition = kZero;
thread_local int WhiteKingPosition = kZero;

bool IsKingInCheck(const Board& board, ChessPieceColor color) {
    const Bitboard king = (color == ChessPieceColor::WHITE) ? board.whiteKings : board.blackKings;
    if (king == EMPTY) {
        return false;
    }
    const int kingSq = lsb(king);

    Bitboard occ = board.allPieces;
    Bitboard enemyPawns = board.getPieceBitboard(
        ChessPieceType::PAWN,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyKnights = board.getPieceBitboard(
        ChessPieceType::KNIGHT,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyBishops = board.getPieceBitboard(
        ChessPieceType::BISHOP,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyRooks = board.getPieceBitboard(
        ChessPieceType::ROOK,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyQueens = board.getPieceBitboard(
        ChessPieceType::QUEEN,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyKing = board.getPieceBitboard(
        ChessPieceType::KING,
        color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);

    Bitboard pawnAttacksBB = kZero;
    if (color == ChessPieceColor::WHITE) {
        pawnAttacksBB = pawnAttacks(ChessPieceColor::WHITE, kingSq);
    } else {
        pawnAttacksBB = pawnAttacks(ChessPieceColor::BLACK, kingSq);
    }
    if (pawnAttacksBB & enemyPawns) {
        return true;
    }

    if (KnightAttacks[kingSq] & enemyKnights) {
        return true;
    }

    if (bishopAttacks(kingSq, occ) & (enemyBishops | enemyQueens)) {
        return true;
    }

    if (rookAttacks(kingSq, occ) & (enemyRooks | enemyQueens)) {
        return true;
    }

    if (KingAttacks[kingSq] & enemyKing) {
        return true;
    }

    return false;
}

bool IsMoveLegal(Board& board, int srcPos, int destPos) {
    Piece& piece = board.squares[srcPos].piece;
    Piece& destPiece = board.squares[destPos].piece;

    if (destPiece.PieceType != ChessPieceType::NONE && destPiece.PieceColor == piece.PieceColor) {
        return false;
    }

    std::vector<Move> moves;
    switch (piece.PieceType) {
        case ChessPieceType::PAWN:
            moves = generatePawnMoves(board, piece.PieceColor);
            break;
        case ChessPieceType::KNIGHT:
            moves = generateKnightMoves(board, piece.PieceColor);
            break;
        case ChessPieceType::BISHOP:
            moves = generateBishopMoves(board, piece.PieceColor);
            break;
        case ChessPieceType::ROOK:
            moves = generateRookMoves(board, piece.PieceColor);
            break;
        case ChessPieceType::QUEEN:
            moves = generateQueenMoves(board, piece.PieceColor);
            break;
        case ChessPieceType::KING:
            moves = generateKingMoves(board, piece.PieceColor);
            addCastlingMovesBitboard(board, piece.PieceColor, &moves);
            break;
        default:
            return false;
    }
    bool found = false;
    for (const auto& m : moves) {
        if (m.first == srcPos && m.second == destPos) {
            found = true;
            break;
        }
    }
    if (!found) {
        return false;
    }

    Board tempBoard = board;
    tempBoard.movePiece(srcPos, destPos);
    tempBoard.updateBitboards();
    return !IsKingInCheck(tempBoard, piece.PieceColor);
}

void addCastlingMovesBitboard(Board& board, ChessPieceColor color,
                              std::vector<Move>* generatedMoves) {
    int kingStart =
        (color == ChessPieceColor::WHITE) ? kWhiteKingStartSquare : kBlackKingStartSquare;
    const Piece& king = board.squares[kingStart].piece;
    if (king.PieceType != ChessPieceType::KING || king.PieceColor != color || king.moved) {
        return;
    }

    Bitboard occ = board.allPieces;
    bool canCastleKingside = false;
    bool canCastleQueenside = false;
    if (color == ChessPieceColor::WHITE) {
        const Piece& kingsideRook = board.squares[kWhiteKingsideRookSquare].piece;
        const Piece& queensideRook = board.squares[kWhiteQueensideRookSquare].piece;
        const bool hasKingsideRight =
            board.hasCastlingRight(CastlingConstants::kWhiteKingsideCastlingRight);
        const bool hasQueensideRight =
            board.hasCastlingRight(CastlingConstants::kWhiteQueensideCastlingRight);

        if (hasKingsideRight && !(occ & (1ULL << kWhiteKingsideTransitSquare)) &&
            !(occ & (1ULL << kWhiteKingsideDestination)) &&
            kingsideRook.PieceType == ChessPieceType::ROOK &&
            kingsideRook.PieceColor == ChessPieceColor::WHITE && !kingsideRook.moved) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(kWhiteKingStartSquare, kWhiteKingsideTransitSquare);
                temp.updateBitboards();
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(kWhiteKingsideTransitSquare, kWhiteKingsideDestination);
                    temp.updateBitboards();
                    if (!IsKingInCheck(temp, color)) {
                        canCastleKingside = true;
                    }
                }
            }
        }
        if (hasQueensideRight && !(occ & (1ULL << kWhiteQueensideEmptySquare)) &&
            !(occ & (1ULL << kWhiteQueensideSecondTransit)) &&
            !(occ & (1ULL << kWhiteQueensideFirstTransit)) &&
            queensideRook.PieceType == ChessPieceType::ROOK &&
            queensideRook.PieceColor == ChessPieceColor::WHITE && !queensideRook.moved) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(kWhiteKingStartSquare, kWhiteQueensideFirstTransit);
                temp.updateBitboards();
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(kWhiteQueensideFirstTransit, kWhiteQueensideSecondTransit);
                    temp.updateBitboards();
                    if (!IsKingInCheck(temp, color)) {
                        canCastleQueenside = true;
                    }
                }
            }
        }
    } else if (color == ChessPieceColor::BLACK) {
        const Piece& kingsideRook = board.squares[kBlackKingsideRookSquare].piece;
        const Piece& queensideRook = board.squares[kBlackQueensideRookSquare].piece;
        const bool hasKingsideRight =
            board.hasCastlingRight(CastlingConstants::kBlackKingsideCastlingRight);
        const bool hasQueensideRight =
            board.hasCastlingRight(CastlingConstants::kBlackQueensideCastlingRight);

        if (hasKingsideRight && !(occ & (1ULL << kBlackKingsideTransitSquare)) &&
            !(occ & (1ULL << kBlackKingsideDestination)) &&
            kingsideRook.PieceType == ChessPieceType::ROOK &&
            kingsideRook.PieceColor == ChessPieceColor::BLACK && !kingsideRook.moved) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(kBlackKingStartSquare, kBlackKingsideTransitSquare);
                temp.updateBitboards();
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(kBlackKingsideTransitSquare, kBlackKingsideDestination);
                    temp.updateBitboards();
                    if (!IsKingInCheck(temp, color)) {
                        canCastleKingside = true;
                    }
                }
            }
        }
        if (hasQueensideRight && !(occ & (1ULL << kBlackQueensideEmptySquare)) &&
            !(occ & (1ULL << kBlackQueensideSecondTransit)) &&
            !(occ & (1ULL << kBlackQueensideFirstTransit)) &&
            queensideRook.PieceType == ChessPieceType::ROOK &&
            queensideRook.PieceColor == ChessPieceColor::BLACK && !queensideRook.moved) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(kBlackKingStartSquare, kBlackQueensideFirstTransit);
                temp.updateBitboards();
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(kBlackQueensideFirstTransit, kBlackQueensideSecondTransit);
                    temp.updateBitboards();
                    if (!IsKingInCheck(temp, color)) {
                        canCastleQueenside = true;
                    }
                }
            }
        }
    }
    if (canCastleKingside) {
        int dest = (color == ChessPieceColor::WHITE) ? kWhiteKingsideDestination
                                                     : kBlackKingsideDestination;
        if (generatedMoves != nullptr) {
            generatedMoves->emplace_back(kingStart, dest);
        }
    }
    if (canCastleQueenside) {
        int dest = (color == ChessPieceColor::WHITE) ? kWhiteQueensideSecondTransit
                                                     : kBlackQueensideSecondTransit;
        if (generatedMoves != nullptr) {
            generatedMoves->emplace_back(kingStart, dest);
        }
    }
}

void GenValidMoves(Board& board) {
    board.whiteChecked = false;
    board.blackChecked = false;

    for (int i = kZero; i < kBoardSquareCount; i++) {
        BlackAttackBoard[i] = false;
        WhiteAttackBoard[i] = false;
    }

    WhiteKingPosition = board.whiteKings ? lsb(board.whiteKings) : kInvalidSquare;
    BlackKingPosition = board.blackKings ? lsb(board.blackKings) : kInvalidSquare;

    board.whiteChecked = IsKingInCheck(board, ChessPieceColor::WHITE);
    board.blackChecked = IsKingInCheck(board, ChessPieceColor::BLACK);
}
