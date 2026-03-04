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

    static bool multiCutPruning(Board& board, int depth, int alpha, int beta, int r);

    static bool historyPruning(const Board& board, int depth, const Move& move,
                               const ThreadSafeHistory& history);

    static bool lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck);
    static bool recaptureExtension(const Board& board, const Move& move, int depth);
    static bool checkExtension(const Board& board, const Move& move, int depth);
    static bool pawnPushExtension(const Board& board, const Move& move, int depth);
    static bool passedPawnExtension(const Board& board, const Move& move, int depth);
};

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
    GamePhase getGamePhase(const Board& board) const;
};

class EnhancedOpeningBook {
public:
    struct BookEntry {
        Move move;
        int weight;
        int games;
        float winRate;
        int averageRating;
    };
    EnhancedOpeningBook(const std::string& bookPath = "books/opening_book.bin");
    Move getBestMove(const Board& board, bool randomize = false);
    bool isInBook(const Board& board);
    void loadBook(const std::string& path);

    struct BookStats {
        size_t totalPositions;
        size_t totalMoves;
        size_t totalGames;
        float averageWinRate;
        float averageRating;
    };
    BookStats getStats() const;

private:
    std::unordered_map<std::string, std::vector<BookEntry>> book;
    std::string bookPath;
    std::string boardToKey(const Board& board);
    Move parseMove(const std::string& move);
};
