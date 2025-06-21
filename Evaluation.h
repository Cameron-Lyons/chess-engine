#ifndef EVALUATION_H
#define EVALUATION_H

#include "ChessBoard.h"
#include "ChessPiece.h"

const int PAWN_TABLE[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

const int KNIGHT_TABLE[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

const int BISHOP_TABLE[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

const int ROOK_TABLE[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

const int QUEEN_TABLE[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

const int KING_TABLE[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {
    int value = 0;
    
    switch (pieceType) {
        case ChessPieceType::PAWN:
            value = PAWN_TABLE[position];
            break;
        case ChessPieceType::KNIGHT:
            value = KNIGHT_TABLE[position];
            break;
        case ChessPieceType::BISHOP:
            value = BISHOP_TABLE[position];
            break;
        case ChessPieceType::ROOK:
            value = ROOK_TABLE[position];
            break;
        case ChessPieceType::QUEEN:
            value = QUEEN_TABLE[position];
            break;
        case ChessPieceType::KING:
            value = KING_TABLE[position];
            break;
        default:
            value = 0;
            break;
    }
    
    if (color == ChessPieceColor::BLACK) {
        value = -value;
    }
    
    return value;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;
    
    for (int col = 0; col < 8; col++) {
        int whitePawns = 0, blackPawns = 0;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[pos].Piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                } else {
                    blackPawns++;
                }
            }
        }
        if (whitePawns > 1) score -= 20 * (whitePawns - 1);
        if (blackPawns > 1) score += 20 * (blackPawns - 1);
    }
    
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col) continue;
                    for (int adjRow = 0; adjRow < 8; adjRow++) {
                        int adjPos = adjRow * 8 + adjCol;
                        if (board.squares[adjPos].Piece.PieceType == ChessPieceType::PAWN && 
                            board.squares[adjPos].Piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) break;
                }
                if (isolated) {
                    if (board.squares[pos].Piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 30;
                    } else {
                        score += 30;
                    }
                }
            }
        }
    }
    
    return score;
}

int evaluateMobility(const Board& board) {
    int whiteMobility = 0, blackMobility = 0;
    
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType != ChessPieceType::NONE) {
            int moves = board.squares[i].Piece.ValidMoves.size();
            if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
                whiteMobility += moves;
            } else {
                blackMobility += moves;
            }
        }
    }
    
    return (whiteMobility - blackMobility) * 10;
}

int evaluateCenterControl(const Board& board) {
    int score = 0;
    
    int centerSquares[] = {27, 28, 35, 36};
    
    for (int square : centerSquares) {
        if (board.squares[square].Piece.PieceType != ChessPieceType::NONE) {
            if (board.squares[square].Piece.PieceColor == ChessPieceColor::WHITE) {
                score += 30;
            } else {
                score -= 30;
            }
        }
    }
    
    return score;
}

int evaluatePosition(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int materialValue = piece.PieceValue;
            int positionalValue = getPieceSquareValue(piece.PieceType, i, piece.PieceColor);
            
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += materialValue + positionalValue;
            } else {
                score -= materialValue + positionalValue;
            }
        }
    }
    
    score += evaluatePawnStructure(board);
    score += evaluateMobility(board);
    score += evaluateCenterControl(board);
    
    return score;
}

#endif
