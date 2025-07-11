// Implementation of search functions
#include "search.h"
#include "engine_globals.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <future>
#include <thread>



void ThreadSafeTT::insert(uint64_t hash, const TTEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex);
    
    // Better replacement strategy - always replace if new entry is deeper or exact
    auto it = table.find(hash);
    if (it != table.end()) {
        const TTEntry& existing = it->second;
        
        // Check for hash collision
        if (existing.zobristKey != 0 && existing.zobristKey != entry.zobristKey) {
            // Hash collision detected - only replace if significantly better
            if (entry.depth >= existing.depth + 2) {
                TTEntry newEntry = entry;
                newEntry.zobristKey = entry.zobristKey;
                table[hash] = newEntry;
            }
            return;
        }
        
        // Replace if new entry is deeper, or same depth but exact bound, or much older
        if (entry.depth >= existing.depth || 
            (entry.depth >= existing.depth - 2 && entry.flag == 0)) {
            TTEntry newEntry = entry;
            newEntry.zobristKey = entry.zobristKey;
            table[hash] = newEntry;
        }
    } else {
        // Table size management - keep it reasonable
        if (table.size() > 100000) { // Limit table size
            // Simple cleanup - remove 10% of entries (should use LRU in production)
            auto removeIt = table.begin();
            for (size_t i = 0; i < table.size() / 10 && removeIt != table.end(); ++i) {
                removeIt = table.erase(removeIt);
            }
        }
        TTEntry newEntry = entry;
        newEntry.zobristKey = entry.zobristKey;
        table[hash] = newEntry;
    }
}

bool ThreadSafeTT::find(uint64_t hash, TTEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = table.find(hash);
    if (it != table.end()) {
        entry = it->second;
        // Verify zobrist key to avoid hash collisions
        if (entry.zobristKey != 0 && entry.zobristKey != hash) {
            return false; // Hash collision
        }
        return true;
    }
    return false;
}

void ThreadSafeTT::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    table.clear();
}

ThreadSafeHistory::ThreadSafeHistory() : table(64, std::vector<int>(64, 0)) {}
void ThreadSafeHistory::update(int srcPos, int destPos, int depth) {
    std::lock_guard<std::mutex> lock(mutex);
    table[srcPos][destPos] += depth * depth;
}
int ThreadSafeHistory::get(int srcPos, int destPos) const {
    std::lock_guard<std::mutex> lock(mutex);
    return table[srcPos][destPos];
}

KillerMoves::KillerMoves() {
    for (int ply = 0; ply < MAX_PLY; ply++) {
        for (int slot = 0; slot < MAX_KILLER_MOVES; slot++) {
            killers[ply][slot] = {-1, -1}; // Invalid move
        }
    }
}

void KillerMoves::store(int ply, std::pair<int, int> move) {
    if (ply < 0 || ply >= MAX_PLY) return;
    
    if (isKiller(ply, move)) return;
    
    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--) {
        killers[ply][i] = killers[ply][i - 1];
    }
    killers[ply][0] = move;
}

bool KillerMoves::isKiller(int ply, std::pair<int, int> move) const {
    if (ply < 0 || ply >= MAX_PLY) return false;
    
    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return true;
        }
    }
    return false;
}

int KillerMoves::getKillerScore(int ply, std::pair<int, int> move) const {
    if (ply < 0 || ply >= MAX_PLY) return 0;
    
    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return 5000 - i * 100; // First killer gets higher score
        }
    }
    return 0;
}

ParallelSearchContext::ParallelSearchContext(int threads)
    : stopSearch(false), nodeCount(0), numThreads(threads), ply(0) {
    if (numThreads == 0) numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
}

// ScoredMove constructor and operators are now inline in the header

SearchResult::SearchResult() : bestMove({-1, -1}), score(0), depth(0), nodes(0), timeMs(0) {}

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
        int idx = pieceToZobristIndex(board.squares[sq].piece);
        if (idx >= 0) h ^= ZobristTable[sq][idx];
    }
    if (board.turn == ChessPieceColor::BLACK) h ^= ZobristBlackToMove;
    return h;
}

int getMVVLVA_Score(const Board& board, int srcPos, int destPos) {
    const Piece& attacker = board.squares[srcPos].piece;
    const Piece& victim = board.squares[destPos].piece;
    if (victim.PieceType == ChessPieceType::NONE) return 0;
    return victim.PieceValue * 10 - attacker.PieceValue;
}

bool isCapture(const Board& board, int /* srcPos */, int destPos) {
    return board.squares[destPos].piece.PieceType != ChessPieceType::NONE;
}

bool givesCheck(const Board& board, int srcPos, int destPos) {
    Board tempBoard = board;
    tempBoard.movePiece(srcPos, destPos);
    tempBoard.updateBitboards(); // Ensure bitboards are properly updated
    ChessPieceColor kingColor = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    return IsKingInCheck(tempBoard, kingColor);
}

bool isInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

int getHistoryScore(const ThreadSafeHistory& historyTable, int srcPos, int destPos) {
    return historyTable.get(srcPos, destPos);
}

// Utility function to get piece value for delta pruning
int getPieceValue(ChessPieceType pieceType) {
    switch (pieceType) {
        case ChessPieceType::PAWN: return 100;
        case ChessPieceType::KNIGHT: return 300;
        case ChessPieceType::BISHOP: return 300;
        case ChessPieceType::ROOK: return 500;
        case ChessPieceType::QUEEN: return 975;
        case ChessPieceType::KING: return 10000;
        case ChessPieceType::NONE: return 0;
        default: return 0;
    }
}

std::vector<ScoredMove> scoreMovesOptimized(const Board& board, const std::vector<std::pair<int, int>>& moves, 
                                          const ThreadSafeHistory& historyTable, const KillerMoves& killerMoves, 
                                          int ply, const std::pair<int, int>& ttMove) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());
    
    for (const auto& move : moves) {
        int score = 0;
        
        // 1. Transposition table move (highest priority)
        if (move == ttMove) {
            score = EnhancedMoveOrdering::HASH_MOVE_SCORE;
        }
        // 2. Captures with MVV-LVA and SEE
        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            score = EnhancedMoveOrdering::CAPTURE_SCORE_BASE + mvvLva * 1000 + see;
        }
        // 3. Killer moves
        else if (killerMoves.isKiller(ply, move)) {
            score = EnhancedMoveOrdering::KILLER_SCORE + 
                   EnhancedMoveOrdering::getKillerScore(killerMoves, ply, move.first, move.second);
        }
        // 4. History heuristics for quiet moves
        else {
            score = EnhancedMoveOrdering::HISTORY_SCORE_BASE + 
                   EnhancedMoveOrdering::getHistoryScore(historyTable, move.first, move.second);
            
            // Add positional bonus for quiet moves
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
        }
        
        scoredMoves.push_back({move, score});
    }
    
    return scoredMoves;
}

void updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth) {
    historyTable.updateScore(fromSquare, toSquare, depth * depth);
}

bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    if (timeLimitMs <= 0) return false;
    
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

std::string getBookMove(const std::string& fen) {
    // First check the new multi-option opening book
    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, options.size() - 1);
            return options[dis(gen)];
        }
    }
    
    // Fallback to legacy opening book
    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) return it->second;
    return "";
}

bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate, bool& StaleMate) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    for (int depth = 1; depth <= 4; depth++) {
        int result = AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == ChessPieceColor::WHITE), 0, historyTable, context);
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
        
        const Piece& piece = board.squares[srcPos].piece;
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

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer, ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply) {
    if (context.stopSearch.load()) return 0;
    
    // Safety check to prevent infinite recursion - be more conservative
    if (ply < 0 || ply >= 50) {
        return evaluatePosition(board);
    }
    
    context.nodeCount.fetch_add(1);
    
    // Stand pat evaluation
    int standPat = evaluatePosition(board);
    
    if (maximizingPlayer) {
        if (standPat >= beta) return beta;
        alpha = std::max(alpha, standPat);
    } else {
        if (standPat <= alpha) return alpha;
        beta = std::min(beta, standPat);
    }
    
    // Delta pruning - don't search moves that can't improve position significantly
            const int DELTA_MARGIN = 975; // Queen value - if we can't improve by this much, prune
    const int BIG_DELTA_MARGIN = 1200; // For very hopeless positions
    
    if (maximizingPlayer) {
        if (standPat + DELTA_MARGIN < alpha) {
            // Position is so bad that even winning a Queen won't help
            return alpha;
        }
        if (standPat + BIG_DELTA_MARGIN < alpha) {
            // Even more hopeless - return immediately
            return standPat;
        }
    } else {
        if (standPat - DELTA_MARGIN > beta) {
            return beta;
        }
        if (standPat - BIG_DELTA_MARGIN > beta) {
            return standPat;
        }
    }
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    // Filter to only captures and checks
    std::vector<std::pair<int, int>> tacticalMoves;
    for (const auto& move : moves) {
        if (isCapture(board, move.first, move.second) || givesCheck(board, move.first, move.second)) {
            tacticalMoves.push_back(move);
        }
    }
    
    if (tacticalMoves.empty()) {
        return standPat;
    }
    
    // Enhanced capture filtering with improved SEE
    std::vector<ScoredMove> scoredMoves;
    for (const auto& move : tacticalMoves) {
        int score = 0;
        bool isCapt = isCapture(board, move.first, move.second);
        
        if (isCapt) {
            int victimValue = getPieceValue(board.squares[move.second].piece.PieceType);
            int attackerValue = getPieceValue(board.squares[move.first].piece.PieceType);
            
            // Basic MVV-LVA scoring
            score = victimValue * 100 - attackerValue;
            
            // ENHANCED SEE FILTERING - Much more aggressive
            int seeValue = staticExchangeEvaluation(board, move.first, move.second);
            
            // Skip bad captures more aggressively in quiescence
            if (seeValue < -25) {
                continue; // Tighter threshold for bad captures
            }
            
            // Delta pruning with SEE consideration
            int expectedGain = std::max(seeValue, victimValue);
            if (maximizingPlayer && standPat + expectedGain + 150 < alpha) {
                continue; // Skip futile captures
            }
            if (!maximizingPlayer && standPat - expectedGain - 150 > beta) {
                continue; // Skip futile captures
            }
            
            // SEE-based scoring enhancement
            score += seeValue;
            
            // Bonus for definitely winning captures
            if (seeValue > victimValue) {
                score += 50; // Bonus for winning more than expected
            }
            
            // Priority for equal/winning captures
            if (seeValue >= 0) {
                score += 200; // Higher priority for non-losing captures
            }
        }
        
        // Check bonus (but not too high to avoid check spam)
        if (givesCheck(board, move.first, move.second)) {
            score += 80; // Reduced from 100 to prioritize good captures
        }
        
        scoredMoves.push_back({move, score});
    }
    
    // Sort by score (best first)
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int bestValue = standPat;
    
    for (const auto& scoredMove : scoredMoves) {
        if (context.stopSearch.load()) return 0;
        
        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.movePiece(move.first, move.second);
        
        int eval = QuiescenceSearch(newBoard, alpha, beta, !maximizingPlayer, historyTable, context, ply + 1);
        if (context.stopSearch.load()) return 0;
        
        if (maximizingPlayer) {
            bestValue = std::max(bestValue, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestValue = std::min(bestValue, eval);
            beta = std::min(beta, eval);
        }
        
        if (beta <= alpha) {
            break; // Beta cutoff
        }
    }
    
    return bestValue;
}

int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply, ThreadSafeHistory& historyTable, ParallelSearchContext& context, bool isPVNode) {
    // Safety checks to prevent segmentation fault - be more conservative
    if (depth < 0 || depth > 20) {
        return 0;
    }
    
    if (ply < 0 || ply >= 50) {
        return 0;
    }
    
    if (isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch = true;
        return 0;
    }
    
    if (depth == 0) {
        return QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
    }
    
    context.nodeCount++;
    
    // Check transposition table
    uint64_t zobristKey = ComputeZobrist(board);
    TTEntry ttEntry;
    if (context.transTable.find(zobristKey, ttEntry) && ttEntry.depth >= depth) {
        if (ttEntry.flag == 0) { // Exact score
            return ttEntry.value;
        } else if (ttEntry.flag == -1 && ttEntry.value <= alpha) { // Lower bound
            return alpha;
        } else if (ttEntry.flag == 1 && ttEntry.value >= beta) { // Upper bound
            return beta;
        }
    }
    
    // Null move pruning
    if (depth >= 3 && !isPVNode && !isInCheck(board, board.turn)) {
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        int nullScore = -PrincipalVariationSearch(board, depth - 3, -beta, -beta + 1, !maximizingPlayer, ply + 1, historyTable, context, false);
        board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        if (nullScore >= beta) {
            return beta;
        }
    }
    
    // Futility pruning
    if (depth <= 3 && !isPVNode && !isInCheck(board, board.turn)) {
        int staticEval = evaluatePosition(board);
        if (staticEval - 300 * depth >= beta) {
            return beta;
        }
    }
    
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    if (moves.empty()) {
        if (isInCheck(board, board.turn)) {
            return maximizingPlayer ? -10000 + ply : 10000 - ply; // Checkmate
        } else {
            return 0; // Stalemate
        }
    }
    
    // Move ordering
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(board, moves, historyTable, context.killerMoves, ply, ttEntry.bestMove);
    std::sort(scoredMoves.begin(), scoredMoves.end(), std::greater<ScoredMove>());
    
    int bestValue = maximizingPlayer ? -10000 : 10000;
    std::pair<int, int> bestMove = {-1, -1};
    int flag = -1; // Lower bound
    
    for (size_t i = 0; i < scoredMoves.size(); ++i) {
        const auto& move = scoredMoves[i].move;
        
        // Make move
        Board tempBoard = board;
        tempBoard.movePiece(move.first, move.second);
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        int score;
        if (i == 0 || isPVNode) {
            // Full window search for first move or PV nodes
            score = -PrincipalVariationSearch(tempBoard, depth - 1, -beta, -alpha, !maximizingPlayer, ply + 1, historyTable, context, isPVNode);
        } else {
            // Null window search for non-PV moves
            score = -PrincipalVariationSearch(tempBoard, depth - 1, -alpha - 1, -alpha, !maximizingPlayer, ply + 1, historyTable, context, false);
            if (score > alpha && score < beta) {
                // Re-search with full window
                score = -PrincipalVariationSearch(tempBoard, depth - 1, -beta, -alpha, !maximizingPlayer, ply + 1, historyTable, context, true);
            }
        }
        
        if (maximizingPlayer) {
            if (score > bestValue) {
                bestValue = score;
                bestMove = move;
                if (score > alpha) {
                    alpha = score;
                    flag = 0; // Exact score
                }
            }
        } else {
            if (score < bestValue) {
                bestValue = score;
                bestMove = move;
                if (score < beta) {
                    beta = score;
                    flag = 0; // Exact score
                }
            }
        }
        
        if (alpha >= beta) {
            // Beta cutoff
            flag = 1; // Upper bound
            context.killerMoves.store(ply, move);
            updateHistoryTable(historyTable, move.first, move.second, depth);
            break;
        }
    }
    
    // Store in transposition table
    context.transTable.insert(zobristKey, TTEntry(depth, bestValue, flag, bestMove, zobristKey));
    
    return bestValue;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply, ThreadSafeHistory& historyTable, ParallelSearchContext& context) {
    
    // Safety checks to prevent segmentation fault - be more conservative
    if (depth < 0 || depth > 20) {
        return 0;
    }
    
    if (ply < 0 || ply >= 50) {
        return 0;
    }
    
    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return 0;
    }
    context.nodeCount.fetch_add(1);
    
    // Check extensions - extend search for important positions
    int extension = 0;
    ChessPieceColor currentColor = maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    
    // Only extend if we're not at the maximum depth and ply limits
    // Be much more conservative with extensions to prevent infinite recursion
    if (depth > 1 && depth <= 3 && ply < 50) {
        // Extend when in check (most important)
        if (isInCheck(board, currentColor)) {
            extension = 1;
        }
        // Only extend for very obvious tactical situations at shallow depths
        else if (depth == 2 || depth == 3) {
            // Only extend for queen captures or very obvious hanging pieces
            bool hasObviousHangingPiece = false;
            for (int i = 0; i < 64 && !hasObviousHangingPiece; i++) {
                const Piece& piece = board.squares[i].piece;
                if (piece.PieceType == ChessPieceType::NONE) continue;
                if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) continue;
                
                // Only check if piece is under attack by enemy pieces
                ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                            ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                
                // Only extend for queen captures or pieces under multiple attacks
                int attackCount = 0;
                for (int j = 0; j < 64; j++) {
                    const Piece& enemyPiece = board.squares[j].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE || 
                        enemyPiece.PieceColor != enemyColor) continue;
                        
                    if (canPieceAttackSquare(board, j, i)) {
                        attackCount++;
                        // Only extend for queen captures or pieces under multiple attacks
                        if (piece.PieceType == ChessPieceType::QUEEN || attackCount >= 2) {
                            hasObviousHangingPiece = true;
                            break;
                        }
                    }
                }
            }
            
            if (hasObviousHangingPiece) {
                extension = 1;
            }
        }
    }
    
    depth += extension;
    
    // Additional safety check after extension - be more conservative
    if (depth > 20 || ply >= 50) {
        return 0;
    }
    
    uint64_t hash = ComputeZobrist(board);
    TTEntry entry;
    std::pair<int, int> hashMove = {-1, -1}; // Store hash move for move ordering
    
    if (context.transTable.find(hash, entry)) {
        if (entry.depth >= depth) {
            if (entry.flag == 0) return entry.value;
            if (entry.flag == -1 && entry.value <= alpha) return alpha;
            if (entry.flag == 1 && entry.value >= beta) return beta;
        }
        // Extract hash move for move ordering even if we don't use the score
        if (entry.bestMove.first != -1 && entry.bestMove.second != -1) {
            hashMove = entry.bestMove;
        }
    }
    if (depth == 0) {
        int eval = QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
        context.transTable.insert(hash, TTEntry(depth, eval, 0, {-1, -1}, hash));
        return eval;
    }
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    if (moves.empty()) {
        int mateScore = maximizingPlayer ? -10000 : 10000;
        context.transTable.insert(hash, TTEntry(depth, mateScore, 0, {-1, -1}, hash));
        return mateScore;
    }
    // Null move pruning - only at reasonable depths and ply, be more conservative
    if (depth >= 3 && depth <= 10 && ply < 30 && !isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
        Board nullBoard = board;
        nullBoard.turn = (nullBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        int R = 3;
        int reducedDepth = depth - 1 - R;
        if (reducedDepth > 0 && reducedDepth <= 10) {
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
            if (context.stopSearch.load()) return 0;
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + 300) {
                        return beta;
                    }
                    // Only verify if we're not too deep
                    if (depth - 1 <= 10 && ply + 1 < 30) {
                        int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
                        if (context.stopSearch.load()) return 0;
                        if (verificationValue >= beta) {
                            return beta;
                        }
                    }
                }
            } else {
                if (nullValue <= alpha) {
                    if (nullValue <= alpha - 300) {
                        return alpha;
                    }
                    // Only verify if we're not too deep
                    if (depth - 1 <= 10 && ply + 1 < 30) {
                        int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
                        if (context.stopSearch.load()) return 0;
                        if (verificationValue <= alpha) {
                            return alpha;
                        }
                    }
                }
            }
        }
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(board, moves, historyTable, context.killerMoves, ply, hashMove);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -10000 : 10000;
    int flag = 0;
    bool foundPV = false; // Principal Variation found flag
    std::pair<int, int> bestMoveFound = {-1, -1}; // Track best move for hash table
    
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
            
            // Principal Variation Search (PVS)
            if (moveCount == 0 || foundPV) {
                // First move or we're in PV - search with full window
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
            } else {
                // Non-PV moves - try null window search first
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, alpha + 1, false, ply + 1, historyTable, context);
                
                // If null window search suggests this move is good, re-search with full window
                if (eval > alpha && eval < beta) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
                }
            }
            
            // SEE-based futility pruning for captures at shallow depths
            if (depth <= 3 && isCaptureMove && !isCheckMove && !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                
                // Skip obviously bad captures at shallow depths
                if (seeValue < -100 && depth <= 2) {
                    moveCount++;
                    continue; // Don't search losing captures at shallow depth
                }
                
                // Reduce search depth for bad captures
                if (seeValue < -50 && depth == 3) {
                    eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, false, ply + 1, historyTable, context);
                    moveCount++;
                    if (context.stopSearch.load()) return 0;
                    if (eval > bestValue) {
                        bestValue = eval;
                        bestMoveFound = move;
                    }
                    if (eval > alpha) {
                        alpha = eval;
                        foundPV = true;
                        updateHistoryTable(historyTable, move.first, move.second, depth);
                    }
                    if (beta <= alpha) {
                        if (!isCaptureMove) {
                            context.killerMoves.store(ply, move);
                        }
                        break;
                    }
                    continue;
                }
            }
            
            // Late Move Reduction - only for non-PV moves and reasonable depths, be more conservative
            if (!foundPV && depth >= 3 && depth <= 10 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor) && ply < 30) {
                
                // Reduce more for later moves
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                // Don't reduce too much in PV nodes or important positions
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                // Ensure we don't reduce below 0
                int reducedDepth = depth - 1 - reduction;
                if (reducedDepth >= 0 && reducedDepth <= 10) {
                    // Try reduced search first
                    int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, alpha, alpha + 1, false, ply + 1, historyTable, context);
                    
                    // If reduced search fails high, do full search
                    if (reducedEval > alpha) {
                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
                    } else {
                        eval = reducedEval;
                    }
                } else {
                    // Fall back to normal search if reduction would be too aggressive
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
                }
            }
            

            
            moveCount++;
            if (context.stopSearch.load()) return 0;
            if (eval > bestValue) {
                bestValue = eval;
                bestMoveFound = move; // Track best move
            }
            if (eval > alpha) {
                alpha = eval;
                foundPV = true; // We found a move that improved alpha
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {
                // Beta cutoff - store killer move if it's not a capture
                if (!isCaptureMove) {
                    context.killerMoves.store(ply, move);
                }
                break;
            }
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
            
            // Principal Variation Search (PVS) for black
            if (moveCount == 0 || foundPV) {
                // First move or we're in PV - search with full window
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
            } else {
                // Non-PV moves - try null window search first
                eval = AlphaBetaSearch(newBoard, depth - 1, beta - 1, beta, true, ply + 1, historyTable, context);
                
                // If null window search suggests this move is good, re-search with full window
                if (eval < beta && eval > alpha) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
                }
            }
            
            // SEE-based futility pruning for captures at shallow depths (minimizing player)
            if (depth <= 3 && isCaptureMove && !isCheckMove && !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);
                
                // Skip obviously bad captures at shallow depths
                if (seeValue < -100 && depth <= 2) {
                    moveCount++;
                    continue; // Don't search losing captures at shallow depth
                }
                
                // Reduce search depth for bad captures
                if (seeValue < -50 && depth == 3) {
                    eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, true, ply + 1, historyTable, context);
                    moveCount++;
                    if (context.stopSearch.load()) return 0;
                    if (eval < bestValue) {
                        bestValue = eval;
                        bestMoveFound = move;
                    }
                    if (eval < beta) {
                        beta = eval;
                        foundPV = true;
                        updateHistoryTable(historyTable, move.first, move.second, depth);
                    }
                    if (beta <= alpha) {
                        if (!isCaptureMove) {
                            context.killerMoves.store(ply, move);
                        }
                        break;
                    }
                    continue;
                }
            }
            
            // Late Move Reduction for black - only for non-PV moves and reasonable depths, be more conservative
            if (!foundPV && depth >= 3 && depth <= 10 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor) && ply < 30) {
                
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                // Ensure we don't reduce below 0
                int reducedDepth = depth - 1 - reduction;
                if (reducedDepth >= 0 && reducedDepth <= 10) {
                    // Try reduced search first
                    int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, beta - 1, beta, true, ply + 1, historyTable, context);
                    
                    // If reduced search fails low, do full search
                    if (reducedEval < beta) {
                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
                    } else {
                        eval = reducedEval;
                    }
                } else {
                    // Fall back to normal search if reduction would be too aggressive
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
                }
            }
            
            moveCount++;
            if (context.stopSearch.load()) return 0;
            if (eval < bestValue) {
                bestValue = eval;
                bestMoveFound = move; // Track best move
            }
            if (eval < beta) {
                beta = eval;
                foundPV = true; // We found a move that improved beta
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {
                // Beta cutoff - store killer move if it's not a capture
                if (!isCaptureMove) {
                    context.killerMoves.store(ply, move);
                }
                break;
            }
        }
        if (bestValue >= beta) flag = 1;
        else if (bestValue <= origAlpha) flag = -1;
        else flag = 0;
    }
    // Store result in transposition table with best move
    context.transTable.insert(hash, TTEntry(depth, bestValue, flag, bestMoveFound, hash));
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs, int numThreads) {
    SearchResult result;
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    
    int lastScore = 0;
    const int ASPIRATION_WINDOW = 50; // Initial aspiration window size
    
    // Check opening book first
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            result.bestMove = {srcRow * 8 + srcCol, destRow * 8 + destCol};
            result.score = 0;
            result.depth = 1;
            result.nodes = 1;
            result.timeMs = 0;
            return result;
        }
    }
    
    // Iterative deepening with aspiration windows - increased max depth for tactics
    for (int depth = 1; depth <= std::max(maxDepth, 10); depth++) {
        int alpha, beta;
        
        if (depth <= 3) {
            // Full window for shallow depths
            alpha = -10000;
            beta = 10000;
        } else {
            // Aspiration windows for deeper searches
            alpha = lastScore - ASPIRATION_WINDOW;
            beta = lastScore + ASPIRATION_WINDOW;
        }
        
        int searchScore;
        bool failedHigh = false, failedLow = false;
        int searchAttempts = 0;
        const int MAX_SEARCH_ATTEMPTS = 4;
        
        do {
            searchScore = AlphaBetaSearch(board, depth, alpha, beta, board.turn == ChessPieceColor::WHITE, 0, context.historyTable, context);
            searchAttempts++;
            
            if (context.stopSearch.load()) {
                break;
            }
            
            if (searchScore <= alpha) {
                // Failed low - widen alpha
                alpha = alpha - ASPIRATION_WINDOW * (1 << searchAttempts);
                failedLow = true;
                if (alpha < -10000) alpha = -10000;
            } else if (searchScore >= beta) {
                // Failed high - widen beta
                beta = beta + ASPIRATION_WINDOW * (1 << searchAttempts);
                failedHigh = true;
                if (beta > 10000) beta = 10000;
            } else {
                // Search completed successfully within window
                break;
            }
            
            if (searchAttempts >= MAX_SEARCH_ATTEMPTS) {
                // Give up on aspiration windows, use full window
                alpha = -10000;
                beta = 10000;
            }
            
        } while ((failedHigh || failedLow) && searchAttempts < MAX_SEARCH_ATTEMPTS && !context.stopSearch.load());
        
        if (context.stopSearch.load()) {
            break;
        }
        
        // Update result with best search from this depth
        lastScore = searchScore;
        result.score = searchScore;
        result.depth = depth;
        result.nodes = context.nodeCount.load();
        
        // Get best move from this iteration
        GenValidMoves(board);
        std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
        if (!moves.empty()) {
            std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(board, moves, context.historyTable, context.killerMoves, 0);
            std::sort(scoredMoves.begin(), scoredMoves.end());
            result.bestMove = scoredMoves[0].move;
        }
        
        // Time management - if we're running low on time, don't start the next depth
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - context.startTime).count();
        
        // Smart time management - spend more time on tactical positions
        double timeRatio = (double)elapsed / timeLimitMs;
        bool timeToStop = false;
        
        // Check if position is tactical (has hanging pieces or checks)
        bool isTactical = false;
        if (isInCheck(board, board.turn)) {
            isTactical = true;
        } else {
            // Quick check for hanging pieces
            for (int i = 0; i < 64 && !isTactical; i++) {
                const Piece& piece = board.squares[i].piece;
                if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::PAWN) {
                    ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                                ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                    for (int j = 0; j < 64; j++) {
                        const Piece& enemyPiece = board.squares[j].piece;
                        if (enemyPiece.PieceColor == enemyColor && canPieceAttackSquare(board, j, i)) {
                            isTactical = true;
                            break;
                        }
                    }
                }
            }
        }
        
        // Allow more time for tactical positions
        double threshold1 = isTactical ? 0.85 : 0.8;
        double threshold2 = isTactical ? 0.95 : 0.9;
        
        if (depth >= 8 && timeRatio > threshold1) {
            timeToStop = true;
        } else if (depth >= 6 && timeRatio > threshold2) {
            timeToStop = true;
        } else if (timeRatio > 0.98) {
            timeToStop = true; // Hard time limit
        }
        
        if (timeToStop) {
            break;
        }
        
        // Debug output removed for UCI compatibility
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
    
    // Use aspiration windows for better search efficiency
    int previousScore = 0;
    int aspirationWindow = 50;
    int maxAspirationWindow = 400;
    
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(board, moves, historyTable, context.killerMoves, 0);
    std::sort(scoredMoves.begin(), scoredMoves.end());
    
    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = {-1, -1};
    
    // Iterative deepening with aspiration windows
    for (int currentDepth = 1; currentDepth <= depth; currentDepth++) {
        
        int alpha, beta;
        
        if (currentDepth == 1) {
            // Full window for first iteration
            alpha = -10000;
            beta = 10000;
        } else {
            // Aspiration window around previous score
            alpha = previousScore - aspirationWindow;
            beta = previousScore + aspirationWindow;
        }
        
        bool searchFailed = false;
        int currentBestEval = bestEval;
        std::pair<int, int> currentBestMove = bestMove;
        
        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            
            int eval = AlphaBetaSearch(newBoard, currentDepth - 1, alpha, beta, 
                                     (board.turn == ChessPieceColor::BLACK), 0, 
                                     historyTable, context);
            
            if (board.turn == ChessPieceColor::WHITE) {
                if (eval > currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;
                    
                    if (eval > alpha) {
                        alpha = eval;
                    }
                    
                    // Check for aspiration window failure
                    if (eval >= beta) {
                        searchFailed = true;
                        break; // Search failed high
                    }
                }
            } else {
                if (eval < currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;
                    
                    if (eval < beta) {
                        beta = eval;
                    }
                    
                    // Check for aspiration window failure
                    if (eval <= alpha) {
                        searchFailed = true;
                        break; // Search failed low
                    }
                }
            }
        }
        
        // Handle aspiration window failures
        if (searchFailed && currentDepth > 1) {
            // Widen the window and research
            aspirationWindow = std::min(aspirationWindow * 2, maxAspirationWindow);
            
            if (board.turn == ChessPieceColor::WHITE && currentBestEval >= beta) {
                // Failed high - research with higher beta
                beta = 10000;
            } else if (board.turn == ChessPieceColor::BLACK && currentBestEval <= alpha) {
                // Failed low - research with lower alpha
                alpha = -10000;
            } else {
                // Failed both ways - use full window
                alpha = -10000;
                beta = 10000;
            }
            
            // Redo this depth with wider window
            currentDepth--;
            continue;
        }
        
        // Update best move and score
        bestEval = currentBestEval;
        bestMove = currentBestMove;
        previousScore = bestEval;
        
        // Reset aspiration window for next iteration
        aspirationWindow = std::max(50, aspirationWindow / 2);
        
        // Debug output removed for UCI compatibility
    }
    
    return bestMove;
}

// ===================================================================
// OPTIMIZED STATIC EXCHANGE EVALUATION (SEE)
// ===================================================================
// This is a much cleaner, more efficient implementation that uses
// a direct simulation approach with proper piece value tracking.

int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare) {
    if (fromSquare < 0 || fromSquare >= 64 || toSquare < 0 || toSquare >= 64) {
        return 0;
    }
    
    const Piece& victim = board.squares[toSquare].piece;
    const Piece& attacker = board.squares[fromSquare].piece;
    
    if (victim.PieceType == ChessPieceType::NONE || attacker.PieceType == ChessPieceType::NONE) {
        return 0;
    }
    
    // Initialize the exchange
    int score = victim.PieceValue;
    ChessPieceColor sideToMove = attacker.PieceColor;
    
    // Create a temporary board for the exchange
    Board tempBoard = board;
    tempBoard.squares[toSquare].piece = attacker;
    tempBoard.squares[fromSquare].piece = Piece(); // Remove attacker
    tempBoard.updateBitboards(); // Properly update bitboards
    
    // Find the smallest attacker to the square
    ChessPieceColor currentSide = sideToMove;
    
    while (true) {
        // Find the smallest attacker
        int smallestAttacker = getSmallestAttacker(tempBoard, toSquare, currentSide);
        if (smallestAttacker == -1) {
            break; // No more attackers
        }
        
        const Piece& currentAttacker = tempBoard.squares[smallestAttacker].piece;
        
        // Make the capture
        tempBoard.squares[toSquare].piece = currentAttacker;
        tempBoard.squares[smallestAttacker].piece = Piece();
        tempBoard.updateBitboards(); // Properly update bitboards after each move
        
        // Update score
        if (currentSide == sideToMove) {
            score = currentAttacker.PieceValue - score;
        } else {
            score = currentAttacker.PieceValue - score;
        }
        
        // Switch sides
        currentSide = (currentSide == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    }
    
    return score;
}

int getSmallestAttacker(const Board& board, int targetSquare, ChessPieceColor color) {
    int smallestValue = 10000;
    int smallestAttacker = -1;
    
    // Check all pieces of the given color
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }
        
        // Check if this piece can attack the target square
        if (canPieceAttackSquare(board, i, targetSquare)) {
            if (piece.PieceValue < smallestValue) {
                smallestValue = piece.PieceValue;
                smallestAttacker = i;
            }
        }
    }
    
    return smallestAttacker;
}

bool isGoodCapture(const Board& board, int fromSquare, int toSquare) {
    int see = staticExchangeEvaluation(board, fromSquare, toSquare);
    return see >= 0; // Capture is good if SEE >= 0
}

bool isCaptureProfitable(const Board& board, int fromSquare, int toSquare, int threshold) {
    int see = staticExchangeEvaluation(board, fromSquare, toSquare);
    return see >= threshold;
}

// ===================================================================
// SEE HELPER FUNCTIONS
// ===================================================================

// Forward declarations and helper functions
bool isDiscoveredCheck(const Board& board, int from, int to);
bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
int staticExchangeEval(const Board& board, int fromSquare, int toSquare);

// Helper function implementations
bool isDiscoveredCheck(const Board& board, int from, int /* to */) {
    // Simplified check - assume any non-direct checking piece might be a discovered check
    ChessPieceType movingPiece = board.squares[from].piece.PieceType;
    return (movingPiece != ChessPieceType::QUEEN && 
            movingPiece != ChessPieceType::ROOK && 
            movingPiece != ChessPieceType::BISHOP);
}

bool isPromotion(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    ChessPieceColor color = board.squares[from].piece.PieceColor;
    
    if (piece != ChessPieceType::PAWN) return false;
    
    int destRow = to / 8;
    return (color == ChessPieceColor::WHITE && destRow == 7) ||
           (color == ChessPieceColor::BLACK && destRow == 0);
}

bool isCastling(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    if (piece != ChessPieceType::KING) return false;
    
    return std::abs(to - from) == 2;
}

// MVV-LVA (Most Valuable Victim - Least Valuable Attacker) scoring table
// Rows: attacker (pawn=0, knight=1, bishop=2, rook=3, queen=4, king=5)
// Cols: victim (pawn=0, knight=1, bishop=2, rook=3, queen=4, king=5)
const int EnhancedMoveOrdering::MVV_LVA_SCORES[6][6] = {
    {0,  0,  0,  0,  0,  0},   // Pawn captures
    {50, 0,  0,  0,  0,  0},   // Knight captures
    {50, 0,  0,  0,  0,  0},   // Bishop captures
    {52, 2,  2,  0,  0,  0},   // Rook captures
    {54, 4,  4,  2,  0,  0},   // Queen captures
    {53, 3,  3,  1,  1,  0}    // King captures
};

int EnhancedMoveOrdering::getMVVLVA_Score(const Board& board, int fromSquare, int toSquare) {
    const Piece& attacker = board.squares[fromSquare].piece;
    const Piece& victim = board.squares[toSquare].piece;
    
    if (victim.PieceType == ChessPieceType::NONE) return 0;
    
    int attackerIndex = static_cast<int>(attacker.PieceType) - 1;
    int victimIndex = static_cast<int>(victim.PieceType) - 1;
    
    if (attackerIndex < 0 || attackerIndex >= 6 || victimIndex < 0 || victimIndex >= 6) {
        return 0;
    }
    
    return MVV_LVA_SCORES[attackerIndex][victimIndex];
}

int EnhancedMoveOrdering::getHistoryScore(const ThreadSafeHistory& history, int fromSquare, int toSquare) {
    return history.get(fromSquare, toSquare);
}

int EnhancedMoveOrdering::getKillerScore(const KillerMoves& killers, int ply, int fromSquare, int toSquare) {
    std::pair<int, int> move = {fromSquare, toSquare};
    if (killers.isKiller(ply, move)) {
        return killers.getKillerScore(ply, move);
    }
    return 0;
}

int EnhancedMoveOrdering::getPositionalScore(const Board& board, int fromSquare, int toSquare) {
    const Piece& piece = board.squares[fromSquare].piece;
    if (piece.PieceType == ChessPieceType::NONE) return 0;
    
    // Simple positional bonus based on piece-square tables
    int toSquareAdjusted = (piece.PieceColor == ChessPieceColor::WHITE) ? toSquare : 63 - toSquare;
    return getPieceSquareValue(piece.PieceType, toSquareAdjusted, piece.PieceColor) / 10;
} 