#include "ValidMoves_internal.h"

#include "Bitboard.h"

using namespace ValidMovesInternal;

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

namespace ValidMovesInternal {
namespace {

bool kingPathIsSafe(const Board& board, const CastlingSideConfig& config, int firstTransit,
                    int finalSquare) {
    if (IsKingInCheck(board, config.color)) {
        return false;
    }

    Board tempBoard = board;
    tempBoard.movePiece(config.kingStart, firstTransit);
    tempBoard.updateBitboards();
    if (IsKingInCheck(tempBoard, config.color)) {
        return false;
    }

    if (finalSquare != firstTransit) {
        tempBoard.movePiece(firstTransit, finalSquare);
        tempBoard.updateBitboards();
        if (IsKingInCheck(tempBoard, config.color)) {
            return false;
        }
    }

    return true;
}

bool rookIsReady(const Board& board, int rookSquare, ChessPieceColor color) {
    const Piece& rook = board.squares[rookSquare].piece;
    return rook.PieceType == ChessPieceType::ROOK && rook.PieceColor == color && !rook.moved;
}

} // namespace

bool canCastleKingside(const Board& board, const CastlingSideConfig& config, Bitboard occupancy) {
    if (!board.hasCastlingRight(config.kingsideCastlingRight)) {
        return false;
    }
    if ((occupancy & (1ULL << config.kingsideTransitSquare)) ||
        (occupancy & (1ULL << config.kingsideDestination))) {
        return false;
    }
    if (!rookIsReady(board, config.kingsideRookSquare, config.color)) {
        return false;
    }

    return kingPathIsSafe(board, config, config.kingsideTransitSquare,
                        config.kingsideDestination);
}

bool canCastleQueenside(const Board& board, const CastlingSideConfig& config, Bitboard occupancy) {
    if (!board.hasCastlingRight(config.queensideCastlingRight)) {
        return false;
    }
    if ((occupancy & (1ULL << config.queensideEmptySquare)) ||
        (occupancy & (1ULL << config.queensideSecondTransit)) ||
        (occupancy & (1ULL << config.queensideFirstTransit))) {
        return false;
    }
    if (!rookIsReady(board, config.queensideRookSquare, config.color)) {
        return false;
    }

    return kingPathIsSafe(board, config, config.queensideFirstTransit,
                          config.queensideSecondTransit);
}

} // namespace ValidMovesInternal

void addCastlingMovesBitboard(Board& board, ChessPieceColor color,
                              std::vector<Move>* generatedMoves) {
    const CastlingSideConfig& config =
        color == ChessPieceColor::WHITE ? kWhiteCastlingSide : kBlackCastlingSide;
    const Piece& king = board.squares[config.kingStart].piece;
    if (king.PieceType != ChessPieceType::KING || king.PieceColor != color || king.moved) {
        return;
    }

    const Bitboard occupancy = board.allPieces;
    if (canCastleKingside(board, config, occupancy) && generatedMoves != nullptr) {
        generatedMoves->emplace_back(config.kingStart, config.kingsideDestination);
    }
    if (canCastleQueenside(board, config, occupancy) && generatedMoves != nullptr) {
        generatedMoves->emplace_back(config.kingStart, config.queensideSecondTransit);
    }
}

void UpdateCheckState(Board& board) {
    board.whiteChecked = IsKingInCheck(board, ChessPieceColor::WHITE);
    board.blackChecked = IsKingInCheck(board, ChessPieceColor::BLACK);
}
