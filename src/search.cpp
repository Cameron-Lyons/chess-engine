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
        case ChessPieceType::QUEEN: return 975;
        case ChessPieceType::KING: return 10000;
        case ChessPieceType::NONE: return 0;
        default: return 0;
    }
}

std::vector<ScoredMove> scoreMovesOptimized(const Board& board, const std::vector<std::pair<int, int>>& moves, const ThreadSafeHistory& historyTable, const KillerMoves& killerMoves, int ply, const std::pair<int, int>& hashMove) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());
    
    for (const auto& move : moves) {
        int score = 0;
        int srcPos = move.first;
        int destPos = move.second;
        
        ChessPieceType movingPiece = board.squares[srcPos].Piece.PieceType;
        ChessPieceType targetPiece = board.squares[destPos].Piece.PieceType;
        ChessPieceColor movingColor = board.squares[srcPos].Piece.PieceColor;
        
        // **PRIORITY 1: Hash moves (best move from transposition table)**
        if (hashMove.first != -1 && hashMove.second != -1 && 
            move.first == hashMove.first && move.second == hashMove.second) {
            score = 15000; // Highest priority
        }
        
        // **PRIORITY 2: Captures with Enhanced MVV-LVA + SEE evaluation**
        else if (targetPiece != ChessPieceType::NONE) {
            int victimValue = getPieceValue(targetPiece);
            int attackerValue = getPieceValue(movingPiece);
            
            // Enhanced MVV-LVA scoring with bigger value differences
            int mvvLvaScore = (victimValue * 1000) - attackerValue;
            
            // Use Static Exchange Evaluation for tactical accuracy
            int seeValue = staticExchangeEvaluation(board, srcPos, destPos);
            
            // Base capture score starts high
            score = 10000 + mvvLvaScore;
            
            // Strong SEE integration
            if (seeValue > 0) {
                score += std::min(seeValue, 500); // Cap SEE bonus to avoid overflow
            } else if (seeValue < 0) {
                // Heavy penalty for bad captures, but still try them if desperate
                score += std::max(seeValue, -400);
            }
            
            // Additional tactical bonuses
            if (victimValue >= 900) { // Queen captures
                score += 500;
            } else if (victimValue >= 500) { // Rook captures  
                score += 300;
            } else if (victimValue >= 300) { // Bishop/Knight captures
                score += 200;
            }
            
            // Promote equal captures (QxQ, RxR, etc.)
            if (std::abs(victimValue - attackerValue) <= 50) {
                score += 150;
            }
            
            // Bonus for capturing defended pieces (often tactical)
            bool targetIsDefended = false;
            ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            for (int i = 0; i < 64; i++) {
                const Piece& defenderPiece = board.squares[i].Piece;
                if (defenderPiece.PieceColor == enemyColor && 
                    canPieceAttackSquare(board, i, destPos)) {
                    targetIsDefended = true;
                    break;
                }
            }
            
            if (targetIsDefended && seeValue >= 0) {
                score += 100; // Bonus for winning defended piece exchanges
            }
        }
        
        // **PRIORITY 3: Checks with discovered check bonuses**
        else if (givesCheck(board, srcPos, destPos)) {
            score = 9000;
            
            // Bonus for discovered checks (more dangerous)
            if (isDiscoveredCheck(board, srcPos, destPos)) {
                score += 200;
            }
        }
        
        // **PRIORITY 4: Promotions**
        else if (isPromotion(board, srcPos, destPos)) {
            score = 8000;
            // Assume queen promotion for simplicity
            score += getPieceValue(ChessPieceType::QUEEN);
        }
        
        // **PRIORITY 5: Castling moves**
        else if (isCastling(board, srcPos, destPos)) {
            score = 7000;
        }
        
        // **PRIORITY 6: Enhanced Killer moves with multiple slots**
        else if (killerMoves.isKiller(ply, move)) {
            score = 6000;
            
            // Give higher bonus for primary killer (first killer move stored)
            int killerScore = killerMoves.getKillerScore(ply, move);
            score += killerScore; // This adds extra bonus for primary killer
            
            // Bonus for killer moves that are also historically good
            int historyScore = getHistoryScore(historyTable, srcPos, destPos);
            if (historyScore > 30) {
                score += 200; // Extra bonus for killer + history combination
            }
        }
        
        // **PRIORITY 7: Quiet moves with detailed positional scoring**
        else {
            score = 1000; // Base quiet move score
            
            // CRITICAL: Huge bonus for moving pieces that are under attack!
            // Check if the moving piece is under attack by the opponent
            ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            // Check if the piece being moved is currently under attack
            bool pieceUnderAttack = false;
            for (int i = 0; i < 64; i++) {
                const Piece& enemyPiece = board.squares[i].Piece;
                if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor) continue;
                
                if (canPieceAttackSquare(board, i, srcPos)) {
                    pieceUnderAttack = true;
                    break;
                }
            }
            
            if (pieceUnderAttack) {
                // HUGE priority for moving attacked pieces, especially the queen
                score += 5000;
                
                if (movingPiece == ChessPieceType::QUEEN) {
                    score += 2000; // Extra bonus for moving attacked queen
                }
                
                // Bonus if moving to a safe square (not attacked)
                bool destSquareSafe = true;
                for (int i = 0; i < 64; i++) {
                    const Piece& enemyPiece = board.squares[i].Piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor) continue;
                    
                    if (canPieceAttackSquare(board, i, destPos)) {
                        destSquareSafe = false;
                        break;
                    }
                }
                
                if (destSquareSafe) {
                    score += 1000; // Extra bonus for moving to safety
                }
            }
            
            // Enhanced History table bonus with scaling
            int historyScore = getHistoryScore(historyTable, srcPos, destPos);
            score += historyScore * 2; // Double the history influence
            
            // Counter-move heuristic: if this move responds well to opponent's last move
            // (This is a simplified version - in full engines this would track last moves)
            if (historyScore > 50) {
                score += 100; // Bonus for historically good moves
            }
            
            // Bonus for moves that improve piece activity
            int srcMobility = 0, destMobility = 0;
            
            // Count how many squares the piece can reach from src vs dest
            // (Simplified mobility calculation)
            for (int testSquare = 0; testSquare < 64; testSquare++) {
                if (canPieceAttackSquare(board, srcPos, testSquare)) {
                    srcMobility++;
                }
            }
            
            // Simulate piece on destination and count mobility
            Board testBoard = board;
            testBoard.squares[destPos].Piece = testBoard.squares[srcPos].Piece;
            testBoard.squares[srcPos].Piece.PieceType = ChessPieceType::NONE;
            
            for (int testSquare = 0; testSquare < 64; testSquare++) {
                if (canPieceAttackSquare(testBoard, destPos, testSquare)) {
                    destMobility++;
                }
            }
            
            // Bonus for moves that increase piece mobility
            if (destMobility > srcMobility) {
                score += (destMobility - srcMobility) * 5;
            }
            
            // CRITICAL: Advanced piece-specific move ordering
            int destRow = destPos / 8;
            int destCol = destPos % 8;
            
            // Special handling for queen moves (prevent blunders)
            if (movingPiece == ChessPieceType::QUEEN) {
                // Massive penalty for queen moves to dangerous squares
                bool isCorner = ((destRow == 0 || destRow == 7) && (destCol == 0 || destCol == 7));
                bool isEdge = (destRow == 0 || destRow == 7 || destCol == 0 || destCol == 7);
                
                if (isCorner) {
                    score -= 3000; // Huge penalty for corner
                } else if (isEdge) {
                    score -= 1500; // Large penalty for edge
                }
                
                // Check if queen would be in enemy territory without support
                bool inEnemyTerritory = false;
                if (movingColor == ChessPieceColor::WHITE && destRow >= 5) {
                    inEnemyTerritory = true;
                } else if (movingColor == ChessPieceColor::BLACK && destRow <= 2) {
                    inEnemyTerritory = true;
                }
                
                if (inEnemyTerritory) {
                    // Count supporting pieces
                    int supportCount = 0;
                    for (int i = 0; i < 64; i++) {
                        const Piece& friendlyPiece = board.squares[i].Piece;
                        if (friendlyPiece.PieceType == ChessPieceType::NONE || 
                            friendlyPiece.PieceColor != movingColor || 
                            i == srcPos || 
                            friendlyPiece.PieceType == ChessPieceType::PAWN) continue;
                        
                        if (canPieceAttackSquare(board, i, destPos)) {
                            supportCount++;
                        }
                    }
                    
                    if (supportCount == 0) {
                        score -= 2000; // Heavy penalty for unsupported queen in enemy territory
                    } else if (supportCount == 1) {
                        score -= 800; // Moderate penalty for barely supported queen
                    }
                }
                
                // Penalize early queen development (encourage piece development first)
                if ((movingColor == ChessPieceColor::WHITE && destRow > 3) ||
                    (movingColor == ChessPieceColor::BLACK && destRow < 4)) {
                    // Count developed pieces (not on starting squares)
                    int developedPieces = 0;
                    for (int i = 0; i < 64; i++) {
                        const Piece& piece = board.squares[i].Piece;
                        if (piece.PieceColor == movingColor && 
                            (piece.PieceType == ChessPieceType::KNIGHT || 
                             piece.PieceType == ChessPieceType::BISHOP)) {
                            int row = i / 8;
                            int startRow = (movingColor == ChessPieceColor::WHITE) ? 0 : 7;
                            if (row != startRow) {
                                developedPieces++;
                            }
                        }
                    }
                    
                    if (developedPieces < 2) {
                        score -= 400; // Penalty for early queen moves before piece development
                    }
                }
            }
            
            // Knight-specific bonuses
            else if (movingPiece == ChessPieceType::KNIGHT) {
                // Centralization bonus
                int centerDistance = std::abs(destRow - 3.5) + std::abs(destCol - 3.5);
                score += (7 - centerDistance) * 15;
                
                // Avoid rim
                if (destRow == 0 || destRow == 7 || destCol == 0 || destCol == 7) {
                    score -= 100;
                }
                
                // Bonus for knight forks and attacks on multiple pieces
                int attackedPieces = 0;
                ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? 
                                            ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                for (int i = 0; i < 64; i++) {
                    const Piece& enemyPiece = board.squares[i].Piece;
                    if (enemyPiece.PieceColor == enemyColor && 
                        canPieceAttackSquare(testBoard, destPos, i)) {
                        attackedPieces++;
                        if (enemyPiece.PieceType == ChessPieceType::QUEEN || 
                            enemyPiece.PieceType == ChessPieceType::ROOK) {
                            score += 150; // Bonus for attacking valuable pieces
                        }
                    }
                }
                
                if (attackedPieces >= 2) {
                    score += 300; // Bonus for potential forks
                }
            }
        }
        
        scoredMoves.push_back({move, score});
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
    
    // NEW: Extend search for critical tactical positions
    if (depth > 0 && depth <= 6) { // Only extend in middle game depth range
        // Extend if there are hanging pieces (potential tactics)
        bool hasHangingPieces = false;
        for (int i = 0; i < 64; i++) {
            const Piece& piece = board.squares[i].Piece;
            if (piece.PieceType == ChessPieceType::NONE) continue;
            if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) continue;
            
            // Quick check if piece is under attack
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            bool isAttacked = false;
            for (int j = 0; j < 64; j++) {
                const Piece& enemyPiece = board.squares[j].Piece;
                if (enemyPiece.PieceType == ChessPieceType::NONE || 
                    enemyPiece.PieceColor != enemyColor) continue;
                    
                if (canPieceAttackSquare(board, j, i)) {
                    isAttacked = true;
                    break;
                }
            }
            
            if (isAttacked) {
                hasHangingPieces = true;
                break;
            }
        }
        
        if (hasHangingPieces) {
            extension = 1; // Extend search when tactics are brewing
        }
    }
    
    depth += extension;
    
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
        int eval = QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context);
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
            
            // Late Move Reduction - only for non-PV moves
            if (!foundPV && depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor)) {
                
                // Reduce more for later moves
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                // Don't reduce too much in PV nodes or important positions
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                // Try reduced search first
                int reducedEval = AlphaBetaSearch(newBoard, depth - 1 - reduction, alpha, alpha + 1, false, ply + 1, historyTable, context);
                
                // If reduced search fails high, do full search
                if (reducedEval > alpha) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1, historyTable, context);
                } else {
                    eval = reducedEval;
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
            
            // Late Move Reduction for black - only for non-PV moves
            if (!foundPV && depth >= 3 && moveCount > 0 && !isCaptureMove && !isCheckMove && 
                !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor)) {
                
                int reduction = 1;
                if (moveCount > 3) reduction = 2;
                if (moveCount > 6) reduction = 3;
                
                if (scoredMove.score > 1000) reduction = std::max(1, reduction - 1);
                
                // Try reduced search first
                int reducedEval = AlphaBetaSearch(newBoard, depth - 1 - reduction, beta - 1, beta, true, ply + 1, historyTable, context);
                
                // If reduced search fails low, do full search
                if (reducedEval < beta) {
                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1, historyTable, context);
                } else {
                    eval = reducedEval;
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
                const Piece& piece = board.squares[i].Piece;
                if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::PAWN) {
                    ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                                ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                    for (int j = 0; j < 64; j++) {
                        const Piece& enemyPiece = board.squares[j].Piece;
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
        
        // Print progress for deeper searches
        if (depth >= 5) {
            std::cout << "Depth " << depth << ": score=" << searchScore 
                      << ", move=" << result.bestMove.first/8 << "," << result.bestMove.first%8 
                      << " to " << result.bestMove.second/8 << "," << result.bestMove.second%8 << std::endl;
        }
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
        
        // Optional: Print search info
        if (currentDepth >= 6) {
            std::cout << "Depth " << currentDepth << ": score=" << bestEval 
                      << ", move=" << (bestMove.first % 8) << "," << (bestMove.first / 8) 
                      << " to " << (bestMove.second % 8) << "," << (bestMove.second / 8) << "\n";
        }
    }
    
    return bestMove;
}

// Static Exchange Evaluation - evaluates capture sequences
int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare) {
    ChessPieceType victim = board.squares[toSquare].Piece.PieceType;
    ChessPieceType attacker = board.squares[fromSquare].Piece.PieceType;
    
    if (victim == ChessPieceType::NONE) {
        return 0; // No capture
    }
    
    // Simple SEE - just check if capture is profitable
    // int gain = getPieceValue(victim) - getPieceValue(attacker); // Currently unused
    
    // TODO: Full SEE would simulate the entire exchange sequence
    // For now, use a simplified version that checks basic profitability
    
    // If we're capturing a more valuable piece, it's likely good
    if (getPieceValue(victim) >= getPieceValue(attacker)) {
        return getPieceValue(victim);
    }
    
    // If capturing a less valuable piece, be more careful
    // Check if the piece is defended
    ChessPieceColor attackerColor = board.squares[fromSquare].Piece.PieceColor;
    ChessPieceColor defenderColor = (attackerColor == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    // Simple heuristic: if target square is defended by a pawn, the capture might be bad
    // Check adjacent squares for defending pawns
    int row = toSquare / 8;
    int col = toSquare % 8;
    
    if (defenderColor == ChessPieceColor::BLACK) {
        // Check if white pawn defends (black pawns attack diagonally down)
        if (row < 7) {
            if (col > 0 && board.squares[(row + 1) * 8 + (col - 1)].Piece.PieceType == ChessPieceType::PAWN &&
                board.squares[(row + 1) * 8 + (col - 1)].Piece.PieceColor == ChessPieceColor::BLACK) {
                return getPieceValue(victim) - getPieceValue(attacker);
            }
            if (col < 7 && board.squares[(row + 1) * 8 + (col + 1)].Piece.PieceType == ChessPieceType::PAWN &&
                board.squares[(row + 1) * 8 + (col + 1)].Piece.PieceColor == ChessPieceColor::BLACK) {
                return getPieceValue(victim) - getPieceValue(attacker);
            }
        }
    } else {
        // Check if black pawn defends (white pawns attack diagonally up)
        if (row > 0) {
            if (col > 0 && board.squares[(row - 1) * 8 + (col - 1)].Piece.PieceType == ChessPieceType::PAWN &&
                board.squares[(row - 1) * 8 + (col - 1)].Piece.PieceColor == ChessPieceColor::WHITE) {
                return getPieceValue(victim) - getPieceValue(attacker);
            }
            if (col < 7 && board.squares[(row - 1) * 8 + (col + 1)].Piece.PieceType == ChessPieceType::PAWN &&
                board.squares[(row - 1) * 8 + (col + 1)].Piece.PieceColor == ChessPieceColor::WHITE) {
                return getPieceValue(victim) - getPieceValue(attacker);
            }
        }
    }
    
    // If no obvious defense, assume capture is good
    return getPieceValue(victim);
}

// Forward declarations and helper functions
bool isDiscoveredCheck(const Board& board, int from, int to);
bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
int staticExchangeEval(const Board& board, int fromSquare, int toSquare);

// Helper function implementations
bool isDiscoveredCheck(const Board& board, int from, int /* to */) {
    // Simplified check - assume any non-direct checking piece might be a discovered check
    ChessPieceType movingPiece = board.squares[from].Piece.PieceType;
    return (movingPiece != ChessPieceType::QUEEN && 
            movingPiece != ChessPieceType::ROOK && 
            movingPiece != ChessPieceType::BISHOP);
}

bool isPromotion(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].Piece.PieceType;
    ChessPieceColor color = board.squares[from].Piece.PieceColor;
    
    if (piece != ChessPieceType::PAWN) return false;
    
    int destRow = to / 8;
    return (color == ChessPieceColor::WHITE && destRow == 7) ||
           (color == ChessPieceColor::BLACK && destRow == 0);
}

bool isCastling(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].Piece.PieceType;
    if (piece != ChessPieceType::KING) return false;
    
    return std::abs(to - from) == 2;
} 