#pragma once

#include "../core/ChessBoard.h"
#include "search.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class AdvancedSearch {
public:
    static bool futilityPruning(const Board& board, int depth, int alpha, int beta, int staticEval);

    static bool staticNullMovePruning(const Board& board, int depth, int alpha, int beta,
                                      int staticEval);

    static bool nullMovePruning(const Board& board, int depth, int alpha, int beta);

    static bool lateMoveReduction(const Board& board, int depth, int moveNumber, int alpha,
                                  int beta);

    static bool multiCutPruning(Board& board, int depth, int alpha, int beta, int r);

    static std::pair<int, int> internalIterativeDeepening(Board& board, int depth, int alpha,
                                                          int beta);

    static bool singularExtension(Board& board, int depth, const std::pair<int, int>& move,
                                  int alpha, int beta);

    static bool historyPruning(const Board& board, int depth, const std::pair<int, int>& move,
                               const ThreadSafeHistory& history);

    static bool lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck);

    static bool recaptureExtension(const Board& board, const std::pair<int, int>& move, int depth);

    static bool checkExtension(const Board& board, const std::pair<int, int>& move, int depth);

    static bool pawnPushExtension(const Board& board, const std::pair<int, int>& move, int depth);

    static bool passedPawnExtension(const Board& board, const std::pair<int, int>& move, int depth);
};

namespace EnhancedMoveOrdering {
struct MoveScore {
    std::pair<int, int> move;
    int score;
    MoveScore(const std::pair<int, int>& m, int s) : move(m), score(s) {}
    bool operator<(const MoveScore& other) const {
        return score > other.score;
    }
};

std::vector<MoveScore> scoreMoves(const Board& board, const std::vector<std::pair<int, int>>& moves,
                                  const ThreadSafeHistory& history, const KillerMoves& killers,
                                  int ply, const std::pair<int, int>& hashMove = {-1, -1});

int getSEEScore(const Board& board, const std::pair<int, int>& move);

int getThreatScore(const Board& board, const std::pair<int, int>& move);

int getMobilityScore(const Board& board, const std::pair<int, int>& move);

int getPositionalScore(const Board& board, const std::pair<int, int>& move);
} // namespace EnhancedMoveOrdering

enum class GamePhase : std::uint8_t { OPENING, MIDDLEGAME, ENDGAME };

class TimeManager {
public:
    struct TimeControl {
        int baseTime;
        int increment;
        int movesToGo;
        bool isInfinite;
    };

    TimeManager(const TimeControl& tc);

    int allocateTime(Board& board, int depth, int nodes, bool isInCheck);

    bool shouldStop(int elapsedTime, int allocatedTime, int depth, int nodes);

    void updateGameProgress(int moveNumber, int totalMoves);

    bool isEmergencyTime(int remainingTime, int allocatedTime);

    GamePhase getGamePhase(const Board& board) const;

private:
    TimeControl timeControl;
    int moveNumber;
    int totalMoves;

    int calculateBaseTime();
    int calculateIncrement();
    double getTimeFactor(int depth, int nodes);
    double getPhaseTimeFactor(GamePhase phase) const;
};

class EnhancedOpeningBook {
public:
    struct BookEntry {
        std::pair<int, int> move;
        int weight;
        int games;
        float winRate;
        int averageRating;
    };

    EnhancedOpeningBook(const std::string& bookPath = "books/opening_book.bin");

    std::vector<BookEntry> getBookMoves(const Board& board);

    std::pair<int, int> getBestMove(const Board& board, bool randomize = false);

    bool isInBook(const Board& board);

    void addMove(const Board& board, const BookEntry& entry);

    void saveBook(const std::string& path);

    void loadBook(const std::string& path);

    struct BookStats {
        size_t totalPositions;
        size_t totalMoves;
        size_t totalGames;
        float averageWinRate;
        float averageRating;
    };

    BookStats getStats() const;

    void analyzeBook();

private:
    std::unordered_map<std::string, std::vector<BookEntry>> book;
    std::string bookPath;

    std::string boardToKey(const Board& board);
    void normalizeWeights(std::vector<BookEntry>& entries);
    std::pair<int, int> parseMove(const std::string& move);
};