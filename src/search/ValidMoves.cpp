#include "ValidMoves.h"
#include "../core/BitboardMoves.h"
#include <iostream>

bool BlackAttackBoard[64] = {false};
bool WhiteAttackBoard[64] = {false};
int BlackKingPosition = 0;
int WhiteKingPosition = 0;

bool IsKingInCheck(const Board& board, ChessPieceColor color) {
    int kingSq = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING && board.squares[i].piece.PieceColor == color) {
            kingSq = i;
            break;
        }
    }
    if (kingSq == -1) return false;

    Bitboard occ = board.allPieces;
    Bitboard enemyPawns = board.getPieceBitboard(ChessPieceType::PAWN, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyKnights = board.getPieceBitboard(ChessPieceType::KNIGHT, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyBishops = board.getPieceBitboard(ChessPieceType::BISHOP, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyRooks = board.getPieceBitboard(ChessPieceType::ROOK, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyQueens = board.getPieceBitboard(ChessPieceType::QUEEN, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);
    Bitboard enemyKing = board.getPieceBitboard(ChessPieceType::KING, color == ChessPieceColor::WHITE ? ChessPieceColor::BLACK : ChessPieceColor::WHITE);

    Bitboard pawnAttacksBB = 0;
    if (color == ChessPieceColor::WHITE) {
        pawnAttacksBB = pawnAttacks(ChessPieceColor::BLACK, kingSq);
    } else {
        pawnAttacksBB = pawnAttacks(ChessPieceColor::WHITE, kingSq);
    }
    if (pawnAttacksBB & enemyPawns) return true;

    if (KnightAttacks[kingSq] & enemyKnights) return true;

    if (bishopAttacks(kingSq, occ) & (enemyBishops | enemyQueens)) return true;

    if (rookAttacks(kingSq, occ) & (enemyRooks | enemyQueens)) return true;

    if (KingAttacks[kingSq] & enemyKing) return true;

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
    tempBoard.updateBitboards(); // Ensure bitboards are properly updated

    if (IsKingInCheck(tempBoard, piece.PieceColor)) {
        return false;
    }
    return true;
}

void addCastlingMovesBitboard(Board& board, ChessPieceColor color) {
    int kingStart = (color == ChessPieceColor::WHITE) ? 4 : 60;
    Piece& king = board.squares[kingStart].piece;
    if (king.PieceType != ChessPieceType::KING || king.moved) return;

    Bitboard occ = board.allPieces;
    bool canCastleKingside = false, canCastleQueenside = false;
    if (color == ChessPieceColor::WHITE && board.whiteCanCastle) {
        if (!(occ & (1ULL << 5)) && !(occ & (1ULL << 6))) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(4, 5);
                temp.updateBitboards(); // Ensure bitboards are properly updated
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(5, 6);
                    temp.updateBitboards(); // Ensure bitboards are properly updated
                    if (!IsKingInCheck(temp, color)) {
                        canCastleKingside = true;
                    }
                }
            }
        }
        if (!(occ & (1ULL << 1)) && !(occ & (1ULL << 2)) && !(occ & (1ULL << 3))) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(4, 3);
                temp.updateBitboards(); // Ensure bitboards are properly updated
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(3, 2);
                    temp.updateBitboards(); // Ensure bitboards are properly updated
                    if (!IsKingInCheck(temp, color)) {
                        canCastleQueenside = true;
                    }
                }
            }
        }
    } else if (color == ChessPieceColor::BLACK && board.blackCanCastle) {
        if (!(occ & (1ULL << 61)) && !(occ & (1ULL << 62))) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(60, 61);
                temp.updateBitboards(); // Ensure bitboards are properly updated
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(61, 62);
                    temp.updateBitboards(); // Ensure bitboards are properly updated
                    if (!IsKingInCheck(temp, color)) {
                        canCastleKingside = true;
                    }
                }
            }
        }
        if (!(occ & (1ULL << 57)) && !(occ & (1ULL << 58)) && !(occ & (1ULL << 59))) {
            if (!IsKingInCheck(board, color)) {
                Board temp = board;
                temp.movePiece(60, 59);
                temp.updateBitboards(); // Ensure bitboards are properly updated
                if (!IsKingInCheck(temp, color)) {
                    temp.movePiece(59, 58);
                    temp.updateBitboards(); // Ensure bitboards are properly updated
                    if (!IsKingInCheck(temp, color)) {
                        canCastleQueenside = true;
                    }
                }
            }
        }
    }
    if (canCastleKingside) {
        king.ValidMoves.push_back((color == ChessPieceColor::WHITE) ? 6 : 62);
    }
    if (canCastleQueenside) {
        king.ValidMoves.push_back((color == ChessPieceColor::WHITE) ? 2 : 58);
    }
}

std::vector<std::pair<int, int>> generatePawnMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    Bitboard pawns = board.getPieceBitboard(ChessPieceType::PAWN, color);
    Bitboard empty = ~board.allPieces;
    Bitboard enemyPieces = (color == ChessPieceColor::WHITE) ? board.blackPieces : board.whitePieces;
    
    Bitboard singlePushes;
    if (color == ChessPieceColor::WHITE) {
        singlePushes = (pawns << 8) & empty;
    } else {
        singlePushes = (pawns >> 8) & empty;
    }
    
    while (singlePushes) {
        int dest = lsb(singlePushes);
        int src = (color == ChessPieceColor::WHITE) ? dest - 8 : dest + 8;
        moves.push_back({src, dest});
        clear_bit(singlePushes, dest);
    }
    
    Bitboard doublePushes;
    if (color == ChessPieceColor::WHITE) {
        Bitboard startingPawns = pawns & 0x000000000000FF00ULL; // Rank 2
        Bitboard singlePushSquares = (startingPawns << 8) & empty;
        doublePushes = (singlePushSquares << 8) & empty;
    } else {
        Bitboard startingPawns = pawns & 0x00FF000000000000ULL; // Rank 7
        Bitboard singlePushSquares = (startingPawns >> 8) & empty;
        doublePushes = (singlePushSquares >> 8) & empty;
    }
    
    while (doublePushes) {
        int dest = lsb(doublePushes);
        int src = (color == ChessPieceColor::WHITE) ? dest - 16 : dest + 16;
        moves.push_back({src, dest});
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
                moves.push_back({src, dest});
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
                moves.push_back({src, dest});
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
                moves.push_back({src, dest});
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
                moves.push_back({src, dest});
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
                moves.push_back({src, dest});
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
            moves.push_back({src, dest});
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

    for (int i = 0; i < 64; i++) {
        BlackAttackBoard[i] = false;
        WhiteAttackBoard[i] = false;
    }

    for (int i = 0; i < 64; i++) {
        board.squares[i].piece.ValidMoves.clear();
    }

    for (int i = 0; i < 64; i++) {
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