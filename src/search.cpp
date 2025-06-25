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
        
        // Replace if new entry is deeper, or same depth but exact bound, or much older
        if (entry.depth >= existing.depth || 
            (entry.depth >= existing.depth - 2 && entry.flag == 0)) {
            table[hash] = entry;
        }
    } else {
        // Table size management - keep it reasonable
        if (table.size() > 100000) { // Limit table size
            // Simple cleanup - remove 10% of entries (should use LRU in production)
            auto removeIt = table.begin();
            for (int i = 0; i < table.size() / 10 && removeIt != table.end(); ++i) {
                removeIt = table.erase(removeIt);
            }
        }
        table[hash] = entry;
    }
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

ScoredMove::ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
bool ScoredMove::operator<(const ScoredMove& other) const {
    return score > other.score; // Higher scores first
}

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

// Utility function to get piece value for delta pruning
int getPieceValue(ChessPieceType pieceType) {
    switch (pieceType) {
        case ChessPieceType::PAWN: return 100;
        case ChessPieceType::KNIGHT: return 300;
        case ChessPieceType::BISHOP: return 300;
        case ChessPieceType::ROOK: return 500;
        case ChessPieceType::QUEEN: return 900;
        case ChessPieceType::KING: return 10000;
        case ChessPieceType::NONE: return 0;
        default: return 0;
    }
}

std::vector<ScoredMove> scoreMovesOptimized(const Board& board, const std::vector<std::pair<int, int>>& moves, const ThreadSafeHistory& historyTable, const KillerMoves& killerMoves, int ply) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());
    
    for (const auto& move : moves) {
        int srcPos = move.first;
        int destPos = move.second;
        int score = 0;
        
        const Piece& movingPiece = board.squares[srcPos].Piece;
        const Piece& targetPiece = board.squares[destPos].Piece;
        
        // 1. Hash move bonus (if we had one from transposition table)
        // TODO: Add hash move from TT
        
        // 2. Captures - MVV-LVA with bonuses
        if (isCapture(board, srcPos, destPos)) {
            score = 10000 + getMVVLVA_Score(board, srcPos, destPos);
            
            // Bonus for capturing with less valuable piece
            if (targetPiece.PieceValue > movingPiece.PieceValue) {
                score += 500; // Good trade
            }
            
            // Bonus for capturing undefended pieces
            // TODO: Add SEE (Static Exchange Evaluation) here
            score += 100;
        }
        
        // 3. Checks
        else if (givesCheck(board, srcPos, destPos)) {
            score = 9000;
            
            // Bonus for discovered checks
            if (movingPiece.PieceType != ChessPieceType::QUEEN && 
                movingPiece.PieceType != ChessPieceType::ROOK && 
                movingPiece.PieceType != ChessPieceType::BISHOP) {
                score += 200; // Likely discovered check
            }
        }
        
        // 4. Promotions
        else if (movingPiece.PieceType == ChessPieceType::PAWN && 
                 (destPos < 8 || destPos >= 56)) {
            score = 8000;
        }
        
        // 5. Castling
        else if (movingPiece.PieceType == ChessPieceType::KING && 
                 abs(destPos - srcPos) == 2) {
            score = 7500; // Encourage castling
        }
        
        // 6. Killer moves
        else if (killerMoves.isKiller(ply, move)) {
            score = killerMoves.getKillerScore(ply, move);
        }
        
        // 7. Quiet moves with positional bonuses
        else {
            score = getHistoryScore(historyTable, srcPos, destPos);
            
            // Piece-specific bonuses
            switch (movingPiece.PieceType) {
                case ChessPieceType::PAWN: {
                    int destRank = destPos / 8;
                    int srcRank = srcPos / 8;
                    
                    // Pawn advance bonus
                    if (movingPiece.PieceColor == ChessPieceColor::WHITE) {
                        score += (destRank - srcRank) * 50; // Forward progress
                        if (destRank >= 5) score += 100; // Advanced pawns
                    } else {
                        score += (srcRank - destRank) * 50;
                        if (destRank <= 2) score += 100; // Advanced pawns
                    }
                    break;
                }
                
                case ChessPieceType::KNIGHT: {
                    // Knight centralization bonus
                    int destFile = destPos % 8;
                    int destRank = destPos / 8;
                    if (destFile >= 2 && destFile <= 5 && destRank >= 2 && destRank <= 5) {
                        score += 150; // Central squares
                    }
                    if (destFile >= 3 && destFile <= 4 && destRank >= 3 && destRank <= 4) {
                        score += 100; // Super center
                    }
                    break;
                }
                
                case ChessPieceType::BISHOP: {
                    // Bishop on long diagonal bonus
                    if ((destPos % 9 == 0) || (destPos % 7 == 0 && destPos != 0 && destPos != 56)) {
                        score += 80;
                    }
                    break;
                }
                
                case ChessPieceType::ROOK: {
                    int destFile = destPos % 8;
                    int destRank = destPos / 8;
                    
                    // Rook on open/semi-open file bonus
                    bool hasOwnPawn = false;
                    bool hasEnemyPawn = false;
                    for (int r = 0; r < 8; r++) {
                        int pos = r * 8 + destFile;
                        if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                            if (board.squares[pos].Piece.PieceColor == movingPiece.PieceColor) {
                                hasOwnPawn = true;
                            } else {
                                hasEnemyPawn = true;
                            }
                        }
                    }
                    if (!hasOwnPawn && !hasEnemyPawn) score += 200; // Open file
                    else if (!hasOwnPawn) score += 100; // Semi-open file
                    
                    // 7th/2nd rank bonus
                    if ((movingPiece.PieceColor == ChessPieceColor::WHITE && destRank == 6) ||
                        (movingPiece.PieceColor == ChessPieceColor::BLACK && destRank == 1)) {
                        score += 150;
                    }
                    break;
                }
                
                case ChessPieceType::QUEEN: {
                    // Queen development penalty in opening
                    // Simple heuristic: if many pieces haven't moved, penalize early queen moves
                    int piecesOnBackRank = 0;
                    int backRank = (movingPiece.PieceColor == ChessPieceColor::WHITE) ? 0 : 7;
                    for (int f = 0; f < 8; f++) {
                        int pos = backRank * 8 + f;
                        if (board.squares[pos].Piece.PieceType != ChessPieceType::NONE && 
                            board.squares[pos].Piece.PieceColor == movingPiece.PieceColor &&
                            board.squares[pos].Piece.PieceType != ChessPieceType::PAWN) {
                            piecesOnBackRank++;
                        }
                    }
                    if (piecesOnBackRank > 4) { // Early in game
                        score -= 200; // Penalize early queen development
                    }
                    break;
                }
                
                case ChessPieceType::KING: {
                    // King safety - prefer moves towards safety in opening/middlegame
                    int totalMaterial = 0;
                    for (int i = 0; i < 64; i++) {
                        if (board.squares[i].Piece.PieceType != ChessPieceType::NONE) {
                            totalMaterial += board.squares[i].Piece.PieceValue;
                        }
                    }
                    
                    if (totalMaterial > 3000) { // Not endgame
                        int destFile = destPos % 8;
                        int destRank = destPos / 8;
                        
                        // Prefer king moves towards corners/safety
                        if (movingPiece.PieceColor == ChessPieceColor::WHITE) {
                            if (destRank == 0 && (destFile <= 2 || destFile >= 5)) {
                                score += 100; // Towards corners for castling
                            }
                        } else {
                            if (destRank == 7 && (destFile <= 2 || destFile >= 5)) {
                                score += 100;
                            }
                        }
                    }
                    break;
                }
                
                default:
                    break;
            }
            
            // General center control bonus
            int destFile = destPos % 8;
            int destRank = destPos / 8;
            if (destFile >= 2 && destFile <= 5 && destRank >= 2 && destRank <= 5) {
                score += 30; // Central squares
            }
            if (destFile >= 3 && destFile <= 4 && destRank >= 3 && destRank <= 4) {
                score += 50; // Super center
            }
        }
        
        scoredMoves.emplace_back(move, score);
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
    if (context.stopSearch.load()) return 0;
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
    const int DELTA_MARGIN = 900; // Queen value - if we can't improve by this much, prune
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
    
    // Score and sort captures by MVV-LVA
    std::vector<ScoredMove> scoredMoves;
    for (const auto& move : tacticalMoves) {
        int score = 0;
        if (isCapture(board, move.first, move.second)) {
            int victimValue = getPieceValue(board.squares[move.second].Piece.PieceType);
            int attackerValue = getPieceValue(board.squares[move.first].Piece.PieceType);
            score = victimValue * 100 - attackerValue; // MVV-LVA
            
            // Delta pruning for individual captures
            if (maximizingPlayer && standPat + victimValue + 200 < alpha) {
                continue; // Skip futile captures
            }
            if (!maximizingPlayer && standPat - victimValue - 200 > beta) {
                continue; // Skip futile captures
            }
        }
        if (givesCheck(board, move.first, move.second)) {
            score += 100; // Checks are important
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
        
        int eval = QuiescenceSearch(newBoard, alpha, beta, !maximizingPlayer, historyTable, context);
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

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply, ThreadSafeHistory& historyTable, ParallelSearchContext& context) {
    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return 0;
    }
    context.nodeCount.fetch_add(1);
    
    // Check extensions - extend search for important positions
    int extension = 0;
    ChessPieceColor currentColor = maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    
    if (depth > 0 && isInCheck(board, currentColor)) {
        extension = 1; // Always extend when in check
    }
    
    depth += extension;
    
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
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
            if (context.stopSearch.load()) return 0;
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + 300) {
                        return beta;
                    }
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
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
                    int verificationValue = AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer, ply + 1, historyTable, context);
                    if (context.stopSearch.load()) return 0;
                    if (verificationValue <= alpha) {
                        return alpha;
                    }
                }
            }
        }
    }
    std::vector<ScoredMove> scoredMoves = scoreMovesOptimized(board, moves, historyTable, context.killerMoves, ply);
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
            
            // Improved Late Move Reduction with more conditions
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor)) {
                
                // Reduce more for later moves
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                // Don't reduce too much in PV nodes or important positions
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                eval = AlphaBetaSearch(newBoard, depth - 1 - reduction, alpha, beta, false, ply + 1, historyTable, context);
                
                // If the reduced search suggests this move is good, search full depth
                if (eval > alpha) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
            }
            moveCount++;
            if (context.stopSearch.load()) return 0;
            if (eval > bestValue) bestValue = eval;
            if (eval > alpha) alpha = eval;
            if (eval > origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
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
            
            // Improved Late Move Reduction for black
            if (depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor)) {
                
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                eval = AlphaBetaSearch(newBoard, depth - 1 - reduction, alpha, beta, true, ply + 1, historyTable, context);
                if (eval < beta) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
                }
            } else {
                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
            }
            moveCount++;
            if (context.stopSearch.load()) return 0;
            if (eval < bestValue) bestValue = eval;
            if (eval < beta) beta = eval;
            if (eval < origAlpha) updateHistoryTable(historyTable, move.first, move.second, depth);
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
    context.transTable.insert(hash, {depth, bestValue, flag});
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs, int numThreads) {
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
    }
    
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    if (moves.empty()) {
        SearchResult emptyResult;
        return emptyResult;
    }
    
    std::pair<int, int> bestMove = {-1, -1};
    int bestScore = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    ChessPieceColor currentTurn = board.turn; // Store the turn for lambda
    
    for (int depth = 1; depth <= maxDepth && !context.stopSearch.load() && !isTimeUp(context.startTime, context.timeLimitMs); depth++) {
        if (context.stopSearch.load()) break;
        
        std::vector<std::future<std::pair<std::pair<int, int>, int>>> futures;
        
        int movesPerThread = std::max(1, static_cast<int>(moves.size()) / numThreads);
        for (int t = 0; t < numThreads && !context.stopSearch.load(); t++) {
            int start = t * movesPerThread;
            int end = (t == numThreads - 1) ? moves.size() : (t + 1) * movesPerThread;
            if (start >= static_cast<int>(moves.size())) break;
            
            futures.push_back(std::async(std::launch::async, [board, moves, start, end, depth, currentTurn, &context]() -> std::pair<std::pair<int, int>, int> {
                std::pair<int, int> threadBestMove = {-1, -1};
                int threadBestScore = (currentTurn == ChessPieceColor::WHITE) ? -10000 : 10000;
                
                for (int i = start; i < end && !context.stopSearch.load(); i++) {
                    const auto& move = moves[i];
                    
                    // Create a copy of the board for this thread to avoid race conditions
                    Board localBoard = board;
                    localBoard.movePiece(move.first, move.second);
                    
                    int eval = AlphaBetaSearch(localBoard, depth - 1, -10000, 10000, 
                                             (currentTurn == ChessPieceColor::BLACK), 0, 
                                             context.historyTable, context);
                    
                    if (context.stopSearch.load()) break;
                    
                    if (currentTurn == ChessPieceColor::WHITE) {
                        if (eval > threadBestScore) {
                            threadBestScore = eval;
                            threadBestMove = move;
                        }
                    } else {
                        if (eval < threadBestScore) {
                            threadBestScore = eval;
                            threadBestMove = move;
                        }
                    }
                }
                return {threadBestMove, threadBestScore};
            }));
        }
        
        for (auto& future : futures) {
            if (context.stopSearch.load()) break;
            auto result = future.get();
            if (result.first.first != -1) { // Valid move found
                if (currentTurn == ChessPieceColor::WHITE) {
                    if (result.second > bestScore) {
                        bestScore = result.second;
                        bestMove = result.first;
                    }
                } else {
                    if (result.second < bestScore) {
                        bestScore = result.second;
                        bestMove = result.first;
                    }
                }
            }
        }
        
        if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) break;
        
        if (abs(bestScore) >= 9000) break;
    }
    
    SearchResult result;
    result.bestMove = bestMove;
    result.score = bestScore;
    result.depth = maxDepth;
    result.nodes = context.nodeCount.load();
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
        
        // Optional: Print search info
        if (currentDepth >= 6) {
            std::cout << "Depth " << currentDepth << ": score=" << bestEval 
                      << ", move=" << (bestMove.first % 8) << "," << (bestMove.first / 8) 
                      << " to " << (bestMove.second % 8) << "," << (bestMove.second / 8) << "\n";
        }
    }
    
    return bestMove;
} 