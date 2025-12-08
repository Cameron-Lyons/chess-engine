#ifndef SEARCH_H
#define SEARCH_H

#include "../core/ChessBoard.h"
#include "../evaluation/Evaluation.h"
#include "ValidMoves.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

std::string getFEN(const Board& board);
bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol,
                        int& destRow);

struct TTEntry {
    int depth;
    int value;
    int flag;
    std::pair<int, int> bestMove;
    uint64_t zobristKey;

    TTEntry() : depth(-1), value(0), flag(0), bestMove({-1, -1}), zobristKey(0) {}
    TTEntry(int d, int v, int f, std::pair<int, int> move = {-1, -1}, uint64_t key = 0)
        : depth(d), value(v), flag(f), bestMove(move), zobristKey(key) {}
};

struct ThreadSafeTT {
    std::unordered_map<uint64_t, TTEntry> table;
    mutable std::mutex mutex;
    void insert(uint64_t hash, const TTEntry& entry);
    bool find(uint64_t hash, TTEntry& entry) const;
    void clear();
};

struct ThreadSafeHistory {
    std::vector<std::vector<int>> table;
    mutable std::mutex mutex;
    ThreadSafeHistory();
    void update(int srcPos, int destPos, int depth);
    int get(int srcPos, int destPos) const;
    int getScore(int srcPos, int destPos) const {
        return get(srcPos, destPos);
    }
    void updateScore(int srcPos, int destPos, int score) {
        update(srcPos, destPos, score);
    }
};

struct KillerMoves {
    static const int MAX_KILLER_MOVES = 2;
    static const int MAX_PLY = 64;

    std::pair<int, int> killers[MAX_PLY][MAX_KILLER_MOVES];

    KillerMoves();
    void store(int ply, std::pair<int, int> move);
    bool isKiller(int ply, std::pair<int, int> move) const;
    int getKillerScore(int ply, std::pair<int, int> move) const;
};

struct ParallelSearchContext {
    std::atomic<bool> stopSearch;
    std::atomic<int> nodeCount;
    ThreadSafeTT transTable;
    ThreadSafeHistory historyTable;
    KillerMoves killerMoves;
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs;
    int numThreads;
    int ply;
    ParallelSearchContext(int threads = 0);
};

struct ScoredMove {
    std::pair<int, int> move;
    int score;
    ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
    bool operator<(const ScoredMove& other) const {
        return score < other.score;
    }
    bool operator>(const ScoredMove& other) const {
        return score > other.score;
    }
};

struct SearchResult {
    std::pair<int, int> bestMove;
    int score;
    int depth;
    int nodes;
    int timeMs;
    SearchResult();
};

extern uint64_t ZobristTable[64][12];
extern uint64_t ZobristBlackToMove;
extern ThreadSafeTT TransTable;

void InitZobrist();
int pieceToZobristIndex(const Piece& p);
uint64_t ComputeZobrist(const Board& board);

int getMVVLVA_Score(const Board& board, int srcPos, int destPos);
bool isCapture(const Board& board, int, int destPos);
bool givesCheck(const Board& board, int srcPos, int destPos);
bool isInCheck(const Board& board, ChessPieceColor color);
std::vector<ScoredMove> scoreMovesParallel(const Board& board,
                                           const std::vector<std::pair<int, int>>& moves,
                                           const ThreadSafeHistory& historyTable, int numThreads);
void updateHistoryTable(ThreadSafeHistory& historyTable, int srcPos, int destPos, int depth);
bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs);
bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate,
                   bool& StaleMate);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply,
                    ThreadSafeHistory& historyTable, ParallelSearchContext& context);
int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply);
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);
std::vector<std::pair<int, int>> GetQuietMoves(Board& board, ChessPieceColor color);
std::vector<ScoredMove> scoreMovesWithKillers(const Board& board,
                                              const std::vector<std::pair<int, int>>& moves,
                                              const ThreadSafeHistory& historyTable,
                                              const KillerMoves& killerMoves, int ply,
                                              int numThreads);
std::string getBookMove(const std::string& fen);
SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs,
                                        int numThreads = 0);
std::pair<int, int> findBestMove(Board& board, int depth);
int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer,
                             int ply, ThreadSafeHistory& historyTable,
                             ParallelSearchContext& context, bool isPVNode = true);

SearchResult lazySMPSearch(Board& board, int maxDepth, int timeLimitMs, int numThreads = 0);

int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare);
bool isGoodCapture(const Board& board, int fromSquare, int toSquare);
bool isCaptureProfitable(const Board& board, int fromSquare, int toSquare, int threshold = 0);
int getSmallestAttacker(const Board& board, int targetSquare, ChessPieceColor color);

bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos);
bool isDiscoveredCheck(const Board& board, int from, int to);
bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
std::vector<ScoredMove> scoreMovesOptimized(const Board& board,
                                            const std::vector<std::pair<int, int>>& moves,
                                            const ThreadSafeHistory& historyTable,
                                            const KillerMoves& killerMoves, int ply,
                                            const std::pair<int, int>& hashMove = {-1, -1});

struct SearchTechniques {
    static const int FUTILITY_MARGIN_PAWN = 100;
    static const int FUTILITY_MARGIN_KNIGHT = 300;
    static const int FUTILITY_MARGIN_BISHOP = 300;
    static const int FUTILITY_MARGIN_ROOK = 500;
    static const int FUTILITY_MARGIN_QUEEN = 900;

    static const int NULL_MOVE_R = 3;
    static const int NULL_MOVE_MARGIN = 300;

    static const int LMR_MIN_DEPTH = 3;
    static const int LMR_MIN_MOVE = 4;

    static const int STATIC_NULL_MARGIN = 900;

    static const int IID_MIN_DEPTH = 4;
};

namespace EnhancedMoveOrdering {
static const int HASH_MOVE_SCORE = 1000000;
static const int CAPTURE_SCORE_BASE = 900000;
static const int KILLER_SCORE = 800000;
static const int HISTORY_SCORE_BASE = 0;
static const int QUIET_SCORE_BASE = -1000000;

extern const int MVV_LVA_SCORES[6][6];

int getMVVLVA_Score(const Board& board, int fromSquare, int toSquare);
int getHistoryScore(const ThreadSafeHistory& history, int fromSquare, int toSquare);
int getKillerScore(const KillerMoves& killers, int ply, int fromSquare, int toSquare);
int getPositionalScore(const Board& board, int fromSquare, int toSquare);
} // namespace EnhancedMoveOrdering

#endif