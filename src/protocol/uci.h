#pragma once

#include "../core/ChessBoard.h"
#include "../search/AdvancedSearch.h"
#include "../search/search.h"
#include "../utils/SearchThread.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
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
        std::string evalFile;
        bool useTablebases = true;
        std::string syzygyPath;
        bool debug = false;
        bool showCurrLine = false;
        int contempt = 0;
    };
    UCIEngine();
    ~UCIEngine();
    void run();
    void processCommand(std::string_view command);
    void handleUCI(std::string_view args = {});
    void handleIsReady(std::string_view args = {});
    void handleSetOption(std::string_view command);
    void handleNewGame(std::string_view args = {});
    void handlePosition(std::string_view command);
    void handleGo(std::string_view command);
    void handleStop(std::string_view args = {});
    void handlePonderHit(std::string_view args = {});
    void handleQuit(std::string_view args = {});
    void handleDebug(std::string_view command) const;
    void handleRegister(std::string_view command);
    void handleInfo(std::string_view command);
    void handleBookStats(std::string_view args = {});

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

    void searchThreadEntry(const SearchTask& task, std::stop_token stopToken);
    void joinSearchThread();

    Board board;
    std::vector<uint64_t> positionHistoryHashes;
    UCIOptions options;
    std::unique_ptr<EnhancedOpeningBook> openingBook;
    std::atomic_bool isSearching{false};
    std::atomic_bool isPondering{false};
    std::atomic_bool stopRequested{false};
    std::stop_source searchStopSource;
    std::stop_token activeSearchStopToken;
    std::chrono::steady_clock::time_point searchStartTime;
    int searchTimeLimit = 0;
    int searchDepthLimit = 0;
    SearchThread searchThread;
    SearchTask activeSearchTask{};
    SearchContext searchContext;
    static constexpr std::size_t kSearchThreadStackBytes = 8ULL * 1024ULL * 1024ULL;

    SearchResult performSearch(const Board& board, int depth, int timeLimit, int optimalTime = 0,
                               int maxTime = 0, std::stop_token stopToken = {});

    void setHashSize(int size);
    void setThreads(int num);
    void setMultiPV(int num);
    void setPonder(bool enabled);
    void setOwnBook(bool enabled);
    void setMoveOverhead(int overhead);
    void setMinimumThinkingTime(int time);
    void setUseNeuralNetwork(bool enabled);
    void setEvalFile(std::string_view path);
    void setUseTablebases(bool enabled);
    void setDebug(bool enabled);
    void setShowCurrLine(bool enabled);
};

class UCINotation {
public:
    static std::string moveToUCI(const Move& move);
    static std::optional<Move> uciToMove(std::string_view uciMove);
};

int runUCIEngine();
