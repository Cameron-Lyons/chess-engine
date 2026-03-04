#pragma once

#include "../ai/EndgameTablebase.h"
#include "../ai/NeuralNetwork.h"
#include "../core/ChessBoard.h"
#include "../search/AdvancedSearch.h"
#include "../search/search.h"

#include <atomic>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
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
        bool useNeuralNetwork = false;
        float nnWeight = 0.7F;
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
    void handleDebug(const std::string& command) const;
    void handleRegister(const std::string& command);
    void handleInfo(const std::string& command);
    void handleBookStats();

    void reportBestMove(const Move& move, const std::optional<Move>& ponderMove = std::nullopt);
    void reportInfo(int depth, int seldepth, int time, int nodes, int nps,
                    const std::vector<Move>& pv, int score, int hashfull);
    void reportInfo(const std::string& info);

private:
    struct SearchTask {
        int depth;
        int timeForMove;
        int optimalTime;
        int maxTime;
        Board boardSnapshot;
    };

    void searchThreadEntry(const std::stop_token& stopToken, const SearchTask& task);

    Board board;
    UCIOptions options;
    std::unique_ptr<NeuralNetworkEvaluator> nnEvaluator;
    std::unique_ptr<EndgameTablebase> tablebase;
    std::unique_ptr<EnhancedOpeningBook> openingBook;
    std::unique_ptr<TimeManager> timeManager;
    std::atomic_bool isSearching{false};
    std::atomic_bool isPondering{false};
    std::chrono::steady_clock::time_point searchStartTime;
    int searchTimeLimit = 0;
    int searchDepthLimit = 0;
    std::jthread searchThread;
    SearchContext searchContext;

    SearchResult performSearch(const Board& board, int depth, int timeLimit, int optimalTime = 0,
                               int maxTime = 0);

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
    static std::string moveToUCI(const Move& move);
    static std::optional<Move> uciToMove(const std::string& uciMove);
};

int runUCIEngine();
