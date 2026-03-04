#include "uci.h"
#include "../ai/SyzygyTablebase.h"
#include "../core/BitboardMoves.h"
#include "../evaluation/Evaluation.h"
#include "../search/search.h"
#include "../utils/TunableParams.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "ai/EndgameTablebase.h"
#include "ai/NeuralNetwork.h"
#include "evaluation/NNUE.h"
#include "search/AdvancedSearch.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
constexpr int kDefaultBaseTimeMs = 300000;
constexpr int kDefaultIncrementMs = 0;
constexpr int kDefaultMovesToGo = 0;
constexpr int kNameTokenLength = 5;
constexpr int kValueTokenLength = 7;
constexpr int kFenFieldCount = 6;
constexpr int kInvalidSquare = -1;
constexpr int kUnsetTimeValue = -1;
constexpr int kDefaultSearchDepth = 64;
constexpr int kDefaultMoveTimeMs = 5000;
constexpr int kInfiniteTimeMs = 999999999;
constexpr int kEstimatedMovesToGo = 30;
constexpr double kIncrementUsageFactor = 0.75;
constexpr double kMaxTimeFraction = 0.5;
constexpr int kMaxTimeMultiplier = 5;
constexpr int kNpsMillisScale = 1000;
constexpr int kMateScoreThreshold = 30000;
constexpr int kMatePlyDivisor = 2;
constexpr int kMatePlyOffset = 1;
constexpr int kBoardDimension = 8;
constexpr int kUciMoveLength = 4;
constexpr char kMinFileChar = 'a';
constexpr char kMinRankChar = '1';
constexpr int kTablebaseMateScore = 30000;
constexpr const char* kStartingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
} // namespace

UCIEngine::UCIEngine() {

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
    board = Board();
    board.InitializeFromFEN(kStartingFen);
    nnEvaluator = std::make_unique<NeuralNetworkEvaluator>();
    tablebase = std::make_unique<EndgameTablebase>();
    openingBook = std::make_unique<EnhancedOpeningBook>();
    TimeManager::TimeControl tc;
    tc.baseTime = kDefaultBaseTimeMs;
    tc.increment = kDefaultIncrementMs;
    tc.movesToGo = kDefaultMovesToGo;
    tc.isInfinite = false;
    timeManager = std::make_unique<TimeManager>(tc);
    searchContext.threads = options.threads;
    searchContext.hashSizeMb = options.hashSize;
    setUseNeuralNetwork(options.useNeuralNetwork);
}

UCIEngine::~UCIEngine() {
    handleStop();
}

void UCIEngine::searchThreadEntry(const std::stop_token& stopToken, const SearchTask& task) {
    try {
        SearchResult result = performSearch(task.boardSnapshot, task.depth, task.timeForMove,
                                            task.optimalTime, task.maxTime);

        if (!stopToken.stop_requested() && isSearching.load()) {
            std::optional<Move> ponderMove;
            reportBestMove(result.bestMove, ponderMove);
            int nps = result.timeMs > 0 ? result.nodes * kNpsMillisScale / result.timeMs : 0;
            reportInfo(result.depth, result.depth, result.timeMs, result.nodes, nps, {},
                       result.score, 0);
        }
    } catch (const std::exception& e) {
        std::cout << "info string Search error: " << e.what() << '\n';
    }

    isSearching.store(false);
    isPondering.store(false);
}

void UCIEngine::run() {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (!input.empty()) {
            processCommand(input);
        }
    }
}

void UCIEngine::processCommand(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "uci") {
        handleUCI();
    } else if (cmd == "isready") {
        handleIsReady();
    } else if (cmd == "setoption") {
        handleSetOption(command);
    } else if (cmd == "ucinewgame") {
        handleNewGame();
    } else if (cmd == "position") {
        handlePosition(command);
    } else if (cmd == "go") {
        handleGo(command);
    } else if (cmd == "stop") {
        handleStop();
    } else if (cmd == "ponderhit") {
        handlePonderHit();
    } else if (cmd == "quit") {
        handleQuit();
    } else if (cmd == "debug") {
        handleDebug(command);
    } else if (cmd == "register") {
        handleRegister(command);
    } else if (cmd == "info") {
        handleInfo(command);
    } else if (cmd == "bookstats") {
        handleBookStats();
    } else {

        std::cout << "info string Unknown command: " << cmd << '\n';
    }
}

void UCIEngine::handleUCI() {
    std::cout << "id name ModernChess v2.0" << '\n';
    std::cout << "id author Chess Engine Team" << '\n';
    std::cout << "option name Hash type spin default 32 min 1 max 1024" << '\n';
    std::cout << "option name Threads type spin default 1 min 1 max 16" << '\n';
    std::cout << "option name MultiPV type spin default 1 min 1 max 10" << '\n';
    std::cout << "option name Ponder type check default false" << '\n';
    std::cout << "option name OwnBook type check default true" << '\n';
    std::cout << "option name Move Overhead type spin default 10 min 0 max 5000" << '\n';
    std::cout << "option name Minimum Thinking Time type spin default 20 min 0 max 5000" << '\n';
    std::cout << "option name Use Neural Network type check default false" << '\n';
    std::cout << "option name Neural Network Weight type spin default 70 min 0 max 100" << '\n';
    std::cout << "option name Use Tablebases type check default true" << '\n';
    std::cout << "option name SyzygyPath type string default " << '\n';
    std::cout << "option name Debug type check default false" << '\n';
    std::cout << "option name Show Current Line type check default false" << '\n';
    std::cout << "option name Contempt type spin default 0 min -100 max 100" << '\n';

    for (const auto& p : TunableRegistry::instance().all()) {
        std::cout << "option name " << p.name << " type spin default " << p.defaultValue << " min "
                  << p.minValue << " max " << p.maxValue << '\n';
    }

    std::cout << "uciok" << '\n';
}

void UCIEngine::handleIsReady() {
    std::cout << "readyok" << '\n';
}

void UCIEngine::handleSetOption(const std::string& command) {
    std::string name;
    std::string value;
    auto namePos = command.find("name ");
    auto valuePos = command.find(" value ");
    if (namePos == std::string::npos) {
        return;
    }
    namePos += kNameTokenLength;
    if (valuePos != std::string::npos) {
        name = command.substr(namePos, valuePos - namePos);
        value = command.substr(valuePos + kValueTokenLength);
    } else {
        name = command.substr(namePos);
    }

    if (name == "Hash") {
        setHashSize(std::stoi(value));
    } else if (name == "Threads") {
        setThreads(std::stoi(value));
    } else if (name == "MultiPV") {
        setMultiPV(std::stoi(value));
    } else if (name == "Ponder") {
        setPonder(value == "true");
    } else if (name == "OwnBook") {
        setOwnBook(value == "true");
    } else if (name == "Move Overhead") {
        setMoveOverhead(std::stoi(value));
    } else if (name == "Minimum Thinking Time") {
        setMinimumThinkingTime(std::stoi(value));
    } else if (name == "Use Neural Network") {
        setUseNeuralNetwork(value == "true");
    } else if (name == "Neural Network Weight") {
        setNNWeight(std::stof(value) / 100.0F);
    } else if (name == "Use Tablebases") {
        setUseTablebases(value == "true");
    } else if (name == "SyzygyPath") {
        options.syzygyPath = value;
        if (!value.empty()) {
            Syzygy::init(value);
        }
    } else if (name == "Debug") {
        setDebug(value == "true");
    } else if (name == "Show Current Line") {
        setShowCurrLine(value == "true");
    } else if (name == "Contempt") {
        options.contempt = std::stoi(value);
    } else {
        TunableRegistry::instance().set(name, std::stoi(value));
    }
}

void UCIEngine::handleNewGame() {

    board = Board();
    board.InitializeFromFEN(kStartingFen);
    std::cout << "info string New game started" << '\n';
}

void UCIEngine::handlePosition(const std::string& command) {
    std::istringstream iss(command);
    std::string word;
    iss >> word;

    if (!(iss >> word)) {
        std::cout << "info string Error: Invalid position command" << '\n';
        return;
    }

    if (word == "startpos") {

        board = Board();
        board.InitializeFromFEN(kStartingFen);
    } else if (word == "fen") {

        std::string fen;
        for (int i = 0; i < kFenFieldCount; i++) {
            std::string part;
            if (iss >> part) {
                if (i > 0) {
                    fen += " ";
                }
                fen += part;
            }
        }

        if (!fen.empty()) {

            board = Board();
            if (auto parsed = board.fromFEN(fen); parsed.has_value()) {
                std::cout << "info string FEN position set: " << fen << '\n';
            } else {
                std::cout << "info string Error: Invalid FEN string" << '\n';
                return;
            }
        } else {
            std::cout << "info string Error: Invalid FEN string" << '\n';
            return;
        }
    } else {
        std::cout << "info string Error: Expected 'startpos' or 'fen'" << '\n';
        return;
    }

    if (iss >> word && word == "moves") {
        std::string move;
        while (iss >> move) {
            if (const auto movePos = UCINotation::uciToMove(move)) {

                std::vector<Move> validMoves = GetAllMoves(board, board.turn);
                bool isValid = false;
                for (const auto& validMove : validMoves) {
                    if (validMove.first == movePos->first && validMove.second == movePos->second) {
                        isValid = true;
                        break;
                    }
                }

                if (isValid) {
                    if (!applySearchMove(board, movePos->first, movePos->second)) {
                        continue;
                    }
                    board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                        : ChessPieceColor::WHITE;
                    board.updateBitboards();
                } else {
                    std::cout << "info string Warning: Invalid move " << move << '\n';
                }
            } else {
                std::cout << "info string Warning: Could not parse move " << move << '\n';
            }
        }
    }
}

void UCIEngine::handleGo(const std::string& command) {
    if (isSearching.load()) {
        std::cout << "info string Search already in progress" << '\n';
        return;
    }

    std::istringstream iss(command);
    std::string word;
    iss >> word;
    int wtime = kUnsetTimeValue;
    int btime = kUnsetTimeValue;
    int winc = 0;
    int binc = 0;
    int movestogo = kUnsetTimeValue;
    int searchDepth = kDefaultSearchDepth;
    int movetime = kUnsetTimeValue;
    bool isPonder = false;
    bool isInfinite = false;

    while (iss >> word) {
        if (word == "wtime") {
            iss >> wtime;
        } else if (word == "btime") {
            iss >> btime;
        } else if (word == "winc") {
            iss >> winc;
        } else if (word == "binc") {
            iss >> binc;
        } else if (word == "movestogo") {
            iss >> movestogo;
        } else if (word == "depth") {
            iss >> searchDepth;
        } else if (word == "movetime") {
            iss >> movetime;
        } else if (word == "ponder") {
            isPonder = true;
        } else if (word == "infinite") {
            isInfinite = true;
        }
    }

    int timeForMove = kDefaultMoveTimeMs;
    int optimalTime = kDefaultMoveTimeMs;
    int maxTime = kDefaultMoveTimeMs;

    if (isPonder || isInfinite) {
        timeForMove = kInfiniteTimeMs;
        optimalTime = kInfiniteTimeMs;
        maxTime = kInfiniteTimeMs;
    } else if (movetime > 0) {
        timeForMove = movetime;
        optimalTime = movetime;
        maxTime = movetime;
    } else if (wtime > 0 || btime > 0) {
        int currentTime{(board.turn == ChessPieceColor::WHITE) ? wtime : btime};
        int increment{(board.turn == ChessPieceColor::WHITE) ? winc : binc};

        if (currentTime > 0) {
            int baseTime = 0;
            if (movestogo > 0) {
                baseTime = currentTime / movestogo;
            } else {
                baseTime = currentTime / kEstimatedMovesToGo;
            }
            optimalTime = baseTime + static_cast<int>(increment * kIncrementUsageFactor) -
                          options.moveOverhead;
            maxTime = std::min(static_cast<int>(currentTime * kMaxTimeFraction),
                               optimalTime * kMaxTimeMultiplier) -
                      options.moveOverhead;
            optimalTime = std::max(optimalTime, options.minimumThinkingTime);
            maxTime = std::max(maxTime, optimalTime);
            timeForMove = optimalTime;
        }
    }

    isSearching.store(true);
    isPondering.store(isPonder);
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = timeForMove;
    searchDepthLimit = searchDepth;

    if (searchThread.joinable()) {
        searchThread.request_stop();
        searchThread.join();
    }

    try {
        searchThread =
            std::jthread([this](const std::stop_token& stopToken,
                                const SearchTask& task) { searchThreadEntry(stopToken, task); },
                         SearchTask{searchDepth, timeForMove, optimalTime, maxTime, board});
    } catch (const std::system_error&) {
        isSearching.store(false);
        isPondering.store(false);
        std::cout << "info string Search thread creation failed" << '\n';
        return;
    }
}

void UCIEngine::handleStop() {
    isSearching.store(false);
    isPondering.store(false);
    if (searchThread.joinable()) {
        searchThread.request_stop();
        searchThread.join();
    }
}

void UCIEngine::handlePonderHit() {
    isPondering.store(false);
}

void UCIEngine::handleQuit() {
    isSearching.store(false);
    isPondering.store(false);
    if (searchThread.joinable()) {
        searchThread.request_stop();
        searchThread.join();
    }
    std::exit(0);
}

void UCIEngine::handleDebug(const std::string& command) const {
    (void)command;
    std::cout << "info string Debug mode: " << (options.debug ? "enabled" : "disabled") << '\n';
}

void UCIEngine::handleRegister(const std::string& command) {
    (void)command;
    std::cout << "info string Registration not required" << '\n';
}

void UCIEngine::handleInfo(const std::string& command) {
    (void)command;
    std::cout << "info string Info command received" << '\n';
}

void UCIEngine::handleBookStats() {
    if (!openingBook) {
        std::cout << "info string Opening book not available" << '\n';
        return;
    }

    EnhancedOpeningBook::BookStats stats = openingBook->getStats();
    std::cout << "info string Book Statistics:" << '\n';
    std::cout << "info string   Positions: " << stats.totalPositions << '\n';
    std::cout << "info string   Moves: " << stats.totalMoves << '\n';
    std::cout << "info string   Total Games: " << stats.totalGames << '\n';
    std::cout << "info string   Average Win Rate: " << stats.averageWinRate << '\n';
    std::cout << "info string   Average Rating: " << stats.averageRating << '\n';
}

void UCIEngine::reportBestMove(const Move& move, const std::optional<Move>& ponderMove) {
    std::cout << "bestmove " << UCINotation::moveToUCI(move)
              << (ponderMove ? " ponder " + UCINotation::moveToUCI(*ponderMove) : "") << '\n';
}

void UCIEngine::reportInfo(int depth, int seldepth, int time, int nodes, int nps,
                           const std::vector<Move>& pv, int score, int hashfull) {
    std::cout << "info depth " << depth;
    if (seldepth > 0) {
        std::cout << " seldepth " << seldepth;
    }
    if (time > 0) {
        std::cout << " time " << time;
    }
    if (nodes > 0) {
        std::cout << " nodes " << nodes;
    }
    if (nps > 0) {
        std::cout << " nps " << nps;
    }
    if (hashfull > 0) {
        std::cout << " hashfull " << hashfull;
    }
    if (score != 0) {
        std::cout << " score ";
        if (score > kMateScoreThreshold) {
            std::cout << "mate "
                      << (kMateScoreThreshold - score + kMatePlyOffset) / kMatePlyDivisor;
        } else if (score < -kMateScoreThreshold) {
            std::cout << "mate "
                      << -(kMateScoreThreshold + score + kMatePlyOffset) / kMatePlyDivisor;
        } else {
            std::cout << "cp " << score;
        }
    }
    if (!pv.empty()) {
        std::cout << " pv";
        for (const auto& move : pv) {
            std::cout << " " << UCINotation::moveToUCI(move);
        }
    }
    std::cout << '\n';
}

void UCIEngine::reportInfo(const std::string& info) {
    std::cout << "info string " << info << '\n';
}

SearchResult UCIEngine::performSearch(const Board& board, int depth, int timeLimit, int optimalTime,
                                      int maxTime) {
    SearchResult result;

    if (options.useTablebases && tablebase) {
        EndgameTablebase::TablebaseResult tbResult;
        if (tablebase->probe(board, tbResult)) {
            if (!tbResult.bestMoves.empty()) {
                result.bestMove = tbResult.bestMoves[0];
                if (tbResult.isExact) {
                    result.score =
                        (tbResult.distanceToMate > 0) ? kTablebaseMateScore : -kTablebaseMateScore;
                } else {
                    result.score = 0;
                }
                result.depth = 0;
                result.nodes = 0;
                result.timeMs = 0;
                return result;
            }
        }
    }

    if (options.ownBook && openingBook) {
        if (openingBook->isInBook(board)) {
            Move bookMove = openingBook->getBestMove(board, false);
            if (bookMove.first != kInvalidSquare && bookMove.second != kInvalidSquare) {
                result.bestMove = bookMove;
                result.score = 0;
                result.depth = 0;
                result.nodes = 0;
                result.timeMs = 0;
                return result;
            }
        }
    }

    Board searchBoard = board;

    SearchConfig config;
    config.maxDepth = depth;
    config.timeLimitMs = timeLimit;
    config.contempt = options.contempt;
    config.multiPV = options.multiPV;
    config.optimalTimeMs = optimalTime;
    config.maxTimeMs = maxTime;
    searchContext.threads = options.threads;
    searchContext.hashSizeMb = options.hashSize;
    result = iterativeDeepeningParallel(searchBoard, config, searchContext);

    return result;
}

void UCIEngine::setHashSize(int size) {
    options.hashSize = size;
    searchContext.hashSizeMb = size;
}

void UCIEngine::setThreads(int num) {
    options.threads = std::clamp(num, 1, 16);
    searchContext.threads = options.threads;
}

void UCIEngine::setMultiPV(int num) {
    options.multiPV = num;
}

void UCIEngine::setPonder(bool enabled) {
    options.ponder = enabled;
}

void UCIEngine::setOwnBook(bool enabled) {
    options.ownBook = enabled;
}

void UCIEngine::setMoveOverhead(int overhead) {
    options.moveOverhead = overhead;
}

void UCIEngine::setMinimumThinkingTime(int time) {
    options.minimumThinkingTime = time;
}

void UCIEngine::setUseNeuralNetwork(bool enabled) {
    options.useNeuralNetwork = enabled;
    if (enabled) {
        if (NNUE::globalEvaluator) {
            setNNUEEnabled(true);
        } else {
            setNNUEEnabled(false);
            std::cout << "info string NNUE requested but no model loaded; using classical eval\n";
        }
    } else {
        setNNUEEnabled(false);
    }
}

void UCIEngine::setNNWeight(float weight) {
    options.nnWeight = weight;
}

void UCIEngine::setUseTablebases(bool enabled) {
    options.useTablebases = enabled;
}

void UCIEngine::setDebug(bool enabled) {
    options.debug = enabled;
}

void UCIEngine::setShowCurrLine(bool enabled) {
    options.showCurrLine = enabled;
}

std::string UCINotation::moveToUCI(const Move& move) {
    if (move.first == kInvalidSquare || move.second == kInvalidSquare) {
        return "0000";
    }

    int fromFile = move.first % kBoardDimension;
    int fromRank = move.first / kBoardDimension;
    int toFile = move.second % kBoardDimension;
    int toRank = move.second / kBoardDimension;
    std::string result;
    result += static_cast<char>(kMinFileChar + fromFile);
    result += static_cast<char>(kMinRankChar + fromRank);
    result += static_cast<char>(kMinFileChar + toFile);
    result += static_cast<char>(kMinRankChar + toRank);
    return result;
}

std::optional<Move> UCINotation::uciToMove(const std::string& uciMove) {
    if (uciMove.length() != kUciMoveLength) {
        return std::nullopt;
    }

    int fromFile{uciMove[0] - kMinFileChar};
    int fromRank{uciMove[1] - kMinRankChar};
    int toFile{uciMove[2] - kMinFileChar};
    int toRank{uciMove[3] - kMinRankChar};

    if (fromFile < 0 || fromFile >= kBoardDimension || fromRank < 0 ||
        fromRank >= kBoardDimension || toFile < 0 || toFile >= kBoardDimension || toRank < 0 ||
        toRank >= kBoardDimension) {
        return std::nullopt;
    }

    int from = (fromRank * kBoardDimension) + fromFile;
    int to = (toRank * kBoardDimension) + toFile;
    return Move{from, to};
}

int runUCIEngine() {
    UCIEngine engine;
    engine.run();
    return 0;
}
