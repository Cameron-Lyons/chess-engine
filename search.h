#ifndef SEARCH_H
#define SEARCH_H

#include "ChessBoard.h"
#include "ValidMoves.h"
#include "Evaluation.h"
#include <vector>
#include <algorithm>

// Structure to hold a move with its score for ordering
struct ScoredMove {
    std::pair<int, int> move;
    int score;
    
    ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
    
    // For sorting (higher scores first)
    bool operator<(const ScoredMove& other) const {
        return score > other.score;
    }
};

// Function to get the MVV-LVA (Most Valuable Victim - Least Valuable Attacker) score
int getMVVLVA_Score(const Board& board, int srcPos, int destPos) {
    const Piece& attacker = board.squares[srcPos].Piece;
    const Piece& victim = board.squares[destPos].Piece;
    
    if (victim.PieceType == NONE) return 0;
    
    // MVV-LVA = victim_value * 10 + attacker_value
    // This ensures captures are ordered by victim value first, then attacker value
    return victim.PieceValue * 10 - attacker.PieceValue;
}

// Function to check if a move is a capture
bool isCapture(const Board& board, int srcPos, int destPos) {
    return board.squares[destPos].Piece.PieceType != NONE;
}

// Function to check if a move gives check
bool givesCheck(const Board& board, int srcPos, int destPos) {
    // Make a temporary move and check if it gives check
    Board tempBoard = board;
    tempBoard.squares[destPos].Piece = tempBoard.squares[srcPos].Piece;
    tempBoard.squares[srcPos].Piece = Piece();
    
    // Generate moves for the opponent and see if any can capture the king
    GenValidMoves(tempBoard);
    
    // Find the king of the side that just moved
    ChessPieceColor kingColor = (board.turn == WHITE) ? BLACK : WHITE;
    int kingPos = (kingColor == WHITE) ? WhiteKingPosition : BlackKingPosition;
    
    // Check if any piece can capture the king
    for (int i = 0; i < 64; i++) {
        if (tempBoard.squares[i].Piece.PieceType != NONE && 
            tempBoard.squares[i].Piece.PieceColor != kingColor) {
            for (int move : tempBoard.squares[i].Piece.ValidMoves) {
                if (move == kingPos) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Function to get the history score for a move (for history heuristic)
int getHistoryScore(const std::vector<std::vector<int>>& historyTable, int srcPos, int destPos) {
    return historyTable[srcPos][destPos];
}

// Function to score moves for ordering
std::vector<ScoredMove> scoreMoves(const Board& board, const std::vector<std::pair<int, int>>& moves, 
                                  const std::vector<std::vector<int>>& historyTable) {
    std::vector<ScoredMove> scoredMoves;
    
    for (const auto& move : moves) {
        int srcPos = move.first;
        int destPos = move.second;
        int score = 0;
        
        // 1. Captures (MVV-LVA ordering)
        if (isCapture(board, srcPos, destPos)) {
            score = 10000 + getMVVLVA_Score(board, srcPos, destPos);
        }
        // 2. Checks
        else if (givesCheck(board, srcPos, destPos)) {
            score = 9000;
        }
        // 3. Promotions
        else if (board.squares[srcPos].Piece.PieceType == PAWN && 
                 (destPos < 8 || destPos >= 56)) {
            score = 8000;
        }
        // 4. History heuristic
        else {
            score = getHistoryScore(historyTable, srcPos, destPos);
        }
        
        scoredMoves.emplace_back(move, score);
    }
    
    return scoredMoves;
}

// Function to update history table
void updateHistoryTable(std::vector<std::vector<int>>& historyTable, int srcPos, int destPos, int depth) {
    historyTable[srcPos][destPos] += depth * depth;
}

// Function declarations
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable);
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);

// Implementation
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate) {
    // Initialize history table (64x64 for all possible moves)
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    // Search for mate in 1-4 moves
    for (int depth = 1; depth <= 4; depth++) {
        int result = AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == WHITE), historyTable);
        
        if (result >= 9000) {
            // White has a winning advantage
            if (movingSide == WHITE) {
                BlackMate = true;
                return true;
            }
        } else if (result <= -9000) {
            // Black has a winning advantage
            if (movingSide == BLACK) {
                WhiteMate = true;
                return true;
            }
        }
    }
    
    // Check for stalemate
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, movingSide);
    if (moves.empty()) {
        StaleMate = true;
        return true;
    }
    
    return false;
}

// Simple alpha-beta search implementation
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable) {
    if (depth == 0) {
        // Return evaluation score using the new comprehensive evaluation
        return evaluatePosition(board);
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? WHITE : BLACK);
    
    if (moves.empty()) {
        // No moves available - checkmate or stalemate
        return maximizingPlayer ? -10000 : 10000;
    }
    
    // Score and sort moves for better ordering
    std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    if (maximizingPlayer) {
        int maxEval = -10000;
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            
            // Update history table for good moves
            if (eval > alpha) {
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = 10000;
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable);
            minEval = std::min(minEval, eval);
            beta = std::min(beta, eval);
            
            // Update history table for good moves
            if (eval < beta) {
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            
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

// Function to find the best move using alpha-beta search with move ordering
std::pair<int, int> findBestMove(Board& board, int depth) {
    // Initialize history table
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.empty()) {
        return {-1, -1}; // No moves available
    }
    
    // Score and sort moves for better ordering
    std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int bestEval = (board.turn == WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = moves[0];
    
    for (const auto& scoredMove : scoredMoves) {
        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.MovePiece(newBoard, move.first, move.second, false);
        
        int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, 
                                 (board.turn == BLACK), historyTable);
        
        if (board.turn == WHITE) {
            if (eval > bestEval) {
                bestEval = eval;
                bestMove = move;
            }
        } else {
            if (eval < bestEval) {
                bestEval = eval;
                bestMove = move;
            }
        }
    }
    
    return bestMove;
}

#endif // SEARCH_H