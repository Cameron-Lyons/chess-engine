// Implementation of search functions
#include "search.h"
#include "engine_globals.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <limits>
#include <future>
#include <thread>
#include <iostream>

// ThreadSafeTT methods
void ThreadSafeTT::insert(uint64_t hash, const TTEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex);
    table[hash] = entry;
}

bool ThreadSafeTT::find(uint64_t hash, TTEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = table.find(hash);
    if (it != table.end()) {
        entry = it->second;
        return true;
    }
    return false;
}

void ThreadSafeTT::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    table.clear();
}

// ThreadSafeHistory methods
ThreadSafeHistory::ThreadSafeHistory() : table(64, std::vector<int>(64, 0)) {}
void ThreadSafeHistory::update(int srcPos, int destPos, int depth) {
    std::lock_guard<std::mutex> lock(mutex);
    table[srcPos][destPos] += depth * depth;
}
int ThreadSafeHistory::get(int srcPos, int destPos) const {
    std::lock_guard<std::mutex> lock(mutex);
    return table[srcPos][destPos];
}

// ParallelSearchContext constructor
ParallelSearchContext::ParallelSearchContext(int threads)
    : stopSearch(false), nodeCount(0), numThreads(threads) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
}

// ScoredMove methods
ScoredMove::ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
bool ScoredMove::operator<(const ScoredMove& other) const {
    return score > other.score;
}

// SearchResult constructor
SearchResult::SearchResult() : bestMove({-1, -1}), score(0), depth(0), nodes(0), timeMs(0) {}

// Zobrist hashing
void InitZobrist() {
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

int pieceToZobristIndex(const Piece& p) {
    if (p.PieceType == ChessPieceType::NONE) return -1;
    return (static_cast<int>(p.PieceType) - 1) + (p.PieceColor == ChessPieceColor::BLACK ? 6 : 0);
}

uint64_t ComputeZobrist(const Board& board) {
    uint64_t h = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int idx = pieceToZobristIndex(board.squares[sq].Piece);
        if (idx >= 0) h ^= ZobristTable[sq][idx];
    }
    if (board.turn == ChessPieceColor::BLACK) h ^= ZobristBlackToMove;
    return h;
}

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

int getHistoryScore(const ThreadSafeHistory& historyTable, int srcPos, int destPos) {
    return historyTable.get(srcPos, destPos);
}

std::vector<ScoredMove> scoreMovesParallel(const Board& board, const std::vector<std::pair<int, int>>& moves, const ThreadSafeHistory& historyTable, int numThreads) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());
    if (moves.size() < static_cast<size_t>(numThreads * 4)) {
        for (const auto& move : moves) {
            int srcPos = move.first;
            int destPos = move.second;
            int score = 0;
            if (isCapture(board, srcPos, destPos)) {
                score = 10000 + getMVVLVA_Score(board, srcPos, destPos);
            } else if (givesCheck(board, srcPos, destPos)) {
                score = 9000;
            } else if (board.squares[srcPos].Piece.PieceType == ChessPieceType::PAWN && (destPos < 8 || destPos >= 56)) {
                score = 8000;
            } else {
                score = getHistoryScore(historyTable, srcPos, destPos);
            }
            scoredMoves.emplace_back(move, score);
        }
    } else {
        std::vector<std::future<std::vector<ScoredMove>>> futures;
        int movesPerThread = moves.size() / numThreads;
        for (int i = 0; i < numThreads; ++i) {
            int start = i * movesPerThread;
            int end = (i == numThreads - 1) ? moves.size() : (i + 1) * movesPerThread;
            futures.push_back(std::async(std::launch::async, [&, start, end]() {
                std::vector<ScoredMove> localScoredMoves;
                for (int j = start; j < end; ++j) {
                    const auto& move = moves[j];
                    int srcPos = move.first;
                    int destPos = move.second;
                    int score = 0;
                    if (isCapture(board, srcPos, destPos)) {
                        score = 10000 + getMVVLVA_Score(board, srcPos, destPos);
                    } else if (givesCheck(board, srcPos, destPos)) {
                        score = 9000;
                    } else if (board.squares[srcPos].Piece.PieceType == ChessPieceType::PAWN && (destPos < 8 || destPos >= 56)) {
                        score = 8000;
                    } else {
                        score = getHistoryScore(historyTable, srcPos, destPos);
                    }
                    localScoredMoves.emplace_back(move, score);
                }
                return localScoredMoves;
            }));
        }
        for (auto& future : futures) {
            auto result = future.get();
            scoredMoves.insert(scoredMoves.end(), result.begin(), result.end());
        }
    }
    return scoredMoves;
}

void updateHistoryTable(ThreadSafeHistory& historyTable, int srcPos, int destPos, int depth) {
    historyTable.update(srcPos, destPos, depth);
}

bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

std::string getBookMove(const std::string& fen) {
    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) return it->second;
    return "";
}

bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    for (int depth = 1; depth <= 4; depth++) {
        int result = AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == ChessPieceColor::WHITE), historyTable, context);
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

std::vector<std::pair<int, int>> GetQuietMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> allMoves = generateBitboardMoves(board, color);
    std::vector<std::pair<int, int>> noisyMoves;
    
    for (const auto& move : allMoves) {
        int srcPos = move.first;
        int destPos = move.second;
        
        if (isCapture(board, srcPos, destPos)) {
            noisyMoves.push_back(move);
            continue;
        }
        
        if (givesCheck(board, srcPos, destPos)) {
            noisyMoves.push_back(move);
            continue;
        }
        
        const Piece& piece = board.squares[srcPos].Piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            int destRow = destPos / 8;
            if ((piece.PieceColor == ChessPieceColor::WHITE && destRow == 7) ||
                (piece.PieceColor == ChessPieceColor::BLACK && destRow == 0)) {
                noisyMoves.push_back(move);
                continue;
            }
        }
    }
    
    return noisyMoves;
}

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer, ThreadSafeHistory& historyTable, ParallelSearchContext& context) {
    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return 0;
    }
    
    context.nodeCount.fetch_add(1);
    
    int standPat = evaluatePosition(board);
    
    if (maximizingPlayer) {
        if (standPat >= beta) {
            return beta;
        }
        if (alpha < standPat) {
            alpha = standPat;
        }
    } else {
        if (standPat <= alpha) {
            return alpha;
        }
        if (beta > standPat) {
            beta = standPat;
        }
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> noisyMoves = GetQuietMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    
    if (noisyMoves.empty()) {
        return standPat;
    }
    
    std::vector<ScoredMove> scoredMoves = scoreMovesParallel(board, noisyMoves, historyTable, 1);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int bestValue = standPat;
    
    if (maximizingPlayer) {
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) return 0;
            
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            
            int eval = QuiescenceSearch(newBoard, alpha, beta, false, historyTable, context);
            
            if (context.stopSearch.load()) return 0;
            
            if (eval > bestValue) {
                bestValue = eval;
            }
            if (eval > alpha) {
                alpha = eval;
            }
            if (beta <= alpha) {
                break; // Beta cutoff
            }
        }
    } else {
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) return 0;
            
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            
            int eval = QuiescenceSearch(newBoard, alpha, beta, true, historyTable, context);
            
            if (context.stopSearch.load()) return 0;
            
            if (eval < bestValue) {
                bestValue = eval;
            }
            if (eval < beta) {
                beta = eval;
            }
            if (beta <= alpha) {
                break; // Alpha cutoff
            }
        }
    }
    
    return bestValue;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, ThreadSafeHistory& historyTable, ParallelSearchContext& context) {
    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return 0;
    }
    context.nodeCount.fetch_add(1);
    uint64_t hash = ComputeZobrist(board);
    TTEntry entry;
    if (context.transTable.find(hash, entry)) {
        if (entry.depth >= depth) {
            if (entry.flag == 0) return entry.value;
            if (entry.flag == -1 && entry.value <= alpha) return alpha;
            if (entry.flag == 1 && entry.value >= beta) return beta;
        }
    }
    if (depth == 0) {
        // Instead of directly evaluating, enter quiescence search
        int eval = QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context);
        context.transTable.insert(hash, {depth, eval, 0});
        return eval;
    }
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    if (moves.empty()) {
        int mateScore = maximizingPlayer ? -10000 : 10000;
        context.transTable.insert(hash, {depth, mateScore, 0});
        return mateScore;
    }
    if (depth >= 3 && !isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
        Board nullBoard = board;
        nullBoard.turn = (nullBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        int R = 3;
        int reducedDepth = depth - 1 - R;
        if (reducedDepth > 0) {
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer, historyTable, context);
            if (context.stopSearch.load()) return 0;
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + 300) {
                        return beta;
                    }
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, historyTable, context);
                    if (context.stopSearch.load()) return 0;
                    if (verificationValue >= beta) {
                        return beta;
                    }
                }
            } else {
                if (nullValue <= alpha) {
                    if (nullValue <= alpha - 300) {
                        return alpha;
                    }
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, historyTable, context);
                    if (context.stopSearch.load()) return 0;
                    if (verificationValue <= alpha) {
                        return alpha;
                    }
                }
            }
        }
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesParallel(board, moves, historyTable, context.numThreads);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -10000 : 10000;
    int flag = 0;
    if (maximizingPlayer) {
        int moveCount = 0;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load()) return 0;
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove) {
                eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, false, historyTable, context);
                if (eval > alpha) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable, context);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, historyTable, context);
            }
            moveCount++;
            if (context.stopSearch.load()) return 0;
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
            if (context.stopSearch.load()) return 0;
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove) {
                eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, true, historyTable, context);
                if (eval < beta) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable, context);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, historyTable, context);
            }
            moveCount++;
            if (context.stopSearch.load()) return 0;
            if (eval < bestValue) bestValue = eval;
            if (eval < beta) beta = eval;
            if (eval < origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
            if (beta <= alpha) break;
        }
        if (bestValue >= beta) flag = 1;
        else if (bestValue <= origAlpha) flag = -1;
        else flag = 0;
    }
    context.transTable.insert(hash, {depth, bestValue, flag});
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs, int numThreads) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    SearchResult result;
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    if (moves.empty()) {
        result.bestMove = {-1, -1};
        return result;
    }
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            result.bestMove = {srcRow * 8 + srcCol, destRow * 8 + destCol};
            result.score = 0;
            result.depth = 0;
            result.nodes = 0;
            result.timeMs = 0;
            return result;
        }
    }
    for (int depth = 1; depth <= maxDepth; depth++) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - context.startTime);
        if (elapsed.count() > timeLimitMs * 0.8) {
            break;
        }
        int nodesAtStart = context.nodeCount.load();
        std::vector<ScoredMove> scoredMoves = scoreMovesParallel(board, moves, context.historyTable, numThreads);
        std::sort(scoredMoves.begin(), scoredMoves.end());
        int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
        std::pair<int, int> bestMoveThisDepth = moves[0];
        std::vector<std::future<std::pair<std::pair<int, int>, int>>> futures;
        int movesPerThread = std::max(1, static_cast<int>(scoredMoves.size()) / numThreads);
        for (int i = 0; i < numThreads && static_cast<size_t>(i * movesPerThread) < scoredMoves.size(); ++i) {
            int start = i * movesPerThread;
            int end = (i == numThreads - 1) ? scoredMoves.size() : (i + 1) * movesPerThread;
            futures.push_back(std::async(std::launch::async, [&, start, end, depth]() {
                std::pair<int, int> localBestMove = {-1, -1};
                int localBestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
                for (int j = start; j < end; ++j) {
                    if (context.stopSearch.load()) break;
                    const auto& scoredMove = scoredMoves[j];
                    const auto& move = scoredMove.move;
                    Board newBoard = board;
                    newBoard.movePiece(move.first, move.second);
                    int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, (board.turn == ChessPieceColor::BLACK), context.historyTable, context);
                    if (context.stopSearch.load()) break;
                    if (board.turn == ChessPieceColor::WHITE) {
                        if (eval > localBestEval) {
                            localBestEval = eval;
                            localBestMove = move;
                        }
                    } else {
                        if (eval < localBestEval) {
                            localBestEval = eval;
                            localBestMove = move;
                        }
                    }
                }
                return std::make_pair(localBestMove, localBestEval);
            }));
        }
        for (auto& future : futures) {
            if (context.stopSearch.load()) break;
            auto [move, eval] = future.get();
            if (move.first != -1) {
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
        }
        if (context.stopSearch.load()) {
            break;
        }
        result.bestMove = bestMoveThisDepth;
        result.score = bestEval;
        result.depth = depth;
        result.nodes = context.nodeCount.load() - nodesAtStart;
    }
    auto endTime = std::chrono::steady_clock::now();
    result.timeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - context.startTime).count();
    return result;
}

std::pair<int, int> findBestMove(Board& board, int depth) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    if (moves.empty()) {
        return {-1, -1};
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesParallel(board, moves, historyTable, 1);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = moves[0];
    for (const auto& scoredMove : scoredMoves) {
        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.movePiece(move.first, move.second);
        int eval = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, (board.turn == ChessPieceColor::BLACK), historyTable, context);
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