#ifndef SEARCH_H
#define SEARCH_H

#include "ChessBoard.h"
#include "ValidMoves.h"
#include <vector>

// Function declarations
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer);
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);

// Implementation
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate){
    bool foundNonCheckBlack = false;
    bool foundNonCheckWhite = false;

    // Generate all valid moves for the current position
    GenValidMoves(board);
    
    // Check if any piece has valid moves
    for(int i = 0; i < 64; i++){
        Square& square = board.squares[i];
        if (square.Piece.PieceType == NONE){
            continue;
        }
        if (square.Piece.PieceColor != movingSide){
            continue;
        } 
        
        // If any piece has valid moves, the game is not stalemate
        if (!square.Piece.ValidMoves.empty()) {
            if (movingSide == WHITE) {
                foundNonCheckWhite = true;
            } else {
                foundNonCheckBlack = true;
            }
        }
    }
    
    // Determine game state
    if (movingSide == WHITE) {
        if (board.whiteChecked && !foundNonCheckWhite) {
            BlackMate = true;
            return true;
        } else if (!board.whiteChecked && !foundNonCheckWhite) {
            StaleMate = true;
            return true;
        }
    } else {
        if (board.blackChecked && !foundNonCheckBlack) {
            WhiteMate = true;
            return true;
        } else if (!board.blackChecked && !foundNonCheckBlack) {
            StaleMate = true;
            return true;
        }
    }
    
    return false;
}

// Simple alpha-beta search implementation
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer) {
    if (depth == 0) {
        // Return evaluation score
        // This is a simplified evaluation - you should implement proper evaluation
        int score = 0;
        for (int i = 0; i < 64; i++) {
            Piece& piece = board.squares[i].Piece;
            if (piece.PieceType != NONE) {
                int value = piece.PieceValue;
                if (piece.PieceColor == BLACK) value = -value;
                score += value;
            }
        }
        return score;
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? WHITE : BLACK);
    
    if (moves.empty()) {
        // No moves available - checkmate or stalemate
        return maximizingPlayer ? -10000 : 10000;
    }
    
    if (maximizingPlayer) {
        int maxEval = -10000;
        for (const auto& move : moves) {
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = 10000;
        for (const auto& move : moves) {
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true);
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

// Get all possible moves for a given color
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    GenValidMoves(board);
    
    for (int i = 0; i < 64; i++) {
        Square& square = board.squares[i];
        if (square.Piece.PieceType != NONE && square.Piece.PieceColor == color) {
            for (int dest : square.Piece.ValidMoves) {
                moves.push_back({i, dest});
            }
        }
    }
    
    return moves;
}

#endif // SEARCH_H