#ifndef UCI_H
#define UCI_H

#include "../core/ChessBoard.h"
#include "../search/search.h"
#include "../search/AdvancedSearch.h"
#include "../ai/NeuralNetwork.h"
#include "../ai/EndgameTablebase.h"
#include <string>
#include <vector>
#include <memory>

// UCI (Universal Chess Interface) protocol implementation
class UCIEngine {
public:
    struct UCIOptions {
        // Search options
        int hashSize = 32;           // Transposition table size in MB
        int threads = 1;             // Number of search threads
        int multiPV = 1;             // Number of principal variations
        bool ponder = false;         // Enable pondering
        bool ownBook = true;         // Use own opening book
        
        // Time management
        int moveOverhead = 10;       // Move overhead in milliseconds
        int minimumThinkingTime = 20; // Minimum thinking time in milliseconds
        
        // Evaluation options
        bool useNeuralNetwork = true;  // Use neural network evaluation
        float nnWeight = 0.7f;         // Weight for neural network vs traditional eval
        bool useTablebases = true;     // Use endgame tablebases
        
        // Debug options
        bool debug = false;           // Enable debug output
        bool showCurrLine = false;    // Show current search line
    };
    
    UCIEngine();
    ~UCIEngine();
    
    // Main UCI loop
    void run();
    void processCommand(const std::string& command);
    
    // UCI command handlers
    void handleUCI();
    void handleIsReady();
    void handleSetOption(const std::string& command);
    void handleNewGame();
    void handlePosition(const std::string& command);
    void handleGo(const std::string& command);
    void handleStop();
    void handleQuit();
    void handleDebug(const std::string& command);
    void handleRegister(const std::string& command);
    void handleInfo(const std::string& command);
    
    // Search result reporting
    void reportBestMove(const std::pair<int, int>& move, const std::pair<int, int>& ponderMove = {-1, -1});
    void reportInfo(int depth, int seldepth, int time, int nodes, int nps, 
                   const std::vector<std::pair<int, int>>& pv, int score, int hashfull);
    void reportInfo(const std::string& info);
    
private:
    Board board;
    UCIOptions options;
    std::unique_ptr<NeuralNetworkEvaluator> nnEvaluator;
    std::unique_ptr<EndgameTablebase> tablebase;
    std::unique_ptr<EnhancedOpeningBook> openingBook;
    std::unique_ptr<TimeManager> timeManager;
    
    // Search state
    bool isSearching;
    std::chrono::steady_clock::time_point searchStartTime;
    int searchTimeLimit;
    int searchDepthLimit;
    int searchNodeLimit;
    
    // Helper functions
    std::string moveToUCI(const std::pair<int, int>& move);
    std::pair<int, int> uciToMove(const std::string& uciMove);
    std::string boardToFEN(const Board& board);
    void parseFEN(const std::string& fen, Board& board);
    void parseMoves(const std::string& moves, Board& board);
    std::vector<std::pair<int, int>> getPrincipalVariation(const Board& board, int depth);
    
    // Search management
    void startSearch();
    void stopSearch();
    SearchResult performSearch(const Board& board, int depth, int timeLimit);
    
    // Option management
    void setHashSize(int size);
    void setThreads(int num);
    void setMultiPV(int num);
    void setPonder(bool enabled);
    void setOwnBook(bool enabled);
    void setMoveOverhead(int overhead);
    void setMinimumThinkingTime(int time);
    void setUseNeuralNetwork(bool enabled);
    void setNNWeight(float weight);
    void setUseTablebases(bool enabled);
    void setDebug(bool enabled);
    void setShowCurrLine(bool enabled);
};

// UCI move notation utilities
class UCINotation {
public:
    // Convert internal move representation to UCI notation
    static std::string moveToUCI(const std::pair<int, int>& move);
    
    // Convert UCI notation to internal move representation
    static std::pair<int, int> uciToMove(const std::string& uciMove);
    
    // Convert square index to algebraic notation
    static std::string squareToAlgebraic(int square);
    
    // Convert algebraic notation to square index
    static int algebraicToSquare(const std::string& algebraic);
    
    // Validate UCI move format
    static bool isValidUCIMove(const std::string& uciMove);
    
    // Get move type (normal, capture, promotion, castling, en passant)
    static std::string getMoveType(const Board& board, const std::pair<int, int>& move);
};

// UCI position parsing
class UCIPosition {
public:
    // Parse FEN string
    static void parseFEN(const std::string& fen, Board& board);
    
    // Parse move list
    static void parseMoves(const std::string& moves, Board& board);
    
    // Generate FEN string
    static std::string generateFEN(const Board& board);
    
    // Validate FEN string
    static bool isValidFEN(const std::string& fen);
    
    // Get position hash for caching
    static uint64_t getPositionHash(const Board& board);
};

// UCI search info formatting
class UCISearchInfo {
public:
    // Format search info for UCI output
    static std::string formatInfo(int depth, int seldepth, int time, int nodes, int nps,
                                 const std::vector<std::pair<int, int>>& pv, int score, int hashfull);
    
    // Format score for UCI output
    static std::string formatScore(int score, bool isMate = false);
    
    // Format principal variation for UCI output
    static std::string formatPV(const std::vector<std::pair<int, int>>& pv);
    
    // Format time for UCI output
    static std::string formatTime(int timeMs);
    
    // Format nodes per second
    static std::string formatNPS(int nodes, int timeMs);
};

#endif 