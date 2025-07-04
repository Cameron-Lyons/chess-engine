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
        
        ChessPieceType movingPiece = board.squares[srcPos].piece.PieceType;
        ChessPieceType targetPiece = board.squares[destPos].piece.PieceType;
        ChessPieceColor movingColor = board.squares[srcPos].piece.PieceColor;
        
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
            
            // Strong SEE integration - DRASTICALLY increased penalties for bad captures
            if (seeValue > 0) {
                score += std::min(seeValue, 500); // Cap SEE bonus to avoid overflow
            } else if (seeValue < 0) {
                // MASSIVE penalty for bad captures - this is critical for tactical strength
                score += std::max(seeValue * 2, -2000); // Double the penalty, cap at -2000
                
                // Extra penalty if capturing with a valuable piece
                if (attackerValue >= 900) { // Queen making bad capture
                    score -= 1500; // Huge additional penalty
                } else if (attackerValue >= 500) { // Rook making bad capture
                    score -= 800; // Large additional penalty
                } else if (attackerValue >= 300) { // Bishop/Knight making bad capture
                    score -= 400; // Moderate additional penalty
                }
                
                // Penalty scales with how bad the capture is
                if (seeValue <= -300) { // Very bad capture (lose major piece)
                    score -= 1000; // Extremely bad
                } else if (seeValue <= -100) { // Bad capture (lose minor piece)
                    score -= 500; // Very bad
                }
            } else {
                // seeValue == 0: Equal exchange - slightly positive
                score += 50;
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
                const Piece& defenderPiece = board.squares[i].piece;
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
            
            // CRITICAL: Apply queen safety penalties even to checking moves!
            if (movingPiece == ChessPieceType::QUEEN) {
                int destRow = destPos / 8;
                int destCol = destPos % 8;
                
                bool isCorner = ((destRow == 0 || destRow == 7) && (destCol == 0 || destCol == 7));
                bool isEdge = (destRow == 0 || destRow == 7 || destCol == 0 || destCol == 7);
                
                // Apply safety penalties to queen checks - safety is more important than check!
                if (isCorner) {
                    score -= 8000; // Check from corner is usually bad
                } else if (isEdge) {
                    score -= 5000; // Check from edge is often dangerous
                }
                
                // Check if queen check move puts queen in danger
                ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? 
                                            ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                bool destSquareAttacked = false;
                int attackerCount = 0;
                
                for (int i = 0; i < 64; i++) {
                    const Piece& enemyPiece = board.squares[i].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE || 
                        enemyPiece.PieceColor != enemyColor) continue;
                    
                    if (canPieceAttackSquare(board, i, destPos)) {
                        destSquareAttacked = true;
                        attackerCount++;
                    }
                }
                
                if (destSquareAttacked) {
                    // Even checking moves are bad if queen gets attacked
                    score -= 6000 + (attackerCount * 1000);
                }
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
                const Piece& enemyPiece = board.squares[i].piece;
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
                    const Piece& enemyPiece = board.squares[i].piece;
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
            testBoard.squares[destPos].piece = testBoard.squares[srcPos].piece;
            testBoard.squares[srcPos].piece.PieceType = ChessPieceType::NONE;
            
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
                // MASSIVE penalties for queen moves to dangerous squares - MUCH higher than before
                bool isCorner = ((destRow == 0 || destRow == 7) && (destCol == 0 || destCol == 7));
                bool isEdge = (destRow == 0 || destRow == 7 || destCol == 0 || destCol == 7);
                
                if (isCorner) {
                    score -= 8000; // EXTREME penalty for corner - higher than check bonus!
                } else if (isEdge) {
                    score -= 5000; // MASSIVE penalty for edge - higher than check bonus!
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
                        const Piece& friendlyPiece = board.squares[i].piece;
                        if (friendlyPiece.PieceType == ChessPieceType::NONE || 
                            friendlyPiece.PieceColor != movingColor || 
                            i == srcPos || 
                            friendlyPiece.PieceType == ChessPieceType::PAWN) continue;
                        
                        if (canPieceAttackSquare(board, i, destPos)) {
                            supportCount++;
                        }
                    }
                    
                    if (supportCount == 0) {
                        score -= 6000; // EXTREME penalty for unsupported queen in enemy territory
                    } else if (supportCount == 1) {
                        score -= 3000; // MASSIVE penalty for barely supported queen
                    }
                }
                
                // Additional safety check: Is destination square attacked by enemy pieces?
                ChessPieceColor enemyColor = (movingColor == ChessPieceColor::WHITE) ? 
                                            ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                bool destSquareAttacked = false;
                int attackerCount = 0;
                
                for (int i = 0; i < 64; i++) {
                    const Piece& enemyPiece = board.squares[i].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE || 
                        enemyPiece.PieceColor != enemyColor) continue;
                    
                    if (canPieceAttackSquare(board, i, destPos)) {
                        destSquareAttacked = true;
                        attackerCount++;
                    }
                }
                
                if (destSquareAttacked) {
                    // MASSIVE penalty for moving queen to attacked square
                    score -= 7000 + (attackerCount * 1000); // Penalty increases with number of attackers
                }
                
                // Penalize early queen development (encourage piece development first)
                if ((movingColor == ChessPieceColor::WHITE && destRow > 3) ||
                    (movingColor == ChessPieceColor::BLACK && destRow < 4)) {
                    // Count developed pieces (not on starting squares)
                    int developedPieces = 0;
                    for (int i = 0; i < 64; i++) {
                        const Piece& piece = board.squares[i].piece;
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
                        score -= 2000; // MUCH higher penalty for early queen moves before piece development
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
                    const Piece& enemyPiece = board.squares[i].piece;
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
            const Piece& piece = board.squares[i].piece;
            if (piece.PieceType == ChessPieceType::NONE) continue;
            if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) continue;
            
            // Quick check if piece is under attack
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            bool isAttacked = false;
            for (int j = 0; j < 64; j++) {
                const Piece& enemyPiece = board.squares[j].piece;
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
    const Piece& victim = board.squares[toSquare].piece;
    const Piece& attacker = board.squares[fromSquare].piece;
    
    // No capture = no gain
    if (victim.PieceType == ChessPieceType::NONE) {
        return 0;
    }
    
    // Simple case: undefended piece
    ChessPieceColor attackerColor = attacker.PieceColor;
    ChessPieceColor defenderColor = (attackerColor == ChessPieceColor::WHITE) ? 
                                   ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    // Check if target square is defended
    bool isDefended = false;
    for (int i = 0; i < 64; i++) {
        const Piece& defender = board.squares[i].piece;
        if (i != toSquare && // Don't count the victim as defending itself
            defender.PieceColor == defenderColor && 
            defender.PieceType != ChessPieceType::NONE &&
            canPieceAttackSquare(board, i, toSquare)) {
            isDefended = true;
            break;
        }
    }
    
    // If undefended, we simply win the victim
    if (!isDefended) {
        return getPieceValue(victim.PieceType);
    }
    
    // ===================================================================
    // DEFENDED PIECE - Full SEE Calculation
    // ===================================================================
    
    // Build ordered lists of attackers and defenders
    std::vector<std::pair<int, int>> attackerList; // {square, pieceValue}
    std::vector<std::pair<int, int>> defenderList;
    
    // Add initial attacker
    attackerList.push_back({fromSquare, getPieceValue(attacker.PieceType)});
    
    // Find all other attackers and defenders
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || i == fromSquare) continue;
        
        if (canPieceAttackSquare(board, i, toSquare)) {
            int pieceValue = getPieceValue(piece.PieceType);
            if (piece.PieceColor == attackerColor) {
                attackerList.push_back({i, pieceValue});
            } else if (i != toSquare) { // Don't count victim as defender
                defenderList.push_back({i, pieceValue});
            }
        }
    }
    
    // Sort by piece value (least valuable first - better to sacrifice pawns before queens)
    auto comparePieceValue = [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
        return a.second < b.second;
    };
    std::sort(attackerList.begin(), attackerList.end(), comparePieceValue);
    std::sort(defenderList.begin(), defenderList.end(), comparePieceValue);
    
    // ===================================================================
    // SIMULATE EXCHANGE SEQUENCE
    // ===================================================================
    
    std::vector<int> materialSwing;
    materialSwing.push_back(getPieceValue(victim.PieceType)); // Initial capture
    
    size_t attackerIdx = 1; // Start from second attacker (first already used)
    size_t defenderIdx = 0;
    int currentVictimValue = getPieceValue(attacker.PieceType);
    bool isAttackerTurn = false; // Next is defender's turn
    
    // Continue the exchange
    while (true) {
        if (isAttackerTurn) {
            // Attacker's turn
            if (attackerIdx >= attackerList.size()) break;
            
            materialSwing.push_back(currentVictimValue);
            currentVictimValue = attackerList[attackerIdx].second;
            attackerIdx++;
        } else {
            // Defender's turn  
            if (defenderIdx >= defenderList.size()) break;
            
            materialSwing.push_back(currentVictimValue);
            currentVictimValue = defenderList[defenderIdx].second;
            defenderIdx++;
        }
        
        isAttackerTurn = !isAttackerTurn;
    }
    
    // ===================================================================
    // MINIMAX EVALUATION - Work backwards to find best result
    // ===================================================================
    
    if (materialSwing.empty()) return 0;
    
    // Work backwards through the material swings
    int result = materialSwing.back();
    
    for (int i = static_cast<int>(materialSwing.size()) - 2; i >= 0; i--) {
        // Each player chooses the best outcome for themselves
        if (i % 2 == 0) {
            // Attacker's turn - wants to maximize
            result = std::max(materialSwing[i] - result, 0);
        } else {
            // Defender's turn - wants to minimize attacker's gain
            result = materialSwing[i] - result;
        }
    }
    
    return result;
}

// ===================================================================
// SEE HELPER FUNCTIONS
// ===================================================================

// Check if a capture is good (SEE >= 0)
bool isGoodCapture(const Board& board, int fromSquare, int toSquare) {
    return staticExchangeEvaluation(board, fromSquare, toSquare) >= 0;
}

// Check if a capture is profitable above a certain threshold
bool isCaptureProfitable(const Board& board, int fromSquare, int toSquare, int threshold) {
    return staticExchangeEvaluation(board, fromSquare, toSquare) >= threshold;
}

// Find the smallest attacker for a given square and color
// Returns the square of the smallest attacker, or -1 if none
int getSmallestAttacker(const Board& board, int targetSquare, ChessPieceColor color) {
    int smallestAttacker = -1;
    int smallestValue = 10000; // Higher than any piece value
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }
        
        if (canPieceAttackSquare(board, i, targetSquare)) {
            int pieceValue = getPieceValue(piece.PieceType);
            if (pieceValue < smallestValue) {
                smallestValue = pieceValue;
                smallestAttacker = i;
            }
        }
    }
    
    return smallestAttacker;
}

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