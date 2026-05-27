#include "ValidMoves_internal.h"

#include <utility>

namespace ValidMovesInternal {
namespace {

Bitboard ownPieces(const Board& board, ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? board.whitePieces : board.blackPieces;
}

Bitboard enemyPieces(const Board& board, ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? board.blackPieces : board.whitePieces;
}

using SlidingAttackFn = Bitboard (*)(int square, Bitboard occupancy);

void appendSlidingPieceMoves(Board& board, ChessPieceColor color, ChessPieceType pieceType,
                             Bitboard targetSquares, SlidingAttackFn attackFn,
                             std::vector<Move>& moves) {
    Bitboard pieces = board.getPieceBitboard(pieceType, color);
    const Bitboard occupancy = board.allPieces;
    while (pieces) {
        const int src = lsb(pieces);
        clear_bit(pieces, src);
        Bitboard attacks = attackFn(src, occupancy) & targetSquares;
        while (attacks) {
            const int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendKnightPieceMoves(Board& board, ChessPieceColor color, Bitboard targetSquares,
                            std::vector<Move>& moves) {
    Bitboard knights = board.getPieceBitboard(ChessPieceType::KNIGHT, color);
    while (knights) {
        const int src = lsb(knights);
        clear_bit(knights, src);
        Bitboard attacks = KnightAttacks[src] & targetSquares;
        while (attacks) {
            const int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendPawnQuietPushes(Board& board, ChessPieceColor color, int src, std::vector<Move>& moves) {
    const Bitboard occupied = board.allPieces;
    if (color == ChessPieceColor::WHITE) {
        const int oneStep = src + kSinglePawnPush;
        if (oneStep < kBoardSquareCount && !(occupied & (1ULL << oneStep))) {
            moves.emplace_back(src, oneStep);
            if ((src / kBoardDimension) == kWhitePawnStartRank) {
                const int twoStep = src + kDoublePawnPush;
                if (!(occupied & (1ULL << twoStep))) {
                    moves.emplace_back(src, twoStep);
                }
            }
        }
    } else {
        const int oneStep = src - kSinglePawnPush;
        if (oneStep >= kZero && !(occupied & (1ULL << oneStep))) {
            moves.emplace_back(src, oneStep);
            if ((src / kBoardDimension) == kBlackPawnStartRank) {
                const int twoStep = src - kDoublePawnPush;
                if (!(occupied & (1ULL << twoStep))) {
                    moves.emplace_back(src, twoStep);
                }
            }
        }
    }
}

void appendPawnRegularCaptures(Board& board, ChessPieceColor color, int src,
                               std::vector<Move>& moves) {
    Bitboard captures = pawnAttacks(color, src) & enemyPieces(board, color);
    while (captures) {
        const int dest = lsb(captures);
        moves.emplace_back(src, dest);
        clear_bit(captures, dest);
    }
}

} // namespace

void appendPawnMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    while (pawns) {
        const int src = lsb(pawns);
        clear_bit(pawns, src);
        appendPawnQuietPushes(board, color, src, moves);
        appendPawnRegularCaptures(board, color, src, moves);
        tryAppendEnPassantMove(board, color, src, moves);
    }
}

void appendKnightMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendKnightPieceMoves(board, color, ~ownPieces(board, color), moves);
}

void appendBishopMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::BISHOP, ~ownPieces(board, color),
                            bishopAttacks, moves);
}

void appendRookMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::ROOK, ~ownPieces(board, color),
                            rookAttacks, moves);
}

void appendQueenMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::QUEEN, ~ownPieces(board, color),
                            queenAttacks, moves);
}

void appendKingMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard king = board.getPieceBitboard(ChessPieceType::KING, color);
    if (!king) {
        return;
    }

    Bitboard kingMovesBB = kingMoves(king, ownPieces(board, color));
    const int src = lsb(king);
    while (kingMovesBB) {
        const int dest = lsb(kingMovesBB);
        moves.emplace_back(src, dest);
        clear_bit(kingMovesBB, dest);
    }
}

void appendPawnCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    while (pawns) {
        const int src = lsb(pawns);
        clear_bit(pawns, src);
        appendPawnRegularCaptures(board, color, src, moves);
        tryAppendEnPassantMove(board, color, src, moves);
    }
}

void appendKnightCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendKnightPieceMoves(board, color, enemyPieces(board, color), moves);
}

void appendBishopCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::BISHOP, enemyPieces(board, color),
                            bishopAttacks, moves);
}

void appendRookCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::ROOK, enemyPieces(board, color),
                            rookAttacks, moves);
}

void appendQueenCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    appendSlidingPieceMoves(board, color, ChessPieceType::QUEEN, enemyPieces(board, color),
                            queenAttacks, moves);
}

void appendKingCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard king = board.getPieceBitboard(ChessPieceType::KING, color);
    if (!king) {
        return;
    }

    Bitboard kingMovesBB = KingAttacks[lsb(king)] & enemyPieces(board, color);
    const int src = lsb(king);
    while (kingMovesBB) {
        const int dest = lsb(kingMovesBB);
        moves.emplace_back(src, dest);
        clear_bit(kingMovesBB, dest);
    }
}

bool hasLegalMoveFromCandidates(Board& board, ChessPieceColor color,
                                const std::vector<Move>& moves) {
    for (const auto& move : moves) {
        const Piece& piece = board.squares[move.first].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }

        if (piece.PieceType == ChessPieceType::KING &&
            (move.second == move.first + 2 || move.second == move.first - 2)) {
            return true;
        }

        Board tempBoard = board;
        if (!tempBoard.movePiece(move.first, move.second)) {
            continue;
        }

        const bool isEnPassantCapture =
            piece.PieceType == ChessPieceType::PAWN && move.second == board.enPassantSquare &&
            board.squares[move.second].piece.PieceType == ChessPieceType::NONE &&
            ((move.first % kBoardDimension) != (move.second % kBoardDimension));
        if (isEnPassantCapture) {
            const int capturedPawnSquare =
                move.second +
                ((color == ChessPieceColor::WHITE) ? -kSinglePawnPush : kSinglePawnPush);
            if (capturedPawnSquare >= kZero && capturedPawnSquare < kBoardSquareCount) {
                tempBoard.squares[capturedPawnSquare].piece = Piece();
            }
        }

        tempBoard.updateBitboards();
        if (!IsKingInCheck(tempBoard, color)) {
            return true;
        }
    }

    return false;
}
} // namespace ValidMovesInternal

using namespace ValidMovesInternal;

std::vector<Move> generatePawnMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendPawnMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateKnightMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendKnightMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateBishopMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendBishopMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateRookMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendRookMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateQueenMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendQueenMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateKingMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(16);
    appendKingMoves(board, color, moves);
    return moves;
}

std::vector<Move> generateBitboardMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);
    appendPawnMoves(board, color, moves);
    appendKnightMoves(board, color, moves);
    appendBishopMoves(board, color, moves);
    appendRookMoves(board, color, moves);
    appendQueenMoves(board, color, moves);
    appendKingMoves(board, color, moves);
    addCastlingMovesBitboard(board, color, &moves);
    return moves;
}

std::vector<Move> generateBitboardCaptureMoves(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(32);
    appendPawnCaptures(board, color, moves);
    appendKnightCaptures(board, color, moves);
    appendBishopCaptures(board, color, moves);
    appendRookCaptures(board, color, moves);
    appendQueenCaptures(board, color, moves);
    appendKingCaptures(board, color, moves);
    return moves;
}

bool hasAnyLegalMove(Board& board, ChessPieceColor color) {
    std::vector<Move> moves;
    moves.reserve(kTypicalMoveCapacity);

    appendPawnMoves(board, color, moves);
    if (hasLegalMoveFromCandidates(board, color, moves)) {
        return true;
    }

    moves.clear();
    appendKnightMoves(board, color, moves);
    if (hasLegalMoveFromCandidates(board, color, moves)) {
        return true;
    }

    moves.clear();
    appendBishopMoves(board, color, moves);
    if (hasLegalMoveFromCandidates(board, color, moves)) {
        return true;
    }

    moves.clear();
    appendRookMoves(board, color, moves);
    if (hasLegalMoveFromCandidates(board, color, moves)) {
        return true;
    }

    moves.clear();
    appendQueenMoves(board, color, moves);
    if (hasLegalMoveFromCandidates(board, color, moves)) {
        return true;
    }

    moves.clear();
    appendKingMoves(board, color, moves);
    addCastlingMovesBitboard(board, color, &moves);
    return hasLegalMoveFromCandidates(board, color, moves);
}
