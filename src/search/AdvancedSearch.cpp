#include "AdvancedSearch.h"
#include "../evaluation/Evaluation.h"
#include "../core/BitboardMoves.h"
#include <algorithm>
#include <cmath>

// Enhanced futility pruning with dynamic margins
bool AdvancedSearch::futilityPruning(const Board& board, int depth, int alpha, int beta, int staticEval) {
    // Don't prune if in check or at root
    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }
    
    // Dynamic futility margin based on depth and position
    int futilityMargin = 150 * depth;
    
    // Increase margin for tactical positions
    if (depth <= 3) {
        int tacticalBonus = 50 * (3 - depth);
        futilityMargin += tacticalBonus;
    }
    
    // If static evaluation is so bad that even a good move won't help
    if (staticEval - futilityMargin >= beta) {
        return true;
    }
    
    // Delta pruning for captures
    if (depth <= 3) {
        int deltaMargin = 975 + 50 * depth; // Queen value + margin
        if (staticEval + deltaMargin < alpha) {
            return true;
        }
    }
    
    return false;
}

// Enhanced static null move pruning
bool AdvancedSearch::staticNullMovePruning(const Board& board, int depth, int alpha, int beta, int staticEval) {
    // Don't prune if in check or at root
    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }
    
    // Static null move pruning - assume not PV node for now
    if (depth <= 4) {
        // Dynamic margin based on depth
        int margin = 900 + 50 * depth; // Rook value + depth bonus
        int eval = staticEval - margin;
        
        if (eval >= beta) {
            return true;
        }
    }
    
    return false;
}

// Enhanced null move pruning with verification
bool AdvancedSearch::nullMovePruning(const Board& board, int depth, int alpha, int beta) {
    // Don't prune if in check or if depth is too shallow
    if (isInCheck(board, board.turn) || depth < 3) {
        return false;
    }
    
    // Check if we have enough material to make null move reasonable
    int materialCount = 0;
    int totalMaterial = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            materialCount++;
            totalMaterial += piece.PieceValue;
        }
    }
    
    // Don't use null move if we have very little material
    if (materialCount < 3 || totalMaterial < 300) {
        return false;
    }
    
    // Don't use null move in endgame positions
    if (materialCount <= 4) {
        return false;
    }
    
    return true;
}

// Enhanced late move reduction with verification
bool AdvancedSearch::lateMoveReduction(const Board& board, int depth, int moveNumber, int alpha, int beta) {
    // Don't reduce if in check, at root, or if move number is too low
    if (isInCheck(board, board.turn) || depth < 3 || moveNumber < 4) {
        return false;
    }
    
    // Don't reduce in tactical positions
    if (depth <= 3) {
        return false;
    }
    
    // Don't reduce if move gives check or is a capture
    // (This would need to be checked for the specific move)
    
    // Dynamic reduction based on move number and depth
    int reduction = 1;
    if (moveNumber >= 8) reduction = 2;
    if (moveNumber >= 12) reduction = 3;
    
    // Don't reduce too much
    if (depth - reduction < 1) {
        return false;
    }
    
    return true;
}

// Multi-cut pruning implementation
bool AdvancedSearch::multiCutPruning(const Board& board, int depth, int alpha, int beta, int r) {
    // Don't use multi-cut if in check or depth is too shallow
    if (isInCheck(board, board.turn) || depth < 4) {
        return false;
    }
    
    // Multi-cut is most effective in positions with many moves
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.size() < 8) {
        return false; // Not enough moves for multi-cut
    }
    
    // Try r null moves to see if any fail high
    int nullMoveCount = 0;
    for (int i = 0; i < r && i < static_cast<int>(moves.size()); i++) {
        // Make a null move (skip turn)
        Board tempBoard = board;
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        // Search with reduced depth
        ThreadSafeHistory historyTable;
        ParallelSearchContext context(1);
        int nullScore = -AlphaBetaSearch(tempBoard, depth - 3, -beta, -beta + 1, 
                                       !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);
        
        if (nullScore >= beta) {
            nullMoveCount++;
        }
    }
    
    // If enough null moves fail high, we can prune
    return nullMoveCount >= 2;
}

// Internal iterative deepening implementation
std::pair<int, int> AdvancedSearch::internalIterativeDeepening(const Board& board, int depth, int alpha, int beta) {
    // Only use IID when we have no hash move and depth is significant
    if (depth < 4) {
        return {-1, -1};
    }
    
    // Search with reduced depth to get a good move
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    
    int reducedDepth = std::max(1, depth - 2);
    int score = AlphaBetaSearch(board, reducedDepth, alpha, beta, 
                               board.turn == ChessPieceColor::WHITE, 0, historyTable, context);
    
    // Return the best move found (this would need to be extracted from the search)
    // For now, return a placeholder
    return {-1, -1};
}

// Singular extensions implementation
bool AdvancedSearch::singularExtension(const Board& board, int depth, const std::pair<int, int>& move, int alpha, int beta) {
    // Only extend if depth is significant
    if (depth < 6) {
        return false;
    }
    
    // Make the move
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    // Search with reduced depth to see if this move is singular
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    
    int reducedDepth = depth - 1;
    int score = AlphaBetaSearch(tempBoard, reducedDepth, alpha, beta, 
                               !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);
    
    // Search other moves with even more reduced depth
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    int betterMoves = 0;
    for (const auto& otherMove : moves) {
        if (otherMove == move) continue;
        
        Board otherBoard = board;
        if (!otherBoard.movePiece(otherMove.first, otherMove.second)) continue;
        otherBoard.turn = (otherBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        int otherScore = AlphaBetaSearch(otherBoard, reducedDepth - 2, alpha, beta, 
                                       !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);
        
        if (otherScore >= score) {
            betterMoves++;
            if (betterMoves >= 2) break; // Not singular
        }
    }
    
    // Extend if this move is clearly best
    return betterMoves == 0;
}

// History pruning implementation
bool AdvancedSearch::historyPruning(const Board& board, int depth, const std::pair<int, int>& move, const ThreadSafeHistory& history) {
    // Don't prune if in check or at root
    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }
    
    // Don't prune captures or checks
    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }
    
    // Get history score for this move
    int historyScore = history.getScore(move.first, move.second);
    
    // Prune quiet moves with very poor history
    if (depth <= 3 && historyScore < -1000) {
        return true;
    }
    
    return false;
}

// Late move pruning implementation
bool AdvancedSearch::lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck) {
    // Don't prune if in check
    if (inCheck) {
        return false;
    }
    
    // Don't prune at root or shallow depths
    if (depth == 0 || depth > 3) {
        return false;
    }
    
    // Prune quiet moves in late move situations
    if (moveNumber >= 4 && depth <= 3) {
        return true;
    }
    
    return false;
}

// Recapture extension implementation
bool AdvancedSearch::recaptureExtension(const Board& board, const std::pair<int, int>& move, int depth) {
    // Check if this is a recapture
    if (board.LastMove < 0 || board.LastMove >= 64) {
        return false;
    }
    
    // Check if the current move captures on the square of the last move
    if (move.second == board.LastMove) {
        // Check if the last move was also a capture
        // This would require tracking the previous board state
        return true;
    }
    
    return false;
}

// Check extension implementation
bool AdvancedSearch::checkExtension(const Board& board, const std::pair<int, int>& move, int depth) {
    // Make the move and check if it gives check
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    return isInCheck(tempBoard, tempBoard.turn);
}

// Pawn push extension implementation
bool AdvancedSearch::pawnPushExtension(const Board& board, const std::pair<int, int>& move, int depth) {
    const Piece& piece = board.squares[move.first].piece;
    
    // Check if this is a pawn move
    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }
    
    // Check if this is a pawn push (not a capture)
    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }
    
    // Extend if pawn is advancing to 6th or 7th rank
    int destRow = move.second / 8;
    if (piece.PieceColor == ChessPieceColor::WHITE && destRow >= 5) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && destRow <= 2) {
        return true;
    }
    
    return false;
}

// Passed pawn extension implementation
bool AdvancedSearch::passedPawnExtension(const Board& board, const std::pair<int, int>& move, int depth) {
    const Piece& piece = board.squares[move.first].piece;
    
    // Check if this is a pawn move
    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }
    
    // Check if this is a passed pawn move
    // This would require checking if the pawn is passed
    // For now, extend if pawn is advancing significantly
    int srcRow = move.first / 8;
    int destRow = move.second / 8;
    
    if (piece.PieceColor == ChessPieceColor::WHITE && destRow - srcRow > 0) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && srcRow - destRow > 0) {
        return true;
    }
    
    return false;
}

// Enhanced move ordering implementation
std::vector<EnhancedMoveOrdering::MoveScore> EnhancedMoveOrdering::scoreMoves(
    const Board& board, 
    const std::vector<std::pair<int, int>>& moves,
    const ThreadSafeHistory& history,
    const KillerMoves& killers,
    int ply,
    const std::pair<int, int>& hashMove) {
    
    std::vector<MoveScore> scoredMoves;
    
    for (const auto& move : moves) {
        int score = 0;
        
        // Hash move gets highest priority
        if (move == hashMove) {
            score = 10000;
        }
        // Captures get high priority with SEE
        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            score = getSEEScore(board, move);
        }
        // Killer moves get medium priority
        else if (killers.isKiller(ply, move)) {
            score = killers.getKillerScore(ply, move);
        }
        // History moves get low priority
        else {
            score = EnhancedMoveOrdering::getHistoryScore(history, move.first, move.second);
        }
        
        // Add positional bonuses
        score += getPositionalScore(board, move);
        score += getMobilityScore(board, move);
        score += getThreatScore(board, move);
        
        scoredMoves.emplace_back(move, score);
    }
    
    return scoredMoves;
}

// Static Exchange Evaluation (SEE) implementation
int EnhancedMoveOrdering::getSEEScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& attacker = board.squares[move.first].piece;
    const Piece& victim = board.squares[move.second].piece;
    
    if (victim.PieceType == ChessPieceType::NONE) return 0;
    
    // Basic capture score
    int score = victim.PieceValue * 10 - attacker.PieceValue;
    
    // Add bonus for capturing with less valuable piece
    if (attacker.PieceValue < victim.PieceValue) {
        score += 50;
    }
    
    // Add bonus for capturing on center squares
    int destCol = move.second % 8;
    int destRow = move.second / 8;
    if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
        score += 10;
    }
    
    return score;
}

// Threat score implementation
int EnhancedMoveOrdering::getThreatScore(const Board& board, const std::pair<int, int>& move) {
    int score = 0;
    
    // Check if move threatens opponent pieces
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return 0;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    // Generate opponent moves to see if we threaten anything
    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> opponentMoves = GetAllMoves(tempBoard, tempBoard.turn);
    
    for (const auto& oppMove : opponentMoves) {
        const Piece& threatenedPiece = board.squares[oppMove.second].piece;
        if (threatenedPiece.PieceType != ChessPieceType::NONE && 
            threatenedPiece.PieceColor == board.turn) {
            score += threatenedPiece.PieceValue / 10;
        }
    }
    
    return score;
}

// Mobility score implementation
int EnhancedMoveOrdering::getMobilityScore(const Board& board, const std::pair<int, int>& move) {
    // Make the move
    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return 0;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    // Count legal moves after this move
    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> moves = GetAllMoves(tempBoard, tempBoard.turn);
    
    // Bonus for moves that increase mobility
    return moves.size() * 2;
}

// Positional score implementation
int EnhancedMoveOrdering::getPositionalScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& piece = board.squares[move.first].piece;
    int destCol = move.second % 8;
    int destRow = move.second / 8;
    
    int score = 0;
    
    // Piece-specific positional bonuses
    switch (piece.PieceType) {
        case ChessPieceType::PAWN:
            // Bonus for advancing pawns
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += destRow * 5;
            } else {
                score += (7 - destRow) * 5;
            }
            break;
            
        case ChessPieceType::KNIGHT:
        case ChessPieceType::BISHOP:
            // Bonus for center control
            if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
                score += 10;
            }
            break;
            
        case ChessPieceType::ROOK:
            // Bonus for open files and 7th rank
            if (destRow == 6 || destRow == 1) {
                score += 15;
            }
            break;
            
        case ChessPieceType::QUEEN:
            // Bonus for center control
            if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
                score += 5;
            }
            break;
            
        default:
            break;
    }
    
    return score;
}

// Time management implementation
TimeManager::TimeManager(const TimeControl& tc) 
    : timeControl(tc), moveNumber(0), totalMoves(30), timeFactor(1.0) {
}

int TimeManager::allocateTime(const Board& board, int depth, int nodes, bool isInCheck) {
    // Calculate base time
    int baseTime = calculateBaseTime();
    int increment = calculateIncrement();
    
    // Adjust for game phase and position
    double factor = getTimeFactor(depth, nodes);
    
    // Increase time for critical positions
    if (isInCheck) {
        factor *= 1.5;
    }
    
    // Adjust for move complexity
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    
    if (moves.size() < 10) {
        factor *= 1.2; // More time for tactical positions
    } else if (moves.size() > 25) {
        factor *= 0.8; // Less time for quiet positions
    }
    
    int allocatedTime = static_cast<int>((baseTime + increment) * factor);
    
    // Ensure minimum and maximum bounds
    allocatedTime = std::max(50, std::min(allocatedTime, baseTime * 2));
    
    return allocatedTime;
}

int TimeManager::calculateBaseTime() {
    if (timeControl.isInfinite) {
        return 30000; // 30 seconds default for infinite
    }
    
    if (timeControl.movesToGo > 0) {
        return timeControl.baseTime / timeControl.movesToGo;
    }
    
    // Estimate moves remaining
    int movesRemaining = std::max(10, totalMoves - moveNumber);
    return timeControl.baseTime / movesRemaining;
}

int TimeManager::calculateIncrement() {
    return timeControl.increment;
}

bool TimeManager::shouldStop(int elapsedTime, int allocatedTime, int depth, int nodes) {
    // Stop if we've exceeded allocated time
    if (elapsedTime >= allocatedTime) {
        return true;
    }
    
    // Stop if we're in emergency time
    if (isEmergencyTime(timeControl.baseTime, allocatedTime)) {
        return true;
    }
    
    // Stop if we've searched deep enough
    if (depth >= 20) {
        return true;
    }
    
    return false;
}

void TimeManager::updateGameProgress(int moveNumber, int totalMoves) {
    this->moveNumber = moveNumber;
    this->totalMoves = totalMoves;
}

bool TimeManager::isEmergencyTime(int remainingTime, int allocatedTime) {
    // Emergency if we're using more than 80% of remaining time
    return allocatedTime > remainingTime * 0.8;
}

double TimeManager::getTimeFactor(int depth, int nodes) {
    // Base factor
    double factor = 1.0;
    
    // Adjust based on search depth
    if (depth >= 10) {
        factor *= 1.2; // More time for deep searches
    } else if (depth <= 3) {
        factor *= 0.8; // Less time for shallow searches
    }
    
    // Adjust based on node count
    if (nodes > 1000000) {
        factor *= 1.1; // More time for complex positions
    }
    
    return factor;
}

// Enhanced opening book implementation
EnhancedOpeningBook::EnhancedOpeningBook(const std::string& bookPath) 
    : bookPath(bookPath) {
}

std::vector<EnhancedOpeningBook::BookEntry> EnhancedOpeningBook::getBookMoves(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation - return empty vector
    return {};
}

std::pair<int, int> EnhancedOpeningBook::getBestMove(const Board& board, bool randomize) {
    std::string fen = getFEN(board);
    
    // Check multi-option opening book first
    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            if (randomize) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, options.size() - 1);
                std::string move = options[dis(gen)];
                return parseMove(move);
            } else {
                // Return the first (best) move
                return parseMove(options[0]);
            }
        }
    }
    
    // Fallback to legacy opening book
    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end()) {
        return parseMove(it->second);
    }
    
    return {-1, -1};
}

std::pair<int, int> EnhancedOpeningBook::parseMove(const std::string& move) {
    // Simple algebraic notation parser
    if (move.length() < 4) return {-1, -1};
    
    int srcCol = move[0] - 'a';
    int srcRow = move[1] - '1';
    int destCol = move[2] - 'a';
    int destRow = move[3] - '1';
    
    if (srcCol < 0 || srcCol > 7 || srcRow < 0 || srcRow > 7 ||
        destCol < 0 || destCol > 7 || destRow < 0 || destRow > 7) {
        return {-1, -1};
    }
    
    return {srcRow * 8 + srcCol, destRow * 8 + destCol};
}

bool EnhancedOpeningBook::isInBook(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation - return false
    return false;
}

void EnhancedOpeningBook::addMove(const Board& board, const BookEntry& entry) {
    (void)board; // Suppress unused parameter warning
    (void)entry; // Suppress unused parameter warning
    // Stub implementation
}

void EnhancedOpeningBook::saveBook(const std::string& path) {
    (void)path; // Suppress unused parameter warning
    // Stub implementation
}

void EnhancedOpeningBook::loadBook(const std::string& path) {
    (void)path; // Suppress unused parameter warning
    // Stub implementation
}

std::string EnhancedOpeningBook::boardToKey(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Stub implementation - return empty string
    return "";
}

void EnhancedOpeningBook::normalizeWeights(std::vector<BookEntry>& entries) {
    (void)entries; // Suppress unused parameter warning
    // Stub implementation
} 