#ifndef UCI_H
#define UCI_H

#include "../ai/EndgameTablebase.h"
#include "../ai/NeuralNetwork.h"
#include "../core/ChessBoard.h"
#include "../search/AdvancedSearch.h"
#include "../search/search.h"
#include <memory>
#include <string>
#include <vector>

class UCIEngine {
public:
    struct UCIOptions {

        int hashSize = 32;
        int threads = 1;
        int multiPV = 1;
        bool ponder = false;
        bool ownBook = true;

        int moveOverhead = 10;
        int minimumThinkingTime = 20;

        bool useNeuralNetwork = true;
        float nnWeight = 0.7f;
        bool useTablebases = true;
        std::string syzygyPath;

        bool debug = false;
        bool showCurrLine = false;
        int contempt = 0;
    };

    UCIEngine();
    ~UCIEngine();

    void run();
    void processCommand(const std::string& command);

    void handleUCI();
    void handleIsReady();
    void handleSetOption(const std::string& command);
    void handleNewGame();
    void handlePosition(const std::string& command);
    void handleGo(const std::string& command);
    void handleStop();
    void handlePonderHit();
    void handleQuit();
    void handleDebug(const std::string& command);
    void handleRegister(const std::string& command);
    void handleInfo(const std::string& command);
    void handleBookStats();

    void reportBestMove(const std::pair<int, int>& move,
                        const std::pair<int, int>& ponderMove = {-1, -1});
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

    bool isSearching;
    bool isPondering;
    std::chrono::steady_clock::time_point searchStartTime;
    int searchTimeLimit;
    int searchDepthLimit;
    int searchNodeLimit;
    std::thread searchThread;
    ParallelSearchContext* activeContext = nullptr;

    std::string moveToUCI(const std::pair<int, int>& move);
    std::pair<int, int> uciToMove(const std::string& uciMove);
    std::string boardToFEN(const Board& board);
    void parseFEN(const std::string& fen, Board& board);
    void parseMoves(const std::string& moves, Board& board);
    std::vector<std::pair<int, int>> getPrincipalVariation(const Board& board, int depth);

    void startSearch();
    void stopSearch();
    SearchResult performSearch(const Board& board, int depth, int timeLimit,
                               int optimalTime = 0, int maxTime = 0);

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

class UCINotation {
public:
    static std::string moveToUCI(const std::pair<int, int>& move);

    static std::pair<int, int> uciToMove(const std::string& uciMove);

    static std::string squareToAlgebraic(int square);

    static int algebraicToSquare(const std::string& algebraic);

    static bool isValidUCIMove(const std::string& uciMove);

    static std::string getMoveType(const Board& board, const std::pair<int, int>& move);
};

class UCIPosition {
public:
    static void parseFEN(const std::string& fen, Board& board);

    static void parseMoves(const std::string& moves, Board& board);

    static std::string generateFEN(const Board& board);

    static bool isValidFEN(const std::string& fen);

    static uint64_t getPositionHash(const Board& board);
};

class UCISearchInfo {
public:
    static std::string formatInfo(int depth, int seldepth, int time, int nodes, int nps,
                                  const std::vector<std::pair<int, int>>& pv, int score,
                                  int hashfull);

    static std::string formatScore(int score, bool isMate = false);

    static std::string formatPV(const std::vector<std::pair<int, int>>& pv);

    static std::string formatTime(int timeMs);

    static std::string formatNPS(int nodes, int timeMs);
};

#endif