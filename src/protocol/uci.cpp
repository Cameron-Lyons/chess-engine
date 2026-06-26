#include "uci.h"
#include "../ai/SyzygyTablebase.h"
#include "../core/BitboardMoves.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "../core/Move.h"
#include "../evaluation/Evaluation.h"
#include "../evaluation/NNUE.h"
#include "../search/AdvancedSearch.h"
#include "../search/ValidMoves.h"
#include "../search/search.h"
#include "../utils/ChessFormat.h"
#include "../utils/TunableParams.h"
#include "uci_output.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
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
constexpr std::string_view kStartingFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

[[nodiscard]] std::string_view ltrim(std::string_view value) {
    const std::size_t start = value.find_first_not_of(' ');
    return start == std::string_view::npos ? std::string_view{} : value.substr(start);
}

[[nodiscard]] std::pair<std::string_view, std::string_view> splitFirstToken(std::string_view value) {
    value = ltrim(value);
    if (value.empty()) {
        return {{}, {}};
    }
    const std::size_t end = value.find(' ');
    if (end == std::string_view::npos) {
        return {value, {}};
    }
    return {value.substr(0, end), ltrim(value.substr(end + 1))};
}

class TokenView {
public:
    explicit TokenView(std::string_view input) : rest_(input) {}

    [[nodiscard]] std::optional<std::string_view> next() {
        rest_ = ltrim(rest_);
        if (rest_.empty()) {
            return std::nullopt;
        }
        const std::size_t end = rest_.find(' ');
        const std::string_view token = rest_.substr(0, end);
        rest_ = end == std::string_view::npos ? std::string_view{} : ltrim(rest_.substr(end + 1));
        return token;
    }

private:
    std::string_view rest_;
};

[[nodiscard]] std::optional<int> parseInt(std::string_view value) {
    int parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (ec != std::errc{} || ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] std::pair<std::string_view, std::string_view> parseSetOption(std::string_view command) {
    const std::size_t namePos = command.find("name ");
    if (namePos == std::string_view::npos) {
        return {{}, {}};
    }
    const std::string_view afterName = command.substr(namePos + kNameTokenLength);
    const std::size_t valuePos = afterName.find(" value ");
    if (valuePos != std::string_view::npos) {
        return {afterName.substr(0, valuePos), afterName.substr(valuePos + kValueTokenLength)};
    }
    return {afterName, {}};
}

using CommandHandler = void (UCIEngine::*)(std::string_view);
} // namespace

UCIEngine::UCIEngine() {

    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
    board = Board();
    board.InitializeFromFEN(kStartingFen);
    positionHistoryHashes = {ComputeZobrist(board)};
    openingBook = std::make_unique<EnhancedOpeningBook>();
    searchContext.threads = options.threads;
    searchContext.hashSizeMb = options.hashSize;
    setUseNeuralNetwork(options.useNeuralNetwork);
}

UCIEngine::~UCIEngine() {
    handleStop();
}

void UCIEngine::searchThreadEntry(const SearchTask& task, std::stop_token stopToken) {
    try {
        SearchResult result = performSearch(task.boardSnapshot, task.depth, task.timeForMove,
                                            task.optimalTime, task.maxTime, std::move(stopToken));

        if (isSearching.load()) {
            std::optional<Move> ponderMove;
            reportBestMove(result.bestMove, ponderMove);
            int nps = result.timeMs > 0 ? result.nodes * kNpsMillisScale / result.timeMs : 0;
            reportInfo(result.depth, result.depth, result.timeMs, result.nodes, nps, {},
                       result.score, 0);
        }
    } catch (const std::exception& e) {
        uci::output::println("info string Search error: {}", e.what());
    }

    isSearching.store(false);
    isPondering.store(false);
}

void UCIEngine::joinSearchThread() {
    searchThread.join();
}

void UCIEngine::run() {
    std::string input;
    while (std::getline(std::cin, input)) {
        if (!input.empty()) {
            processCommand(input);
        }
    }
}

void UCIEngine::processCommand(std::string_view command) {
    const auto [cmd, args] = splitFirstToken(command);
    if (cmd.empty()) {
        return;
    }

    static constexpr std::array<CommandHandler, 12> kCommands = {
        &UCIEngine::handleUCI,
        &UCIEngine::handleIsReady,
        &UCIEngine::handleSetOption,
        &UCIEngine::handleNewGame,
        &UCIEngine::handlePosition,
        &UCIEngine::handleGo,
        &UCIEngine::handleStop,
        &UCIEngine::handlePonderHit,
        &UCIEngine::handleQuit,
        &UCIEngine::handleRegister,
        &UCIEngine::handleInfo,
        &UCIEngine::handleBookStats,
    };
    static constexpr std::array<std::string_view, kCommands.size()> kCommandNames = {
        "uci",       "isready",   "setoption", "ucinewgame", "position", "go",
        "stop",      "ponderhit", "quit",      "register",   "info",     "bookstats",
    };

    if (cmd == "debug") {
        handleDebug(args);
        return;
    }

    for (std::size_t i = 0; i < kCommands.size(); ++i) {
        if (cmd == kCommandNames[i]) {
            (this->*kCommands[i])(args);
            return;
        }
    }

    uci::output::println("info string Unknown command: {}", cmd);
}

void UCIEngine::handleUCI(std::string_view /*args*/) {
    uci::output::println("id name ModernChess v2.0");
    uci::output::println("id author Chess Engine Team");
    uci::output::println("option name Hash type spin default 32 min 1 max 1024");
    uci::output::println("option name Threads type spin default 1 min 1 max 16");
    uci::output::println("option name MultiPV type spin default 1 min 1 max 10");
    uci::output::println("option name Ponder type check default false");
    uci::output::println("option name OwnBook type check default true");
    uci::output::println("option name Move Overhead type spin default 10 min 0 max 5000");
    uci::output::println("option name Minimum Thinking Time type spin default 20 min 0 max 5000");
    uci::output::println("option name Use Neural Network type check default false");
    uci::output::println("option name EvalFile type string default ");
    uci::output::println("option name Use Tablebases type check default true");
    uci::output::println("option name SyzygyPath type string default ");
    uci::output::println("option name Debug type check default false");
    uci::output::println("option name Show Current Line type check default false");
    uci::output::println("option name Contempt type spin default 0 min -100 max 100");

    for (const auto& p : TunableRegistry::instance().all()) {
        uci::output::println("option name {} type spin default {} min {} max {}", p.name,
                             p.defaultValue, p.minValue, p.maxValue);
    }

    uci::output::println("uciok");
}

void UCIEngine::handleIsReady(std::string_view /*args*/) {
    uci::output::println("readyok");
}

void UCIEngine::handleSetOption(std::string_view command) {
    const auto [name, value] = parseSetOption(command);
    if (name.empty()) {
        return;
    }

    static constexpr std::array<std::pair<std::string_view, void (UCIEngine::*)(int)>, 5> kSpinOptions =
        {{{"Hash", &UCIEngine::setHashSize},
          {"Threads", &UCIEngine::setThreads},
          {"MultiPV", &UCIEngine::setMultiPV},
          {"Move Overhead", &UCIEngine::setMoveOverhead},
          {"Minimum Thinking Time", &UCIEngine::setMinimumThinkingTime}}};

    for (const auto& [optionName, setter] : kSpinOptions) {
        if (name == optionName) {
            if (const auto parsed = parseInt(value)) {
                (this->*setter)(*parsed);
            }
            return;
        }
    }

    static constexpr std::array<std::pair<std::string_view, void (UCIEngine::*)(bool)>, 6>
        kCheckOptions = {{{"Ponder", &UCIEngine::setPonder},
                          {"OwnBook", &UCIEngine::setOwnBook},
                          {"Use Neural Network", &UCIEngine::setUseNeuralNetwork},
                          {"Use Tablebases", &UCIEngine::setUseTablebases},
                          {"Debug", &UCIEngine::setDebug},
                          {"Show Current Line", &UCIEngine::setShowCurrLine}}};

    for (const auto& [optionName, setter] : kCheckOptions) {
        if (name == optionName) {
            (this->*setter)(value == "true");
            return;
        }
    }

    if (name == "EvalFile") {
        setEvalFile(value);
        return;
    }

    if (name == "SyzygyPath") {
        options.syzygyPath = std::string(value);
        if (!value.empty()) {
            Syzygy::init(options.syzygyPath);
        }
        return;
    }

    if (name == "Contempt") {
        if (const auto parsed = parseInt(value)) {
            options.contempt = *parsed;
        }
        return;
    }

    if (const auto parsed = parseInt(value)) {
        TunableRegistry::instance().set(name, *parsed);
    }
}

void UCIEngine::handleNewGame(std::string_view /*args*/) {

    board = Board();
    board.InitializeFromFEN(kStartingFen);
    positionHistoryHashes = {ComputeZobrist(board)};
    uci::output::println("info string New game started");
}

void UCIEngine::handlePosition(std::string_view command) {
    TokenView tokens(command);

    const auto kind = tokens.next();
    if (!kind) {
        uci::output::println("info string Error: Invalid position command");
        return;
    }

    if (*kind == "startpos") {
        board = Board();
        board.InitializeFromFEN(kStartingFen);
        positionHistoryHashes = {ComputeZobrist(board)};
    } else if (*kind == "fen") {
        std::string fen;
        for (int i = 0; i < kFenFieldCount; ++i) {
            if (const auto part = tokens.next(); part && !part->empty()) {
                if (!fen.empty()) {
                    fen += ' ';
                }
                fen.append(part->begin(), part->end());
            }
        }

        if (fen.empty()) {
            uci::output::println("info string Error: Invalid FEN string");
            return;
        }

        board = Board();
        if (const auto parsed = board.fromFEN(fen); parsed.has_value()) {
            positionHistoryHashes = {ComputeZobrist(board)};
            uci::output::println("info string FEN position set: {}", fen);
        } else {
            uci::output::println("info string Error: Invalid FEN string");
            return;
        }
    } else {
        uci::output::println("info string Error: Expected 'startpos' or 'fen'");
        return;
    }

    if (const auto movesToken = tokens.next(); movesToken && *movesToken == "moves") {
        while (const auto moveToken = tokens.next()) {
            if (const auto movePos = UCINotation::uciToMove(*moveToken)) {
                Board legalProbe = board;
                if (IsMoveLegal(legalProbe, movePos->first, movePos->second)) {
                    if (!applySearchMove(board, movePos->first, movePos->second)) {
                        continue;
                    }
                    board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                        : ChessPieceColor::WHITE;
                    board.updateBitboards();
                    positionHistoryHashes.push_back(ComputeZobrist(board));
                } else {
                    uci::output::println("info string Warning: Invalid move {}", *moveToken);
                }
            } else {
                uci::output::println("info string Warning: Could not parse move {}", *moveToken);
            }
        }
    }
}

void UCIEngine::handleGo(std::string_view command) {
    if (isSearching.load()) {
        uci::output::println("info string Search already in progress");
        return;
    }

    TokenView tokens(command);
    int wtime = kUnsetTimeValue;
    int btime = kUnsetTimeValue;
    int winc = 0;
    int binc = 0;
    int movestogo = kUnsetTimeValue;
    int searchDepth = kDefaultSearchDepth;
    int movetime = kUnsetTimeValue;
    bool isPonder = false;
    bool isInfinite = false;

    while (const auto token = tokens.next()) {
        if (*token == "wtime") {
            if (const auto value = tokens.next(); value) {
                wtime = parseInt(*value).value_or(kUnsetTimeValue);
            }
        } else if (*token == "btime") {
            if (const auto value = tokens.next(); value) {
                btime = parseInt(*value).value_or(kUnsetTimeValue);
            }
        } else if (*token == "winc") {
            if (const auto value = tokens.next(); value) {
                winc = parseInt(*value).value_or(0);
            }
        } else if (*token == "binc") {
            if (const auto value = tokens.next(); value) {
                binc = parseInt(*value).value_or(0);
            }
        } else if (*token == "movestogo") {
            if (const auto value = tokens.next(); value) {
                movestogo = parseInt(*value).value_or(kUnsetTimeValue);
            }
        } else if (*token == "depth") {
            if (const auto value = tokens.next(); value) {
                searchDepth = parseInt(*value).value_or(kDefaultSearchDepth);
            }
        } else if (*token == "movetime") {
            if (const auto value = tokens.next(); value) {
                movetime = parseInt(*value).value_or(kUnsetTimeValue);
            }
        } else if (*token == "ponder") {
            isPonder = true;
        } else if (*token == "infinite") {
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

    joinSearchThread();

    isSearching.store(true);
    isPondering.store(isPonder);
    stopRequested.store(false);
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = timeForMove;
    searchDepthLimit = searchDepth;

    searchStopSource = std::stop_source{};
    activeSearchStopToken = searchStopSource.get_token();
    activeSearchTask = SearchTask{searchDepth, timeForMove, optimalTime, maxTime, board};

    const int createResult =
        searchThread.start([this, task = activeSearchTask,
                            token = activeSearchStopToken]() { searchThreadEntry(task, token); },
                           kSearchThreadStackBytes, activeSearchStopToken);

    if (createResult != 0) {
        isSearching.store(false);
        isPondering.store(false);
        uci::output::println("info string Search thread creation failed");
        return;
    }
}

void UCIEngine::handleStop(std::string_view /*args*/) {
    if (!isSearching.load()) {
        joinSearchThread();
        stopRequested.store(false);
        return;
    }

    stopRequested.store(true);
    searchStopSource.request_stop();
    joinSearchThread();
    isSearching.store(false);
    isPondering.store(false);
    stopRequested.store(false);
}

void UCIEngine::handlePonderHit(std::string_view /*args*/) {
    isPondering.store(false);
}

void UCIEngine::handleQuit(std::string_view /*args*/) {
    isSearching.store(false);
    isPondering.store(false);
    joinSearchThread();
    std::exit(0);
}

void UCIEngine::handleDebug(std::string_view /*command*/) const {
    uci::output::println("info string Debug mode: {}", options.debug ? "enabled" : "disabled");
}

void UCIEngine::handleRegister(std::string_view /*command*/) {
    uci::output::println("info string Registration not required");
}

void UCIEngine::handleInfo(std::string_view /*command*/) {
    uci::output::println("info string Info command received");
}

void UCIEngine::handleBookStats(std::string_view /*args*/) {
    if (!openingBook) {
        uci::output::println("info string Opening book not available");
        return;
    }

    const EnhancedOpeningBook::BookStats stats = openingBook->getStats();
    uci::output::print("info string Book Statistics:\n"
                       "info string   Positions: {}\n"
                       "info string   Moves: {}\n"
                       "info string   Total Games: {}\n"
                       "info string   Average Win Rate: {}\n"
                       "info string   Average Rating: {}\n",
                       stats.totalPositions, stats.totalMoves, stats.totalGames,
                       stats.averageWinRate, stats.averageRating);
}

void UCIEngine::reportBestMove(const Move& move, const std::optional<Move>& ponderMove) {
    if (ponderMove) {
        uci::output::println("bestmove {} ponder {}", UCINotation::moveToUCI(move),
                             UCINotation::moveToUCI(*ponderMove));
        return;
    }
    uci::output::println("bestmove {}", UCINotation::moveToUCI(move));
}

void UCIEngine::reportInfo(int depth, int seldepth, int time, int nodes, int nps,
                           const std::vector<Move>& pv, int score, int hashfull) {
    std::string line = std::format("info depth {}", depth);
    if (seldepth > 0) {
        line += std::format(" seldepth {}", seldepth);
    }
    if (time > 0) {
        line += std::format(" time {}", time);
    }
    if (nodes > 0) {
        line += std::format(" nodes {}", nodes);
    }
    if (nps > 0) {
        line += std::format(" nps {}", nps);
    }
    if (hashfull > 0) {
        line += std::format(" hashfull {}", hashfull);
    }
    if (score != 0) {
        if (score > kMateScoreThreshold) {
            line += std::format(" score mate {}",
                                (kMateScoreThreshold - score + kMatePlyOffset) / kMatePlyDivisor);
        } else if (score < -kMateScoreThreshold) {
            line += std::format(" score mate {}",
                                -(kMateScoreThreshold + score + kMatePlyOffset) / kMatePlyDivisor);
        } else {
            line += std::format(" score cp {}", score);
        }
    }
    if (!pv.empty()) {
        line += " pv";
        for (const auto& move : pv) {
            line += std::format(" {}", UCINotation::moveToUCI(move));
        }
    }
    uci::output::println("{}", line);
}

void UCIEngine::reportInfo(const std::string& info) {
    uci::output::println("info string {}", info);
}

SearchResult UCIEngine::performSearch(const Board& board, int depth, int timeLimit, int optimalTime,
                                      int maxTime, std::stop_token stopToken) {
    SearchResult result;

    if (options.useTablebases && Syzygy::canProbe(board)) {
        Syzygy::DTZResult dtzResult{};
        if (Syzygy::probeRoot(board, dtzResult)) {
            result.bestMove = Move{dtzResult.from, dtzResult.to};
            switch (dtzResult.wdl) {
                case Syzygy::PROBE_WIN:
                case Syzygy::PROBE_CURSED_WIN:
                    result.score = kTablebaseMateScore;
                    break;
                case Syzygy::PROBE_LOSS:
                case Syzygy::PROBE_BLESSED_LOSS:
                    result.score = -kTablebaseMateScore;
                    break;
                default:
                    result.score = 0;
                    break;
            }
            result.depth = 0;
            result.nodes = 0;
            result.timeMs = 0;
            return result;
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
    if (positionHistoryHashes.size() > 1) {
        config.previousPositionHashes.assign(positionHistoryHashes.begin(),
                                             positionHistoryHashes.end() - 1);
    }
    config.externalStop = &stopRequested;
    config.externalStopToken = stopToken.stop_possible() ? &stopToken : nullptr;
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
            uci::output::println(
                "info string NNUE requested but no model loaded; using classical eval");
        }
    } else {
        setNNUEEnabled(false);
    }
}

void UCIEngine::setEvalFile(std::string_view path) {
    options.evalFile = std::string(path);
    if (path.empty()) {
        NNUE::globalEvaluator.reset();
        setNNUEEnabled(false);
        return;
    }
    if (!NNUE::init(options.evalFile)) {
        uci::output::println("info string Failed to load NNUE model: {}", options.evalFile);
        setNNUEEnabled(false);
        return;
    }
    uci::output::println("info string NNUE model loaded: {}", options.evalFile);
    if (options.useNeuralNetwork) {
        setNNUEEnabled(true);
    }
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
    return chess::format::moveToUci(move);
}

std::optional<Move> UCINotation::uciToMove(std::string_view uciMove) {
    if (uciMove.size() != kUciMoveLength) {
        return std::nullopt;
    }

    const int fromFile{uciMove[0] - kMinFileChar};
    const int fromRank{uciMove[1] - kMinRankChar};
    const int toFile{uciMove[2] - kMinFileChar};
    const int toRank{uciMove[3] - kMinRankChar};

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
