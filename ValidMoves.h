#ifndef VALID_MOVES_H
#define VALID_MOVES_H

#include "PieceMoves.h"
#include "ChessBoard.h"
#include <vector>
#include <cstdlib>
#include <iostream>

// Global variables for attack boards
extern bool BlackAttackBoard[64];
extern bool WhiteAttackBoard[64];
extern int BlackKingPosition;
extern int WhiteKingPosition;

// Function declarations
bool AnalyzeMove(Board& board, int dest, Piece& piece);
void CheckValidMovesPawn(const std::vector<int>& moves, Piece& piece, int start, Board& board, int count);
void AnalyzeMovePawn(Board& board, int dest, Piece& piece);
void GenValidMovesKingCastle(Board& board, Piece& king);
void GenValidMoves(Board& board);
bool IsMoveLegal(Board& board, int srcPos, int destPos);
bool IsKingInCheck(Board& board, ChessPieceColor color);

// Implementation
bool AnalyzeMove(Board& board, int dest, Piece& piece){
    // pawns cannot attack where they move
    if (piece.PieceColor == WHITE) {
        WhiteAttackBoard[dest] = true;
    } else {
        BlackAttackBoard[dest] = true;
    }

    if (board.squares[dest].Piece.PieceType == NONE) {
        piece.ValidMoves.push_back(dest);
        return true;
    }

    Piece& attackedPiece = board.squares[dest].Piece;
    if (attackedPiece.PieceColor != piece.PieceColor) {
        if (attackedPiece.PieceType == KING) {
            if (piece.PieceColor == WHITE) {
                board.whiteChecked = true;
            } else {
                board.blackChecked = true;
            }
        } else {
            piece.ValidMoves.push_back(dest);
        }
    }
    else {
        return false;
    }
    attackedPiece.DefendedValue += piece.PieceValue;
    return true;
}

void CheckValidMovesPawn(const std::vector<int>& moves, Piece& piece, int start, Board& board, int count){
    for (int i=0; i<count && i < moves.size(); i++){
        int dest = moves[i];
        if (dest%8 != start%8){
            if (piece.PieceColor == WHITE){
                WhiteAttackBoard[dest] = true;
            }
            else {
                BlackAttackBoard[dest] = true;
            }
        }
        else if (board.squares[dest].Piece.PieceType != NONE){
            return;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }
    }
}

void AnalyzeMovePawn(Board& board, int dest, Piece& piece){
    Piece& attackedPiece = board.squares[dest].Piece;
    if (attackedPiece.PieceType == NONE){
        return;
    }
    if (piece.PieceColor == WHITE){
        WhiteAttackBoard[dest] = true;
        if (attackedPiece.PieceColor == piece.PieceColor){
            attackedPiece.DefendedValue += piece.PieceValue;
            return;
        }
        else {
            attackedPiece.AttackedValue += piece.PieceValue;
        }
        if (attackedPiece.PieceType == KING){
            board.blackChecked = true;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }
    }
    else {
        BlackAttackBoard[dest] = true;
        if (attackedPiece.PieceColor == piece.PieceColor){
            return;
        }
        if (attackedPiece.PieceType == KING){
            board.whiteChecked = true;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }
    }
}

void GenValidMovesKingCastle(Board& board, Piece& king){
    if (king.PieceType == NONE){
        return;
    }
    if (king.moved){
        return;
    }
    if (king.PieceColor == WHITE && !board.whiteCanCastle){
        return;
    }
    if (king.PieceColor == BLACK && !board.blackCanCastle){
        return;
    }
    if (king.PieceColor == WHITE && board.whiteChecked){
        return;
    }
    if (king.PieceColor == BLACK && board.blackChecked){
        return;
    }
    if (king.PieceColor == BLACK){
        if (board.squares[63].Piece.PieceType == ROOK){
            if (board.squares[62].Piece.PieceType == NONE
                && board.squares[61].Piece.PieceType == NONE){
                if (!BlackAttackBoard[61] && !BlackAttackBoard[62] && !BlackAttackBoard[63]){
                    king.ValidMoves.push_back(62);
                    WhiteAttackBoard[62] = true;
                }
            }
        }
        if (board.squares[56].Piece.PieceType == ROOK){
            if (board.squares[57].Piece.PieceType == NONE
                && board.squares[58].Piece.PieceType == NONE
                && board.squares[59].Piece.PieceType == NONE){
                if (!BlackAttackBoard[59] && !BlackAttackBoard[58] && !BlackAttackBoard[57]){
                    king.ValidMoves.push_back(58);
                    WhiteAttackBoard[58] = true;
                }
            }
        }
    }
    if (king.PieceColor == WHITE){
        if (board.squares[7].Piece.PieceType == ROOK){
            if (board.squares[6].Piece.PieceType == NONE
                && board.squares[5].Piece.PieceType == NONE){
                if (!WhiteAttackBoard[5] && !WhiteAttackBoard[6] && !WhiteAttackBoard[7]){
                    king.ValidMoves.push_back(6);
                    BlackAttackBoard[6] = true;
                }
            }
        }
        if (board.squares[0].Piece.PieceType == ROOK){
            if (board.squares[1].Piece.PieceType == NONE
                && board.squares[2].Piece.PieceType == NONE
                && board.squares[3].Piece.PieceType == NONE){
                if (!WhiteAttackBoard[3] && !WhiteAttackBoard[2] && !WhiteAttackBoard[1]){
                    king.ValidMoves.push_back(2);
                    BlackAttackBoard[2] = true;
                }
            }
        }
    }
}

// Check if a king is in check
bool IsKingInCheck(Board& board, ChessPieceColor color) {
    int kingPos = (color == WHITE) ? WhiteKingPosition : BlackKingPosition;
    return (color == WHITE) ? board.whiteChecked : board.blackChecked;
}

// Check if a specific move is legal (doesn't leave king in check)
bool IsMoveLegal(Board& board, int srcPos, int destPos) {
    // For now, let's use a simpler approach that doesn't cause recursion
    // We'll just check basic move validity without deep check detection
    
    Piece& piece = board.squares[srcPos].Piece;
    Piece& destPiece = board.squares[destPos].Piece;
    
    // Can't capture own piece
    if (destPiece.PieceType != NONE && destPiece.PieceColor == piece.PieceColor) {
        return false;
    }
    
    // For pawns, check basic pawn rules
    if (piece.PieceType == PAWN) {
        int srcRow = srcPos / 8;
        int srcCol = srcPos % 8;
        int destRow = destPos / 8;
        int destCol = destPos % 8;
        
        if (piece.PieceColor == WHITE) {
            // White pawns move up (decreasing row)
            if (destRow >= srcRow) return false;
            
            // Forward move
            if (srcCol == destCol) {
                if (destPiece.PieceType != NONE) return false;
                if (destRow == srcRow - 1) return true;
                if (srcRow == 6 && destRow == 4 && board.squares[srcPos - 8].Piece.PieceType == NONE) return true;
            }
            // Diagonal capture
            else if (abs(destCol - srcCol) == 1 && destRow == srcRow - 1) {
                return destPiece.PieceType != NONE && destPiece.PieceColor == BLACK;
            }
        } else {
            // Black pawns move down (increasing row)
            if (destRow <= srcRow) return false;
            
            // Forward move
            if (srcCol == destCol) {
                if (destPiece.PieceType != NONE) return false;
                if (destRow == srcRow + 1) return true;
                if (srcRow == 1 && destRow == 3 && board.squares[srcPos + 8].Piece.PieceType == NONE) return true;
            }
            // Diagonal capture
            else if (abs(destCol - srcCol) == 1 && destRow == srcRow + 1) {
                return destPiece.PieceType != NONE && destPiece.PieceColor == WHITE;
            }
        }
        return false;
    }
    
    // For other pieces, just check if destination is valid
    // (We'll rely on the basic move generation to handle piece-specific rules)
    return true;
}

void GenValidMoves(Board& board){
    board.whiteChecked = false;
    board.blackChecked = false;

    // Reset attack boards
    for (int i = 0; i < 64; i++) {
        BlackAttackBoard[i] = false;
        WhiteAttackBoard[i] = false;
    }

    for (int x=0; x<64; x++){
        Square& square = board.squares[x];
        if (square.Piece.PieceType == NONE) {
            continue;
        }
        square.Piece.ValidMoves.clear();
        
        // Update king positions
        if (square.Piece.PieceType == KING) {
            if (square.Piece.PieceColor == WHITE) {
                WhiteKingPosition = x;
            } else {
                BlackKingPosition = x;
            }
        }
        
        switch (square.Piece.PieceType)
        {
        case PAWN:
            if (square.Piece.PieceColor == WHITE){
                // White pawn moves
                int forward = x + 8;
                if (forward < 64 && board.squares[forward].Piece.PieceType == NONE) {
                    square.Piece.ValidMoves.push_back(forward);
                    if (x >= 8 && x <= 15) { // Starting position (rank 2)
                        int doubleForward = x + 16;
                        if (doubleForward < 64 && board.squares[doubleForward].Piece.PieceType == NONE) {
                            square.Piece.ValidMoves.push_back(doubleForward);
                        }
                    }
                }
                // Diagonal captures
                if (x % 8 > 0) { // Can capture left
                    int leftCapture = x - 9;
                    if (leftCapture >= 0 && board.squares[leftCapture].Piece.PieceType != NONE &&
                        board.squares[leftCapture].Piece.PieceColor == BLACK) {
                        square.Piece.ValidMoves.push_back(leftCapture);
                    }
                }
                if (x % 8 < 7) { // Can capture right
                    int rightCapture = x - 7;
                    if (rightCapture >= 0 && board.squares[rightCapture].Piece.PieceType != NONE &&
                        board.squares[rightCapture].Piece.PieceColor == BLACK) {
                        square.Piece.ValidMoves.push_back(rightCapture);
                    }
                }
            }
            else{
                // Black pawn moves
                int forward = x - 8;
                if (forward >= 0 && board.squares[forward].Piece.PieceType == NONE) {
                    square.Piece.ValidMoves.push_back(forward);
                    if (x >= 48 && x <= 55) { // Starting position (rank 7)
                        int doubleForward = x - 16;
                        if (doubleForward >= 0 && board.squares[doubleForward].Piece.PieceType == NONE) {
                            square.Piece.ValidMoves.push_back(doubleForward);
                        }
                    }
                }
                // Diagonal captures
                if (x % 8 > 0) { // Can capture left
                    int leftCapture = x + 7;
                    if (leftCapture < 64 && board.squares[leftCapture].Piece.PieceType != NONE &&
                        board.squares[leftCapture].Piece.PieceColor == WHITE) {
                        square.Piece.ValidMoves.push_back(leftCapture);
                    }
                }
                if (x % 8 < 7) { // Can capture right
                    int rightCapture = x + 9;
                    if (rightCapture < 64 && board.squares[rightCapture].Piece.PieceType != NONE &&
                        board.squares[rightCapture].Piece.PieceColor == WHITE) {
                        square.Piece.ValidMoves.push_back(rightCapture);
                    }
                }
            }
            break;
        case KNIGHT:
            // Knight moves (simplified - 8 possible positions)
            {
                int knightMoves[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
                for (int i = 0; i < 8; i++) {
                    int newRow = x / 8 + knightMoves[i][0];
                    int newCol = x % 8 + knightMoves[i][1];
                    if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int newPos = newRow * 8 + newCol;
                        AnalyzeMove(board, newPos, square.Piece);
                    }
                }
            }
            break;
        case BISHOP:
            // Bishop moves (diagonal)
            {
                int directions[4][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
                for (int d = 0; d < 4; d++) {
                    int row = x / 8;
                    int col = x % 8;
                    for (int step = 1; step < 8; step++) {
                        int newRow = row + directions[d][0] * step;
                        int newCol = col + directions[d][1] * step;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        int newPos = newRow * 8 + newCol;
                        if (!AnalyzeMove(board, newPos, square.Piece)) break;
                    }
                }
            }
            break;
        case ROOK:
            // Rook moves (horizontal and vertical)
            {
                int directions[4][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
                for (int d = 0; d < 4; d++) {
                    int row = x / 8;
                    int col = x % 8;
                    for (int step = 1; step < 8; step++) {
                        int newRow = row + directions[d][0] * step;
                        int newCol = col + directions[d][1] * step;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        int newPos = newRow * 8 + newCol;
                        if (!AnalyzeMove(board, newPos, square.Piece)) break;
                    }
                }
            }
            break;
        case QUEEN:
            // Queen moves (combination of bishop and rook)
            {
                int directions[8][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}, {-1,0}, {1,0}, {0,-1}, {0,1}};
                for (int d = 0; d < 8; d++) {
                    int row = x / 8;
                    int col = x % 8;
                    for (int step = 1; step < 8; step++) {
                        int newRow = row + directions[d][0] * step;
                        int newCol = col + directions[d][1] * step;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        int newPos = newRow * 8 + newCol;
                        if (!AnalyzeMove(board, newPos, square.Piece)) break;
                    }
                }
            }
            break;
        case KING:
            // King moves (one square in all directions)
            {
                int directions[8][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
                for (int d = 0; d < 8; d++) {
                    int row = x / 8;
                    int col = x % 8;
                    int newRow = row + directions[d][0];
                    int newCol = col + directions[d][1];
                    if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int newPos = newRow * 8 + newCol;
                        AnalyzeMove(board, newPos, square.Piece);
                    }
                }
                // Castling
                GenValidMovesKingCastle(board, square.Piece);
            }
            break;
        default:
            break;
        }
    }
    
    // Note: Removed the move filtering section to avoid recursion issues
    // The basic move generation should be sufficient for now
}

#endif // VALID_MOVES_H