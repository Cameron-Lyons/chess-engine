#ifndef ADVANCED_SEARCH_H
#define ADVANCED_SEARCH_H

#include "ChessBoard.h"
#include "search.h"
#include <vector>
#include <unordered_map>

// Advanced search techniques for improved engine strength
class AdvancedSearch {
public:
    // Futility pruning - skip moves that are unlikely to improve alpha
    static bool futilityPruning(const Board& board, int depth, int alpha, int beta, int staticEval);
    
    // Static null move pruning - skip moves when static eval is already good
    static bool staticNullMovePruning(const Board& board, int depth, int alpha, int beta, int staticEval);
    
    // Multi-cut pruning - use multiple null moves to detect zugzwang
    static bool multiCutPruning(const Board& board, int depth, int alpha, int beta, int r);
    
    // Internal iterative deepening - when no hash move is available
    static std::pair<int, int> internalIterativeDeepening(const Board& board, int depth, int alpha, int beta);
    
    // Singular extensions - extend search for moves that are clearly best
    static bool singularExtension(const Board& board, int depth, const std::pair<int, int>& move, int alpha, int beta);
    
    // History pruning - skip quiet moves that historically perform poorly
    static bool historyPruning(const Board& board, int depth, const std::pair<int, int>& move, const ThreadSafeHistory& history);
    
    // Late move pruning - skip quiet moves in late move situations
    static bool lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck);
    
    // Recapture extensions - extend search for recaptures
    static bool recaptureExtension(const Board& board, const std::pair<int, int>& move, int depth);
    
    // Check extensions - extend search for checking moves
    static bool checkExtension(const Board& board, const std::pair<int, int>& move, int depth);
    
    // Pawn push extensions - extend search for advancing pawns
    static bool pawnPushExtension(const Board& board, const std::pair<int, int>& move, int depth);
    
    // Passed pawn extensions - extend search for passed pawn moves
    static bool passedPawnExtension(const Board& board, const std::pair<int, int>& move, int depth);
};

// Enhanced move ordering for better alpha-beta efficiency
class EnhancedMoveOrdering {
public:
    struct MoveScore {
        std::pair<int, int> move;
        int score;
        MoveScore(const std::pair<int, int>& m, int s) : move(m), score(s) {}
        bool operator<(const MoveScore& other) const { return score > other.score; }
    };
    
    // Score moves with multiple heuristics
    static std::vector<MoveScore> scoreMoves(const Board& board, 
                                           const std::vector<std::pair<int, int>>& moves,
                                           const ThreadSafeHistory& history,
                                           const KillerMoves& killers,
                                           int ply,
                                           const std::pair<int, int>& hashMove = {-1, -1});
    
    // SEE (Static Exchange Evaluation) based move ordering
    static int getSEEScore(const Board& board, const std::pair<int, int>& move);
    
    // Threat detection for move ordering
    static int getThreatScore(const Board& board, const std::pair<int, int>& move);
    
    // Mobility-based move scoring
    static int getMobilityScore(const Board& board, const std::pair<int, int>& move);
    
    // Position-based move scoring
    static int getPositionalScore(const Board& board, const std::pair<int, int>& move);
};

// Time management for tournament play
class TimeManager {
public:
    struct TimeControl {
        int baseTime;      // Base time in milliseconds
        int increment;     // Increment per move in milliseconds
        int movesToGo;     // Moves until next time control (0 = sudden death)
        bool isInfinite;   // Infinite time mode
    };
    
    TimeManager(const TimeControl& tc);
    
    // Calculate time allocation for current move
    int allocateTime(const Board& board, int depth, int nodes, bool isInCheck);
    
    // Check if we should stop searching
    bool shouldStop(int elapsedTime, int allocatedTime, int depth, int nodes);
    
    // Update time management based on game progress
    void updateGameProgress(int moveNumber, int totalMoves);
    
    // Emergency time management
    bool isEmergencyTime(int remainingTime, int allocatedTime);
    
private:
    TimeControl timeControl;
    int moveNumber;
    int totalMoves;
    double timeFactor;
    
    // Helper functions
    int calculateBaseTime();
    int calculateIncrement();
    double getTimeFactor(int depth, int nodes);
};

// Opening book with enhanced features
class EnhancedOpeningBook {
public:
    struct BookEntry {
        std::pair<int, int> move;
        int weight;        // Move weight (higher = more likely)
        int games;         // Number of games this move was played
        float winRate;     // Win rate for this move
        int averageRating; // Average rating of players who played this move
    };
    
    EnhancedOpeningBook(const std::string& bookPath = "books/opening_book.bin");
    
    // Get book moves for position
    std::vector<BookEntry> getBookMoves(const Board& board);
    
    // Get best book move
    std::pair<int, int> getBestMove(const Board& board, bool randomize = false);
    
    // Check if position is in book
    bool isInBook(const Board& board);
    
    // Add move to book
    void addMove(const Board& board, const BookEntry& entry);
    
    // Save book to file
    void saveBook(const std::string& path);
    
    // Load book from file
    void loadBook(const std::string& path);
    
private:
    std::unordered_map<std::string, std::vector<BookEntry>> book;
    std::string bookPath;
    
    // Helper functions
    std::string boardToKey(const Board& board);
    void normalizeWeights(std::vector<BookEntry>& entries);
};

#endif // ADVANCED_SEARCH_H 