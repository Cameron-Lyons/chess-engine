#include "ValidMoves.h"
#include "../core/BitboardMoves.h"

#include <iostream>

namespace {
constexpr int kZero = 0;
constexpr int kInvalidSquare = -1;
constexpr int kBoardSquareCount = kValidMovesBoardSquareCount;
constexpr int kSinglePawnPush = 8;
constexpr int kDoublePawnPush = 16;

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

constexpr Bitboard kWhitePawnStartMask = 0x000000000000FF00ULL;
constexpr Bitboard kBlackPawnStartMask = 0x00FF000000000000ULL;
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

void addCastlingMovesBitboard(Board& board, ChessPieceColor color) {
    int kingStart =
        (color == ChessPieceColor::WHITE) ? kWhiteKingStartSquare : kBlackKingStartSquare;
    Piece& king = board.squares[kingStart].piece;
    if (king.PieceType != ChessPieceType::KING || king.moved) {
        return;
    }

    Bitboard occ = board.allPieces;
    bool canCastleKingside = false;
    bool canCastleQueenside = false;
    if (color == ChessPieceColor::WHITE && board.whiteCanCastle) {
        if (!(occ & (1ULL << kWhiteKingsideTransitSquare)) &&
            !(occ & (1ULL << kWhiteKingsideDestination))) {
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
            !(occ & (1ULL << kWhiteQueensideFirstTransit))) {
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
        if (!(occ & (1ULL << kBlackKingsideTransitSquare)) &&
            !(occ & (1ULL << kBlackKingsideDestination))) {
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
            !(occ & (1ULL << kBlackQueensideFirstTransit))) {
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
        king.ValidMoves.push_back((color == ChessPieceColor::WHITE) ? kWhiteKingsideDestination
                                                                    : kBlackKingsideDestination);
    }
    if (canCastleQueenside) {
        king.ValidMoves.push_back((color == ChessPieceColor::WHITE) ? kWhiteQueensideSecondTransit
                                                                    : kBlackQueensideSecondTransit);
    }
}

std::vector<std::pair<int, int>> generatePawnMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    Bitboard empty = ~board.allPieces;
    Bitboard enemyPieces =
        (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;

    Bitboard singlePushes = kZero;
    if (color == ChessPieceColor::WHITE) {
        singlePushes = (pawns << kSinglePawnPush) & empty;
    } else {
        singlePushes = (pawns >> kSinglePawnPush) & empty;
    }

    while (singlePushes) {
        int dest = lsb(singlePushes);
        int src =
            (color == ChessPieceColor::WHITE) ? dest - kSinglePawnPush : dest + kSinglePawnPush;
        moves.emplace_back(src, dest);
        clear_bit(singlePushes, dest);
    }

    Bitboard doublePushes = kZero;
    if (color == ChessPieceColor::WHITE) {
        Bitboard startingPawns = pawns & kWhitePawnStartMask;
        Bitboard singlePushSquares = (startingPawns << kSinglePawnPush) & empty;
        doublePushes = (singlePushSquares << kSinglePawnPush) & empty;
    } else {
        Bitboard startingPawns = pawns & kBlackPawnStartMask;
        Bitboard singlePushSquares = (startingPawns >> kSinglePawnPush) & empty;
        doublePushes = (singlePushSquares >> kSinglePawnPush) & empty;
    }

    while (doublePushes) {
        int dest = lsb(doublePushes);
        int src =
            (color == ChessPieceColor::WHITE) ? dest - kDoublePawnPush : dest + kDoublePawnPush;
        moves.emplace_back(src, dest);
        clear_bit(doublePushes, dest);
    }

    Bitboard captures = pawnCaptures(pawns, enemyPieces, color);
    while (captures) {
        int dest = lsb(captures);
        Bitboard tempPawns = pawns;
        while (tempPawns) {
            int src = lsb(tempPawns);
            Bitboard pawnAttack = pawnAttacks(color, src);
            if (pawnAttack & (1ULL << dest)) {
                moves.emplace_back(src, dest);
                break;
            }
            clear_bit(tempPawns, src);
        }
        clear_bit(captures, dest);
    }

    return moves;
}

std::vector<std::pair<int, int>> generateKnightMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard knights = board.getPieceBitboard(ChessPieceType::KNIGHT, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;

    Bitboard knightMovesBB = knightMoves(knights, ownPieces);
    while (knightMovesBB) {
        int dest = lsb(knightMovesBB);
        Bitboard tempKnights = knights;
        while (tempKnights) {
            int src = lsb(tempKnights);
            if (KnightAttacks[src] & (1ULL << dest)) {
                moves.emplace_back(src, dest);
                break;
            }
            clear_bit(tempKnights, src);
        }
        clear_bit(knightMovesBB, dest);
    }

    return moves;
}

std::vector<std::pair<int, int>> generateBishopMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard bishops = board.getPieceBitboard(ChessPieceType::BISHOP, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;

    Bitboard bishopMovesBB = bishopMoves(bishops, ownPieces, board.allPieces);
    while (bishopMovesBB) {
        int dest = lsb(bishopMovesBB);
        Bitboard tempBishops = bishops;
        while (tempBishops) {
            int src = lsb(tempBishops);
            if (bishopAttacks(src, board.allPieces) & (1ULL << dest)) {
                moves.emplace_back(src, dest);
                break;
            }
            clear_bit(tempBishops, src);
        }
        clear_bit(bishopMovesBB, dest);
    }

    return moves;
}

std::vector<std::pair<int, int>> generateRookMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard rooks = board.getPieceBitboard(ChessPieceType::ROOK, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;

    Bitboard rookMovesBB = rookMoves(rooks, ownPieces, board.allPieces);
    while (rookMovesBB) {
        int dest = lsb(rookMovesBB);
        Bitboard tempRooks = rooks;
        while (tempRooks) {
            int src = lsb(tempRooks);
            if (rookAttacks(src, board.allPieces) & (1ULL << dest)) {
                moves.emplace_back(src, dest);
                break;
            }
            clear_bit(tempRooks, src);
        }
        clear_bit(rookMovesBB, dest);
    }

    return moves;
}

std::vector<std::pair<int, int>> generateQueenMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard queens = board.getPieceBitboard(ChessPieceType::QUEEN, color);
    Bitboard ownPieces = (color == ChessPieceColor::WHITE) ? board.whitePieces : board.blackPieces;

    Bitboard queenMovesBB = queenMoves(queens, ownPieces, board.allPieces);
    while (queenMovesBB) {
        int dest = lsb(queenMovesBB);
        Bitboard tempQueens = queens;
        while (tempQueens) {
            int src = lsb(tempQueens);
            if (queenAttacks(src, board.allPieces) & (1ULL << dest)) {
                moves.emplace_back(src, dest);
                break;
            }
            clear_bit(tempQueens, src);
        }
        clear_bit(queenMovesBB, dest);
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

    addCastlingMovesBitboard(board, board.turn);

    board.whiteChecked = IsKingInCheck(board, ChessPieceColor::WHITE);
    board.blackChecked = IsKingInCheck(board, ChessPieceColor::BLACK);
}
