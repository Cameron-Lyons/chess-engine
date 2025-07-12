#ifndef ENDGAME_TABLEBASE_H
#define ENDGAME_TABLEBASE_H

#include "../core/ChessBoard.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>
#include <memory>

// Endgame tablebase interface for perfect endgame play
class EndgameTablebase {
public:
    enum class TablebaseType {
        SYZYGY,     // Syzygy tablebases (recommended)
        NALIMOV,    // Nalimov tablebases (legacy)
        CUSTOM      // Custom tablebase format
    };
    
    struct TablebaseResult {
        int distanceToMate;  // Positive = winning, negative = losing, 0 = draw
        std::vector<std::pair<int, int>> bestMoves;  // All best moves
        bool isExact;        // True if exact result, false if lower bound
    };
    
    EndgameTablebase(const std::string& tablebasePath = "tablebases/");
    ~EndgameTablebase();
    
    // Check if position is in tablebase
    bool isInTablebase(const Board& board);
    
    // Get tablebase result for position
    bool probe(const Board& board, TablebaseResult& result);
    
    // Get best move from tablebase
    bool getBestMove(const Board& board, std::pair<int, int>& bestMove);
    
    // Check if position is winning/losing/draw
    bool isWinning(const Board& board);
    bool isLosing(const Board& board);
    bool isDraw(const Board& board);
    
    // Load tablebases for specific piece combinations
    void loadTablebase(const std::string& pieceCombination);
    
    // Get available tablebases
    std::vector<std::string> getAvailableTablebases();
    
    // Set tablebase type
    void setTablebaseType(TablebaseType type);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Helper functions
    std::string boardToTablebaseKey(const Board& board);
    bool isValidEndgamePosition(const Board& board);
    int countPieces(const Board& board);
};

// Endgame knowledge for positions not in tablebases
class EndgameKnowledge {
public:
    // KPK (King and Pawn vs King) evaluation
    static int evaluateKPK(const Board& board);
    
    // KRK (King and Rook vs King) evaluation
    static int evaluateKRK(const Board& board);
    
    // KQK (King and Queen vs King) evaluation
    static int evaluateKQK(const Board& board);
    
    // KBNK (King, Bishop, Knight vs King) evaluation
    static int evaluateKBNK(const Board& board);
    
    // KBBK (King and two Bishops vs King) evaluation
    static int evaluateKBBK(const Board& board);
    
    // General endgame evaluation
    static int evaluateEndgame(const Board& board);
    
private:
    static int getKingDistance(int king1, int king2);
    static bool isPawnPromoting(const Board& board, int pawnSquare);
    static int getPawnAdvancement(const Board& board, int pawnSquare);
};

#endif // ENDGAME_TABLEBASE_H 