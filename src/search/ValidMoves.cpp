#include "ValidMoves.h"
#include "../core/BitboardMoves.h"

#include <iostream>

namespace {
constexpr int kZero = 0;
constexpr int kInvalidSquare = -1;
constexpr int kBoardSquareCount = kValidMovesBoardSquareCount;
constexpr int kBoardDimension = 8;
constexpr int kSinglePawnPush = 8;
constexpr int kDoublePawnPush = 16;
constexpr int kWhitePawnStartRank = 1;
constexpr int kBlackPawnStartRank = 6;
constexpr int kMinFile = 0;
constexpr int kMaxFile = kBoardDimension - 1;
constexpr int kWhiteEnPassantCaptureLeftOffset = 7;
constexpr int kWhiteEnPassantCaptureRightOffset = 9;
constexpr int kBlackEnPassantCaptureLeftOffset = 9;
constexpr int kBlackEnPassantCaptureRightOffset = 7;

constexpr int kWhiteKingStartSquare = 4;
constexpr int kBlackKingStartSquare = 60;
constexpr int kWhiteKingsideTransitSquare = 5;
constexpr int kWhiteKingsideDestination = 6;
constexpr int kWhiteQueensideFirstTransit = 3;
constexpr int kWhiteQueensideSecondTransit = 2;
constexpr int kWhiteQueensideEmptySquare = 1;
constexpr int kBlackKingsideTransitSquare = 61;
constexpr int kBlackKingsideDestination = 62;
constexpr int kBlackQueensideFirstTransit = 59;
constexpr int kBlackQueensideSecondTransit = 58;
constexpr int kBlackQueensideEmptySquare = 57;
constexpr int kWhiteKingsideRookSquare = 7;
constexpr int kWhiteQueensideRookSquare = 0;
constexpr int kBlackKingsideRookSquare = 63;
constexpr int kBlackQueensideRookSquare = 56;

} // namespace

bool BlackAttackBoard[kValidMovesBoardSquareCount] = {false};
bool WhiteAttackBoard[kValidMovesBoardSquareCount] = {false};
int BlackKingPosition = kZero;
int WhiteKingPosition = kZero;

bool IsKingInCheck(const Board& board, ChessPieceColor color) {
    int kingSq = kInvalidSquare;
    for (int i = kZero; i < kBoardSquareCount; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING &&
            board.squares[i].piece.PieceColor == color) {
            kingSq = i;
            break;
        }
    }
    if (kingSq == kInvalidSquare) {
        return false;
    }

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
        pawnAttacksBB = pawnAttacks(ChessPieceColor::BLACK, kingSq);
    } else {
        pawnAttacksBB = pawnAttacks(ChessPieceColor::WHITE, kingSq);
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

    std::vector<std::pair<int, int>> moves;
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
                              std::vector<std::pair<int, int>>* generatedMoves) {
    int kingStart =
        (color == ChessPieceColor::WHITE) ? kWhiteKingStartSquare : kBlackKingStartSquare;
    const Piece& king = board.squares[kingStart].piece;
    if (king.PieceType != ChessPieceType::KING || king.PieceColor != color || king.moved) {
        return;
    }

    Bitboard occ = board.allPieces;
    bool canCastleKingside = false;
    bool canCastleQueenside = false;
    if (color == ChessPieceColor::WHITE && board.whiteCanCastle) {
        const Piece& kingsideRook = board.squares[kWhiteKingsideRookSquare].piece;
        const Piece& queensideRook = board.squares[kWhiteQueensideRookSquare].piece;

        if (!(occ & (1ULL << kWhiteKingsideTransitSquare)) &&
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
        if (!(occ & (1ULL << kWhiteQueensideEmptySquare)) &&
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
    } else if (color == ChessPieceColor::BLACK && board.blackCanCastle) {
        const Piece& kingsideRook = board.squares[kBlackKingsideRookSquare].piece;
        const Piece& queensideRook = board.squares[kBlackQueensideRookSquare].piece;

        if (!(occ & (1ULL << kBlackKingsideTransitSquare)) &&
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
        if (!(occ & (1ULL << kBlackQueensideEmptySquare)) &&
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
        int dest =
            (color == ChessPieceColor::WHITE) ? kWhiteKingsideDestination : kBlackKingsideDestination;
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

std::vector<std::pair<int, int>> generatePawnMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

        if (board.enPassantSquare >= kZero && board.enPassantSquare < kBoardSquareCount) {
            int epSquare = board.enPassantSquare;
            int srcFile = src % kBoardDimension;
            if (color == ChessPieceColor::WHITE) {
                if ((srcFile > kMinFile && epSquare == src + kWhiteEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile &&
                     epSquare == src + kWhiteEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare - kSinglePawnPush;
                    if (capturedPawnSquare >= kZero &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor == ChessPieceColor::BLACK &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            } else {
                if ((srcFile > kMinFile && epSquare == src - kBlackEnPassantCaptureLeftOffset) ||
                    (srcFile < kMaxFile &&
                     epSquare == src - kBlackEnPassantCaptureRightOffset)) {
                    int capturedPawnSquare = epSquare + kSinglePawnPush;
                    if (capturedPawnSquare < kBoardSquareCount &&
                        board.squares[capturedPawnSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[capturedPawnSquare].piece.PieceColor == ChessPieceColor::WHITE &&
                        board.squares[epSquare].piece.PieceType == ChessPieceType::NONE) {
                        moves.emplace_back(src, epSquare);
                    }
                }
            }
        }
    }

    return moves;
}

std::vector<std::pair<int, int>> generateKnightMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

    return moves;
}

std::vector<std::pair<int, int>> generateBishopMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

    return moves;
}

std::vector<std::pair<int, int>> generateRookMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

    return moves;
}

std::vector<std::pair<int, int>> generateQueenMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

    return moves;
}

std::vector<std::pair<int, int>> generateKingMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
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

    return moves;
}

std::vector<std::pair<int, int>> generateBitboardMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    moves.reserve(kBoardSquareCount);
    auto pawnMoves = generatePawnMoves(board, color);
    auto knightMoves = generateKnightMoves(board, color);
    auto bishopMoves = generateBishopMoves(board, color);
    auto rookMoves = generateRookMoves(board, color);
    auto queenMoves = generateQueenMoves(board, color);
    auto kingMoves = generateKingMoves(board, color);
    moves.insert(moves.end(), pawnMoves.begin(), pawnMoves.end());
    moves.insert(moves.end(), knightMoves.begin(), knightMoves.end());
    moves.insert(moves.end(), bishopMoves.begin(), bishopMoves.end());
    moves.insert(moves.end(), rookMoves.begin(), rookMoves.end());
    moves.insert(moves.end(), queenMoves.begin(), queenMoves.end());
    moves.insert(moves.end(), kingMoves.begin(), kingMoves.end());
    addCastlingMovesBitboard(board, color, &moves);
    return moves;
}

void GenValidMoves(Board& board) {
    board.whiteChecked = false;
    board.blackChecked = false;

    for (int i = kZero; i < kBoardSquareCount; i++) {
        BlackAttackBoard[i] = false;
        WhiteAttackBoard[i] = false;
    }

    for (int i = kZero; i < kBoardSquareCount; i++) {
        board.squares[i].piece.ValidMoves.clear();
    }

    for (int i = kZero; i < kBoardSquareCount; i++) {
        Square& square = board.squares[i];
        if (square.piece.PieceType == ChessPieceType::KING) {
            if (square.piece.PieceColor == ChessPieceColor::WHITE) {
                WhiteKingPosition = i;
            } else {
                BlackKingPosition = i;
            }
        }
    }

    auto moves = generateBitboardMoves(board, board.turn);

    for (const auto& move : moves) {
        int src = move.first;
        int dest = move.second;
        board.squares[src].piece.ValidMoves.push_back(dest);
    }

    board.whiteChecked = IsKingInCheck(board, ChessPieceColor::WHITE);
    board.blackChecked = IsKingInCheck(board, ChessPieceColor::BLACK);
}
