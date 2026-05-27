#include "ValidMoves_internal.h"

#include <utility>

namespace ValidMovesInternal {
void appendPawnMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    const Bitboard occupied = board.allPieces;
    const Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;

    while (pawns) {
        int src = lsb(pawns);
        clear_bit(pawns, src);

        if (color == ChessPieceColor::WHITE) {
            int oneStep = src + kSinglePawnPush;
            if (oneStep < kBoardSquareCount && !(occupied & (1ULL << oneStep))) {
                moves.emplace_back(src, oneStep);
                if ((src / kBoardDimension) == kWhitePawnStartRank) {
                    int twoStep = src + kDoublePawnPush;
                    if (!(occupied & (1ULL << twoStep))) {
                        moves.emplace_back(src, twoStep);
                    }
                }
            }
        } else {
            int oneStep = src - kSinglePawnPush;
            if (oneStep >= kZero && !(occupied & (1ULL << oneStep))) {
                moves.emplace_back(src, oneStep);
                if ((src / kBoardDimension) == kBlackPawnStartRank) {
                    int twoStep = src - kDoublePawnPush;
                    if (!(occupied & (1ULL << twoStep))) {
                        moves.emplace_back(src, twoStep);
                    }
                }
            }
        }

        Bitboard captures = pawnAttacks(color, src) & enemyPieces;
        while (captures) {
            int dest = lsb(captures);
            moves.emplace_back(src, dest);
            clear_bit(captures, dest);
        }

        if (const auto enPassantTarget = board.enPassantSquare.target()) {
            const int epSquare = *enPassantTarget;
            int srcFile = src % kBoardDimension;
            if (color == ChessPieceColor::WHITE) {
                if ((srcFile > kMinFile && epSquare == src + kWhiteEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile && epSquare == src + kWhiteEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare - kSinglePawnPush;
                    if (capturedPawnSquare >= kZero &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor ==
                            ChessPieceColor::BLACK &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            } else {
                if ((srcFile > kMinFile && epSquare == src - kBlackEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile && epSquare == src - kBlackEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare + kSinglePawnPush;
                    if (capturedPawnSquare < kBoardSquareCount &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor ==
                            ChessPieceColor::WHITE &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            }
        }
    }
}

void appendKnightMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard knights = board.getPieceBitboard(ChessPieceType::KNIGHT, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;
    while (knights) {
        int src = lsb(knights);
        clear_bit(knights, src);
        Bitboard attacks = KnightAttacks[src] & ~ownPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendBishopMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard bishops = board.getPieceBitboard(ChessPieceType::BISHOP, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;
    while (bishops) {
        int src = lsb(bishops);
        clear_bit(bishops, src);
        Bitboard attacks = bishopAttacks(src, board.allPieces) & ~ownPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendRookMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard rooks = board.getPieceBitboard(ChessPieceType::ROOK, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;
    while (rooks) {
        int src = lsb(rooks);
        clear_bit(rooks, src);
        Bitboard attacks = rookAttacks(src, board.allPieces) & ~ownPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendQueenMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard queens = board.getPieceBitboard(ChessPieceType::QUEEN, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;
    while (queens) {
        int src = lsb(queens);
        clear_bit(queens, src);
        Bitboard attacks = queenAttacks(src, board.allPieces) & ~ownPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendKingMoves(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard king = board.getPieceBitboard(ChessPieceType::KING, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;

    if (king) {
        Bitboard kingMovesBB = kingMoves(king, ownPieces);
        while (kingMovesBB) {
            int dest = lsb(kingMovesBB);
            int src = lsb(king);
            moves.emplace_back(src, dest);
            clear_bit(kingMovesBB, dest);
        }
    }
}

void appendPawnCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    const Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;

    while (pawns) {
        int src = lsb(pawns);
        clear_bit(pawns, src);

        Bitboard captures = pawnAttacks(color, src) & enemyPieces;
        while (captures) {
            int dest = lsb(captures);
            moves.emplace_back(src, dest);
            clear_bit(captures, dest);
        }

        if (const auto enPassantTarget = board.enPassantSquare.target()) {
            const int epSquare = *enPassantTarget;
            int srcFile = src % kBoardDimension;
            if (color == ChessPieceColor::WHITE) {
                if ((srcFile > kMinFile && epSquare == src + kWhiteEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile && epSquare == src + kWhiteEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare - kSinglePawnPush;
                    if (capturedPawnSquare >= kZero &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor ==
                            ChessPieceColor::BLACK &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            } else {
                if ((srcFile > kMinFile && epSquare == src - kBlackEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile && epSquare == src - kBlackEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare + kSinglePawnPush;
                    if (capturedPawnSquare < kBoardSquareCount &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor ==
                            ChessPieceColor::WHITE &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            }
        }
    }
}

void appendKnightCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard knights = board.getPieceBitboard(ChessPieceType::KNIGHT, color);
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;
    while (knights) {
        int src = lsb(knights);
        clear_bit(knights, src);
        Bitboard attacks = KnightAttacks[src] & enemyPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendBishopCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard bishops = board.getPieceBitboard(ChessPieceType::BISHOP, color);
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;
    while (bishops) {
        int src = lsb(bishops);
        clear_bit(bishops, src);
        Bitboard attacks = bishopAttacks(src, board.allPieces) & enemyPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendRookCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard rooks = board.getPieceBitboard(ChessPieceType::ROOK, color);
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;
    while (rooks) {
        int src = lsb(rooks);
        clear_bit(rooks, src);
        Bitboard attacks = rookAttacks(src, board.allPieces) & enemyPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendQueenCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard queens = board.getPieceBitboard(ChessPieceType::QUEEN, color);
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;
    while (queens) {
        int src = lsb(queens);
        clear_bit(queens, src);
        Bitboard attacks = queenAttacks(src, board.allPieces) & enemyPieces;
        while (attacks) {
            int dest = lsb(attacks);
            moves.emplace_back(src, dest);
            clear_bit(attacks, dest);
        }
    }
}

void appendKingCaptures(Board& board, ChessPieceColor color, std::vector<Move>& moves) {
    Bitboard king = board.getPieceBitboard(ChessPieceType::KING, color);
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;

    if (king) {
        Bitboard kingMovesBB = KingAttacks[lsb(king)] & enemyPieces;
        while (kingMovesBB) {
            int dest = lsb(kingMovesBB);
            int src = lsb(king);
            moves.emplace_back(src, dest);
            clear_bit(kingMovesBB, dest);
        }
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
