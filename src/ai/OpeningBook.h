#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "../core/ChessBoard.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>

// Enhanced opening book system
class OpeningBook {
public:
    // Book entry with statistics
    struct BookEntry {
        std::string move;
        int games;
        int wins;
        int draws;
        int losses;
        float averageRating;
        float score; // Win percentage
        int depth; // Analysis depth
        float evaluation; // Position evaluation after move
        
        BookEntry() : games(0), wins(0), draws(0), losses(0), 
                     averageRating(0.0f), score(0.5f), depth(0), evaluation(0.0f) {}
        
        BookEntry(const std::string& m, int g, int w, int d, int l, float ar, float s, int dep, float eval)
            : move(m), games(g), wins(w), draws(d), losses(l), 
              averageRating(ar), score(s), depth(dep), evaluation(eval) {}
        
        // Calculate win percentage
        float getWinPercentage() const;
        
        // Calculate draw percentage
        float getDrawPercentage() const;
        
        // Calculate loss percentage
        float getLossPercentage() const;
        
        // Get expected score (0.0 = loss, 0.5 = draw, 1.0 = win)
        float getExpectedScore() const;
        
        // Check if move is reliable (enough games)
        bool isReliable(int minGames = 10) const;
        
        // Check if move is good (high win percentage)
        bool isGood(float threshold = 0.55f) const;
    };
    
    // Position entry with multiple moves
    struct PositionEntry {
        std::string fen;
        std::vector<BookEntry> moves;
        int totalGames;
        float averageRating;
        std::string bestMove;
        float bestScore;
        
        PositionEntry() : totalGames(0), averageRating(0.0f), bestScore(0.5f) {}
        
        // Add a move to this position
        void addMove(const BookEntry& entry);
        
        // Get the best move based on criteria
        std::string getBestMove(bool preferReliable = true, float reliabilityThreshold = 0.55f) const;
        
        // Get a random move with weighted selection
        std::string getRandomMove(float temperature = 1.0f) const;
        
        // Get moves above a certain score threshold
        std::vector<BookEntry> getGoodMoves(float threshold = 0.45f) const;
        
        // Check if position is in book
        bool hasMoves() const { return !moves.empty(); }
        
        // Get number of moves
        size_t getMoveCount() const { return moves.size(); }
    };
    
    // Book statistics
    struct BookStats {
        size_t totalPositions;
        size_t totalMoves;
        size_t totalGames;
        float averageRating;
        int maxDepth;
        std::chrono::steady_clock::time_point lastUpdated;
        
        BookStats() : totalPositions(0), totalMoves(0), totalGames(0), 
                     averageRating(0.0f), maxDepth(0) {}
        
        void update();
        void print() const;
    };
    
    // Opening book configuration
    struct BookConfig {
        bool useBook;
        bool preferReliable;
        bool useEvaluation;
        bool useStatistics;
        float reliabilityThreshold;
        float temperature;
        int minGames;
        int maxDepth;
        float evaluationWeight;
        float statisticsWeight;
        
        BookConfig() : useBook(true), preferReliable(true), useEvaluation(true), 
                      useStatistics(true), reliabilityThreshold(0.55f), temperature(1.0f),
                      minGames(10), maxDepth(20), evaluationWeight(0.3f), statisticsWeight(0.7f) {}
    };
    
    OpeningBook(const BookConfig& config = BookConfig{});
    ~OpeningBook() = default;
    
    // Main functions
    std::string getMove(const Board& board);
    std::string getBestMove(const Board& board);
    std::string getRandomMove(const Board& board);
    std::vector<BookEntry> getAllMoves(const Board& board);
    
    // Book management
    void loadBook(const std::string& filename);
    void saveBook(const std::string& filename);
    void addPosition(const std::string& fen, const BookEntry& entry);
    void updatePosition(const std::string& fen, const std::string& move, int result, float rating);
    void removePosition(const std::string& fen);
    void clear();
    
    // Book analysis
    void analyzeBook();
    void validateBook();
    void optimizeBook();
    BookStats getStats() const;
    
    // Book generation
    void generateFromGames(const std::string& pgnFile);
    void generateFromEngine(const std::string& enginePath, int depth, int timeLimit);
    void mergeBooks(const std::vector<std::string>& bookFiles, const std::string& outputFile);
    
    // Configuration
    void setConfig(const BookConfig& config);
    const BookConfig& getConfig() const;
    
    // Utility functions
    bool isInBook(const Board& board) const;
    bool isInBook(const std::string& fen) const;
    int getBookDepth(const Board& board) const;
    float getBookScore(const Board& board) const;
    
private:
    BookConfig config;
    std::unordered_map<std::string, PositionEntry> positions;
    BookStats stats;
    std::mt19937 rng;
    
    // Internal helper functions
    std::string boardToFEN(const Board& board) const;
    std::string normalizeFEN(const std::string& fen) const;
    float calculateMoveScore(const BookEntry& entry) const;
    std::string selectMoveByCriteria(const PositionEntry& entry) const;
    std::string selectMoveByTemperature(const PositionEntry& entry, float temperature) const;
    
    // Book analysis helpers
    void calculatePositionStatistics();
    void removeUnreliableMoves();
    void sortMovesByScore();
    void validateMoves();
    
    // File I/O helpers
    void parseBookFile(const std::string& filename);
    void writeBookFile(const std::string& filename) const;
    void parsePGNFile(const std::string& filename);
    void parseEngineAnalysis(const std::string& analysis);
    
    // Move generation helpers
    std::vector<std::string> generateLegalMoves(const Board& board) const;
    bool isValidMove(const Board& board, const std::string& move) const;
    std::string convertMoveToAlgebraic(const Board& board, int from, int to) const;
    std::pair<int, int> convertAlgebraicToMove(const Board& board, const std::string& move) const;
};

// Global opening book instance
extern std::unique_ptr<OpeningBook> g_openingBook;

// Global functions
void initializeOpeningBook(const OpeningBook::BookConfig& config = OpeningBook::BookConfig{});
OpeningBook* getOpeningBook();
std::string getBookMove(const Board& board);
bool isBookMove(const Board& board);

// Book file formats
namespace BookFormats {
    enum Format {
        BIN,    // Binary format
        PGN,    // PGN format
        JSON,   // JSON format
        CSV     // CSV format
    };
    
    void saveBook(const std::string& filename, const OpeningBook& book, Format format);
    void loadBook(const std::string& filename, OpeningBook& book, Format format);
}

#endif // OPENING_BOOK_H 