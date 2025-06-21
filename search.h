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
#include <iostream>

struct ScoredMove {
    std::pair<int, int> move;
    int score;
    
    ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
    
    bool operator<(const ScoredMove& other) const {
        return score > other.score;
    }
};

struct SearchResult {
    std::pair<int, int> bestMove;
    int score;
    int depth;
    int nodes;
    int timeMs;
    
    SearchResult() : bestMove({-1, -1}), score(0), depth(0), nodes(0), timeMs(0) {}
};

static int nodeCount = 0;

extern uint64_t ZobristTable[64][12]; // 64 squares, 12 piece types (6 per color)
extern uint64_t ZobristBlackToMove;

inline void InitZobrist() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    std::mt19937_64 rng(202406);
    std::uniform_int_distribution<uint64_t> dist;
    for (int sq = 0; sq < 64; ++sq) {
        for (int pt = 0; pt < 12; ++pt) {
            ZobristTable[sq][pt] = dist(rng);
        }
    }
    ZobristBlackToMove = dist(rng);
}

inline int pieceToZobristIndex(const Piece& p) {
    if (p.PieceType == ChessPieceType::NONE) return -1;
    return (static_cast<int>(p.PieceType) - 1) + (p.PieceColor == ChessPieceColor::BLACK ? 6 : 0);
}

inline uint64_t ComputeZobrist(const Board& board) {
    uint64_t h = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int idx = pieceToZobristIndex(board.squares[sq].Piece);
        if (idx >= 0) h ^= ZobristTable[sq][idx];
    }
    if (board.turn == ChessPieceColor::BLACK) h ^= ZobristBlackToMove;
    return h;
}

struct TTEntry {
    int depth;
    int value;
    int flag; // 0 = exact, -1 = alpha, 1 = beta
};

extern std::unordered_map<uint64_t, TTEntry> TransTable;

int getMVVLVA_Score(const Board& board, int srcPos, int destPos) {
    const Piece& attacker = board.squares[srcPos].Piece;
    const Piece& victim = board.squares[destPos].Piece;
    
    if (victim.PieceType == ChessPieceType::NONE) return 0;
    
    return victim.PieceValue * 10 - attacker.PieceValue;
}

bool isCapture(const Board& board, int /* srcPos */, int destPos) {
    return board.squares[destPos].Piece.PieceType != ChessPieceType::NONE;
}

bool givesCheck(const Board& board, int srcPos, int destPos) {
    Board tempBoard = board;
    tempBoard.movePiece(srcPos, destPos);
    
    ChessPieceColor kingColor = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    return IsKingInCheck(tempBoard, kingColor);
}

bool isInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

int getHistoryScore(const std::vector<std::vector<int>>& historyTable, int srcPos, int destPos) {
    return historyTable[srcPos][destPos];
}

std::vector<ScoredMove> scoreMoves(const Board& board, const std::vector<std::pair<int, int>>& moves, 
                                  const std::vector<std::vector<int>>& historyTable) {
    std::vector<ScoredMove> scoredMoves;
    
    for (const auto& move : moves) {
        int srcPos = move.first;
        int destPos = move.second;
        int score = 0;
        
        if (isCapture(board, srcPos, destPos)) {
            score = 10000 + getMVVLVA_Score(board, srcPos, destPos);
        }
        else if (givesCheck(board, srcPos, destPos)) {
            score = 9000;
        }
        else if (board.squares[srcPos].Piece.PieceType == ChessPieceType::PAWN && 
                 (destPos < 8 || destPos >= 56)) {
            score = 8000;
        }
        else {
            score = getHistoryScore(historyTable, srcPos, destPos);
        }
        
        scoredMoves.emplace_back(move, score);
    }
    
    return scoredMoves;
}

void updateHistoryTable(std::vector<std::vector<int>>& historyTable, int srcPos, int destPos, int depth) {
    historyTable[srcPos][destPos] += depth * depth;
}

bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable,
                   const std::chrono::steady_clock::time_point& startTime, int timeLimitMs);
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);

extern std::map<std::string, std::string> OpeningBook;

inline std::string getBookMove(const std::string& fen) {
    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) return it->second;
    return "";
}

bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate) {
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    for (int depth = 1; depth <= 4; depth++) {
        int result = AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == ChessPieceColor::WHITE), historyTable, std::chrono::steady_clock::now(), std::numeric_limits<int>::max());
        
        if (result >= 9000) {
            if (movingSide == ChessPieceColor::WHITE) {
                BlackMate = true;
                return true;
            }
        } else if (result <= -9000) {
            if (movingSide == ChessPieceColor::BLACK) {
                WhiteMate = true;
                return true;
            }
        }
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, movingSide);
    if (moves.empty()) {
        StaleMate = true;
        return true;
    }
    
    return false;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, 
                   std::vector<std::vector<int>>& historyTable,
                   const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    if (isTimeUp(startTime, timeLimitMs)) {
        return 0;
    }
    
    nodeCount++;
    
    uint64_t hash = ComputeZobrist(board);
    auto ttIt = TransTable.find(hash);
    if (ttIt != TransTable.end()) {
        const TTEntry& entry = ttIt->second;
        if (entry.depth >= depth) {
            if (entry.flag == 0) return entry.value;
            if (entry.flag == -1 && entry.value <= alpha) return alpha;
            if (entry.flag == 1 && entry.value >= beta) return beta;
        }
    }
    
    if (depth == 0) {
        int eval = evaluatePosition(board);
        TransTable[hash] = {depth, eval, 0};
        return eval;
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    
    if (moves.empty()) {
        int mateScore = maximizingPlayer ? -10000 : 10000;
        TransTable[hash] = {depth, mateScore, 0};
        return mateScore;
    }
    
    if (depth >= 3 && !isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
        Board nullBoard = board;
        nullBoard.turn = (nullBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        int R = 3;
        int reducedDepth = depth - 1 - R;
        if (reducedDepth > 0) {
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer, historyTable, startTime, timeLimitMs);
            
            if (isTimeUp(startTime, timeLimitMs)) return 0;
            
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + 300) {
                        return beta;
                    }
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, historyTable, startTime, timeLimitMs);
                    if (isTimeUp(startTime, timeLimitMs)) return 0;
                    if (verificationValue >= beta) {
                        return beta;
                    }
                }
            } else {
                if (nullValue <= alpha) {
                    if (nullValue <= alpha - 300) {
                        return alpha;
                    }
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, historyTable, startTime, timeLimitMs);
                    if (isTimeUp(startTime, timeLimitMs)) return 0;
                    if (verificationValue <= alpha) {
                        return alpha;
                    }
                }
            }
        }
    }
    
    std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -10000 : 10000;
    int flag = 0;
    
    if (maximizingPlayer) {
        int moveCount = 0;
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove) {
                eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, false, historyTable, startTime, timeLimitMs);
                if (eval > alpha) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable, startTime, timeLimitMs);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable, startTime, timeLimitMs);
            }
            moveCount++;
            if (isTimeUp(startTime, timeLimitMs)) return 0;
            if (eval > bestValue) bestValue = eval;
            if (eval > alpha) alpha = eval;
            if (eval > origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
            if (beta <= alpha) break;
        }
        if (bestValue <= origAlpha) flag = -1;
        else if (bestValue >= beta) flag = 1;
        else flag = 0;
    } else {
        int moveCount = 0;
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove) {
                eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, true, historyTable, startTime, timeLimitMs);
                if (eval < beta) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable, startTime, timeLimitMs);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable, startTime, timeLimitMs);
            }
            moveCount++;
            if (isTimeUp(startTime, timeLimitMs)) return 0;
            if (eval < bestValue) bestValue = eval;
            if (eval < beta) beta = eval;
            if (eval < origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
            if (beta <= alpha) break;
        }
        if (bestValue >= beta) flag = 1;
        else if (bestValue <= origAlpha) flag = -1;
        else flag = 0;
    }
    TransTable[hash] = {depth, bestValue, flag};
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepening(Board& board, int maxDepth, int timeLimitMs) {
    SearchResult result;
    auto startTime = std::chrono::steady_clock::now();
    
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.empty()) {
        result.bestMove = {-1, -1};
        return result;
    }
    
    for (int depth = 1; depth <= maxDepth; depth++) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
        if (elapsed.count() > timeLimitMs * 0.8) {
            break;
        }
        
        int nodesAtStart = nodeCount;
        
        std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
        std::sort(scoredMoves.begin(), scoredMoves.end());
        
        int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
        std::pair<int, int> bestMoveThisDepth = moves[0];
        
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            
            int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, 
                                     (board.turn == ChessPieceColor::BLACK), historyTable, startTime, timeLimitMs);
            
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
        
        if (isTimeUp(startTime, timeLimitMs)) {
            break;
        }
        
        result.bestMove = bestMoveThisDepth;
        result.score = bestEval;
        result.depth = depth;
        result.nodes = nodeCount - nodesAtStart;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    return result;
}

std::pair<int, int> findBestMove(Board& board, int depth) {
    std::vector<std::vector<int>> historyTable(64, std::vector<int>(64, 0));
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.empty()) {
        return {-1, -1};
    }
    
    std::vector<ScoredMove> scoredMoves = scoreMoves(board, moves, historyTable);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = moves[0];
    
    for (const auto& scoredMove : scoredMoves) {
        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.movePiece(move.first, move.second);
        
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

#endif