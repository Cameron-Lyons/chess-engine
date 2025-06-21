#ifndef SEARCH_H
#define SEARCH_H

#include "ChessBoard.h"
#include "ValidMoves.h"
#include "Evaluation.h"
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <random>
#include <cstdint>
#include <map>
#include <string>

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

// Structure to hold search results
struct SearchResult {
    std::pair<int, int> bestMove;
    int score;
    int depth;
    int nodes;
    int timeMs;
    
    SearchResult() : bestMove({-1, -1}), score(0), depth(0), nodes(0), timeMs(0) {}
};

// Global node counter for search statistics
static int nodeCount = 0;

// Zobrist hashing
extern uint64_t ZobristTable[64][12]; // 64 squares, 12 piece types (6 per color)
extern uint64_t ZobristBlackToMove;

// Initialize Zobrist table
inline void InitZobrist() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    std::mt19937_64 rng(202406); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint64_t> dist;
    for (int sq = 0; sq < 64; ++sq) {
        for (int pt = 0; pt < 12; ++pt) {
            ZobristTable[sq][pt] = dist(rng);
        }
    }
    ZobristBlackToMove = dist(rng);
}

// Piece type to Zobrist index
inline int pieceToZobristIndex(const Piece& p) {
    if (p.PieceType == ChessPieceType::NONE) return -1;
    return (static_cast<int>(p.PieceType) - 1) + (p.PieceColor == ChessPieceColor::BLACK ? 6 : 0);
}

// Compute Zobrist hash for a board
inline uint64_t ComputeZobrist(const Board& board) {
    uint64_t h = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int idx = pieceToZobristIndex(board.squares[sq].Piece);
        if (idx >= 0) h ^= ZobristTable[sq][idx];
    }
    if (board.turn == ChessPieceColor::BLACK) h ^= ZobristBlackToMove;
    return h;
}

// Transposition table entry
struct TTEntry {
    int depth;
    int value;
    int flag; // 0 = exact, -1 = alpha, 1 = beta
};

// Transposition table
extern std::unordered_map<uint64_t, TTEntry> TransTable;

// Function to get the MVV-LVA (Most Valuable Victim - Least Valuable Attacker) score
int getMVVLVA_Score(const Board& board, int srcPos, int destPos) {
    const Piece& attacker = board.squares[srcPos].Piece;
    const Piece& victim = board.squares[destPos].Piece;
    
    if (victim.PieceType == ChessPieceType::NONE) return 0;
    
    // MVV-LVA = victim_value * 10 + attacker_value
    // This ensures captures are ordered by victim value first, then attacker value
    return victim.PieceValue * 10 - attacker.PieceValue;
}

// Function to check if a move is a capture
bool isCapture(const Board& board, int /* srcPos */, int destPos) {
    return board.squares[destPos].Piece.PieceType != ChessPieceType::NONE;
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
    ChessPieceColor kingColor = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    int kingPos = (kingColor == ChessPieceColor::WHITE) ? WhiteKingPosition : BlackKingPosition;
    
    // Check if any piece can capture the king
    for (int i = 0; i < 64; i++) {
        if (tempBoard.squares[i].Piece.PieceType != ChessPieceType::NONE && 
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
        else if (board.squares[srcPos].Piece.PieceType == ChessPieceType::PAWN && 
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

// Function to check if time limit has been reached
bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

// Function declarations
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable,
                   const std::chrono::steady_clock::time_point& startTime, int timeLimitMs);
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);

// Opening book: maps FEN (without move clocks) to best move in coordinate notation (e.g., "e2e4")
extern std::map<std::string, std::string> OpeningBook;

// Function to get a book move for a given FEN
inline std::string getBookMove(const std::string& fen) {
    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) return it->second;
    return "";
}

// Implementation
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate) {
    // Initialize history table (64x64 for all possible moves)
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    // Search for mate in 1-4 moves
    for (int depth = 1; depth <= 4; depth++) {
        int result = AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == ChessPieceColor::WHITE), historyTable, std::chrono::steady_clock::now(), std::numeric_limits<int>::max());
        
        if (result >= 9000) {
            // White has a winning advantage
            if (movingSide == ChessPieceColor::WHITE) {
                BlackMate = true;
                return true;
            }
        } else if (result <= -9000) {
            // Black has a winning advantage
            if (movingSide == ChessPieceColor::BLACK) {
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

// Simple alpha-beta search implementation with time management
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable,
                   const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    // Check time limit
    if (isTimeUp(startTime, timeLimitMs)) {
        return 0; // Return neutral score if time is up
    }
    
    nodeCount++; // Increment node counter
    
    // Transposition table lookup
    uint64_t hash = ComputeZobrist(board);
    auto ttIt = TransTable.find(hash);
    if (ttIt != TransTable.end()) {
        const TTEntry& entry = ttIt->second;
        if (entry.depth >= depth) {
            if (entry.flag == 0) return entry.value; // exact
            if (entry.flag == -1 && entry.value <= alpha) return alpha; // alpha
            if (entry.flag == 1 && entry.value >= beta) return beta; // beta
        }
    }
    
    if (depth == 0) {
        int eval = evaluatePosition(board);
        // Store in TT as exact
        TransTable[hash] = {depth, eval, 0};
        return eval;
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    
    if (moves.empty()) {
        // No moves available - checkmate or stalemate
        int mateScore = maximizingPlayer ? -10000 : 10000;
        TransTable[hash] = {depth, mateScore, 0};
        return mateScore;
    }
    
    // Score and sort moves for better ordering
    std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -10000 : 10000;
    int flag = 0;
    
    if (maximizingPlayer) {
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable, startTime, timeLimitMs);
            if (isTimeUp(startTime, timeLimitMs)) return 0;
            if (eval > bestValue) bestValue = eval;
            if (eval > alpha) alpha = eval;
            if (eval > origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
            if (beta <= alpha) break;
        }
        // Set TT flag
        if (bestValue <= origAlpha) flag = -1; // alpha
        else if (bestValue >= beta) flag = 1; // beta
        else flag = 0; // exact
    } else {
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            int eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable, startTime, timeLimitMs);
            if (isTimeUp(startTime, timeLimitMs)) return 0;
            if (eval < bestValue) bestValue = eval;
            if (eval < beta) beta = eval;
            if (eval < origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
            if (beta <= alpha) break;
        }
        // Set TT flag
        if (bestValue >= beta) flag = 1; // beta
        else if (bestValue <= origAlpha) flag = -1; // alpha
        else flag = 0; // exact
    }
    // Store in TT
    TransTable[hash] = {depth, bestValue, flag};
    return bestValue;
}

// Get all possible moves for a given color
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> moves;
    GenValidMoves(board);
    
    for (int i = 0; i < 64; i++) {
        Square& square = board.squares[i];
        if (square.Piece.PieceType != ChessPieceType::NONE && square.Piece.PieceColor == color) {
            for (int dest : square.Piece.ValidMoves) {
                moves.push_back({i, dest});
            }
        }
    }
    
    return moves;
}

// Function to find the best move using iterative deepening
SearchResult iterativeDeepening(Board& board, int maxDepth, int timeLimitMs) {
    SearchResult result;
    auto startTime = std::chrono::steady_clock::now();
    
    // Initialize history table
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.empty()) {
        result.bestMove = {-1, -1}; // No moves available
        return result;
    }
    
    // Start with depth 1 and increase
    for (int depth = 1; depth <= maxDepth; depth++) {
        // Check if we have enough time for this depth
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
        if (elapsed.count() > timeLimitMs * 0.8) { // Use 80% of time limit
            break;
        }
        
        // Reset node counter for this iteration
        int nodesAtStart = nodeCount;
        
        // Score and sort moves for better ordering
        std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
        std::sort(scoredMoves.begin(), scoredMoves.end());
        
        int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
        std::pair<int, int> bestMoveThisDepth = moves[0];
        
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.MovePiece(newBoard, move.first, move.second, false);
            
            int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, 
                                     (board.turn == ChessPieceColor::BLACK), historyTable, startTime, timeLimitMs);
            
            // Check if time is up
            if (isTimeUp(startTime, timeLimitMs)) {
                break;
            }
            
            if (board.turn == ChessPieceColor::WHITE) {
                if (eval > bestEval) {
                    bestEval = eval;
                    bestMoveThisDepth = move;
                }
            } else {
                if (eval < bestEval) {
                    bestEval = eval;
                    bestMoveThisDepth = move;
                }
            }
        }
        
        // Check if time is up
        if (isTimeUp(startTime, timeLimitMs)) {
            break;
        }
        
        // Update result with this depth's findings
        result.bestMove = bestMoveThisDepth;
        result.score = bestEval;
        result.depth = depth;
        result.nodes = nodeCount - nodesAtStart;
        
        // Print search info
        // auto iterationTime = std::chrono::steady_clock::now();
        // auto iterationElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(iterationTime - startTime);
        
        // Comment out or remove the 'info depth ...' print statement
        // std::cout << "info depth " << depth 
        //           << " score " << result.score
        //           << " time " << iterationElapsed.count()
        //           << " nodes " << result.nodes
        //           << " pv " << (char)('a' + srcCol) << (srcRow + 1) 
        //           << (char)('a' + destCol) << (destRow + 1) << "\n";
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    return result;
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
    
    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = moves[0];
    
    for (const auto& scoredMove : scoredMoves) {
        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.MovePiece(newBoard, move.first, move.second, false);
        
        int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, 
                                 (board.turn == ChessPieceColor::BLACK), historyTable, 
                                 std::chrono::steady_clock::now(), std::numeric_limits<int>::max());
        
        if (board.turn == ChessPieceColor::WHITE) {
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