#ifndef EVALUATION_H
#define EVALUATION_H

#include "ChessBoard.h"
#include "ChessPiece.h"

// Piece-square tables for positional evaluation
// Values are from white's perspective (positive = good for white)
// For black pieces, we'll negate these values

// Pawn position values (encourage pawn advancement and center control)
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

// Knight position values (favor center and outposts)
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

// Bishop position values (favor long diagonals)
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

// Rook position values (favor 7th rank and open files)
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

// Queen position values (moderate center control)
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

// King position values (safety in endgame, center in opening)
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

// Function to get piece-square table value
int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {
    int value = 0;
    
    switch (pieceType) {
        case PAWN:
            value = PAWN_TABLE[position];
            break;
        case KNIGHT:
            value = KNIGHT_TABLE[position];
            break;
        case BISHOP:
            value = BISHOP_TABLE[position];
            break;
        case ROOK:
            value = ROOK_TABLE[position];
            break;
        case QUEEN:
            value = QUEEN_TABLE[position];
            break;
        case KING:
            value = KING_TABLE[position];
            break;
        default:
            value = 0;
            break;
    }
    
    // For black pieces, flip the board and negate the value
    if (color == BLACK) {
        int flippedPosition = 63 - position;
        value = -value;
    }
    
    return value;
}

// Function to evaluate pawn structure
int evaluatePawnStructure(const Board& board) {
    int score = 0;
    
    // Count doubled pawns (penalty)
    for (int col = 0; col < 8; col++) {
        int whitePawns = 0, blackPawns = 0;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].Piece.PieceType == PAWN) {
                if (board.squares[pos].Piece.PieceColor == WHITE) {
                    whitePawns++;
                } else {
                    blackPawns++;
                }
            }
        }
        if (whitePawns > 1) score -= 20 * (whitePawns - 1); // Penalty for doubled pawns
        if (blackPawns > 1) score += 20 * (blackPawns - 1); // Bonus for opponent's doubled pawns
    }
    
    // Bonus for isolated pawns (penalty)
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].Piece.PieceType == PAWN) {
                bool isolated = true;
                // Check adjacent files for friendly pawns
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col) continue;
                    for (int adjRow = 0; adjRow < 8; adjRow++) {
                        int adjPos = adjRow * 8 + adjCol;
                        if (board.squares[adjPos].Piece.PieceType == PAWN && 
                            board.squares[adjPos].Piece.PieceColor == board.squares[pos].Piece.PieceColor) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) break;
                }
                if (isolated) {
                    if (board.squares[pos].Piece.PieceColor == WHITE) {
                        score -= 30; // Penalty for white isolated pawn
                    } else {
                        score += 30; // Bonus for black isolated pawn (penalty for white)
                    }
                }
            }
        }
    }
    
    return score;
}

// Function to evaluate mobility (number of legal moves)
int evaluateMobility(const Board& board) {
    int whiteMobility = 0, blackMobility = 0;
    
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType != NONE) {
            int moves = board.squares[i].Piece.ValidMoves.size();
            if (board.squares[i].Piece.PieceColor == WHITE) {
                whiteMobility += moves;
            } else {
                blackMobility += moves;
            }
        }
    }
    
    return (whiteMobility - blackMobility) * 10; // 10 points per move
}

// Function to evaluate center control
int evaluateCenterControl(const Board& board) {
    int score = 0;
    
    // Center squares: d4, e4, d5, e5
    int centerSquares[] = {27, 28, 35, 36}; // d4, e4, d5, e5
    
    for (int square : centerSquares) {
        // Check if any piece attacks this square
        if (board.squares[square].Piece.PieceType != NONE) {
            if (board.squares[square].Piece.PieceColor == WHITE) {
                score += 30; // White controls center
            } else {
                score -= 30; // Black controls center
            }
        }
    }
    
    return score;
}

// Main evaluation function
int evaluatePosition(const Board& board) {
    int score = 0;
    
    // Material and piece-square table evaluation
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != NONE) {
            int materialValue = piece.PieceValue;
            int positionalValue = getPieceSquareValue(piece.PieceType, i, piece.PieceColor);
            
            if (piece.PieceColor == WHITE) {
                score += materialValue + positionalValue;
            } else {
                score -= materialValue + positionalValue;
            }
        }
    }
    
    // Add positional bonuses
    score += evaluatePawnStructure(board);
    score += evaluateMobility(board);
    score += evaluateCenterControl(board);
    
    return score;
}

#endif // EVALUATION_H
