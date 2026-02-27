#include "uci.h"
#include "../ai/SyzygyTablebase.h"
#include "../core/BitboardMoves.h"
#include "../evaluation/Evaluation.h"
#include "../search/ValidMoves.h"
#include "../search/search.h"
#include "../utils/TunableParams.h"
#include "../utils/engine_globals.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <thread>

extern Board ChessBoard;
extern Board PrevBoard;

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
constexpr std::size_t kUciSearchThreadStackBytes = 8ULL * 1024ULL * 1024ULL;
constexpr int kBoardDimension = 8;
constexpr int kBoardSquareCount = kBoardDimension * kBoardDimension;
constexpr int kMaxSquareIndex = kBoardSquareCount - 1;
constexpr int kUciMoveLength = 4;
constexpr int kAlgebraicSquareLength = 2;
constexpr int kEnPassantTokenLength = 2;
constexpr char kMinFileChar = 'a';
constexpr char kMaxFileChar = 'h';
constexpr char kMinRankChar = '1';
constexpr char kEnPassantWhiteRankChar = '3';
constexpr char kEnPassantBlackRankChar = '6';
constexpr int kTablebaseMateScore = 30000;
constexpr const char* kStartingFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
} // namespace

UCIEngine::UCIEngine() : isSearching(false), isPondering(false) {

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
    setUseNeuralNetwork(options.useNeuralNetwork);
}

UCIEngine::~UCIEngine() = default;

void* UCIEngine::searchThreadEntry(void* arg) {
    std::unique_ptr<SearchTask> task(static_cast<SearchTask*>(arg));
    UCIEngine* self = task->engine;

    try {
        SearchResult result = self->performSearch(task->boardSnapshot, task->depth, task->timeForMove,
                                                  task->optimalTime, task->maxTime);

        if (self->isSearching) {
            std::pair<int, int> ponderMove = {kInvalidSquare, kInvalidSquare};
            if (result.bestMove.first >= 0) {
                Board tempBoard = task->boardSnapshot;
                if (applySearchMove(tempBoard, result.bestMove.first, result.bestMove.second)) {
                    tempBoard.turn =
                        (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;
                    uint64_t hash = ComputeZobrist(tempBoard);
                    TTEntry ttEntry;
                    if (TransTable.find(hash, ttEntry) && ttEntry.bestMove.first >= 0) {
                        ponderMove = ttEntry.bestMove;
                    }
                }
            }
            self->reportBestMove(result.bestMove, ponderMove);
            int nps = result.timeMs > 0 ? result.nodes * kNpsMillisScale / result.timeMs : 0;
            self->reportInfo(result.depth, result.depth, result.timeMs, result.nodes, nps, {},
                             result.score, 0);
        }
    } catch (const std::exception& e) {
        std::cout << "info string Search error: " << e.what() << '\n';
    }

    self->isSearching = false;
    self->isPondering = false;
    return nullptr;
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
    TransTable.clear();
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
    } else         if (word == "fen") {

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
            board.InitializeFromFEN(fen);
            std::cout << "info string FEN position set: " << fen << '\n';
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
            std::pair<int, int> movePos = UCINotation::uciToMove(move);
            if (movePos.first != -1 && movePos.second != -1) {

                GenValidMoves(board);
                std::vector<std::pair<int, int>> validMoves = GetAllMoves(board, board.turn);
                bool isValid = false;
                for (const auto& validMove : validMoves) {
                    if (validMove.first == movePos.first && validMove.second == movePos.second) {
                        isValid = true;
                        break;
                    }
                }

                if (isValid) {
                    if (!applySearchMove(board, movePos.first, movePos.second)) {
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
    if (isSearching) {
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

    isSearching = true;
    isPondering = isPonder;
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = timeForMove;
    searchDepthLimit = searchDepth;

    if (searchThreadActive) {
        pthread_join(searchThread, nullptr);
        searchThreadActive = false;
    }

    auto* task = new SearchTask{this, searchDepth, timeForMove, optimalTime, maxTime, board};
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, kUciSearchThreadStackBytes);
    int createRc = pthread_create(&searchThread, &attr, &UCIEngine::searchThreadEntry, task);
    pthread_attr_destroy(&attr);
    if (createRc != 0) {
        delete task;
        isSearching = false;
        isPondering = false;
        std::cout << "info string Search thread creation failed" << '\n';
        return;
    }
    searchThreadActive = true;
}

void UCIEngine::handleStop() {
    isSearching = false;
    isPondering = false;
    if (searchThreadActive) {
        pthread_join(searchThread, nullptr);
        searchThreadActive = false;
    }
}

void UCIEngine::handlePonderHit() {
    isPondering = false;
}

void UCIEngine::handleQuit() {
    isSearching = false;
    isPondering = false;
    if (searchThreadActive) {
        pthread_join(searchThread, nullptr);
        searchThreadActive = false;
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

void UCIEngine::reportBestMove(const std::pair<int, int>& move,
                               const std::pair<int, int>& ponderMove) {
    std::cout << "bestmove " << UCINotation::moveToUCI(move)
              << (ponderMove.first != kInvalidSquare ? " ponder " + UCINotation::moveToUCI(ponderMove)
                                                    : "")
              << '\n';
}

void UCIEngine::reportInfo(int depth, int seldepth, int time, int nodes, int nps,
                           const std::vector<std::pair<int, int>>& pv, int score, int hashfull) {
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
            std::pair<int, int> bookMove = openingBook->getBestMove(board, false);
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

    result = iterativeDeepeningParallel(searchBoard, depth, timeLimit, options.threads,
                                        options.contempt, options.multiPV, optimalTime, maxTime,
                                        options.hashSize);

    return result;
}

void UCIEngine::setHashSize(int size) {
    options.hashSize = size;
    TransTable.resize(size);
}

void UCIEngine::setThreads(int num) {
    options.threads = std::clamp(num, 1, 16);
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

std::string UCINotation::moveToUCI(const std::pair<int, int>& move) {
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

std::pair<int, int> UCINotation::uciToMove(const std::string& uciMove) {
    if (uciMove.length() != kUciMoveLength) {
        return {kInvalidSquare, kInvalidSquare};
    }

    int fromFile{uciMove[0] - kMinFileChar};
    int fromRank{uciMove[1] - kMinRankChar};
    int toFile{uciMove[2] - kMinFileChar};
    int toRank{uciMove[3] - kMinRankChar};

    if (fromFile < 0 || fromFile >= kBoardDimension || fromRank < 0 ||
        fromRank >= kBoardDimension || toFile < 0 || toFile >= kBoardDimension || toRank < 0 ||
        toRank >= kBoardDimension) {
        return {kInvalidSquare, kInvalidSquare};
    }

    int from = (fromRank * kBoardDimension) + fromFile;
    int to = (toRank * kBoardDimension) + toFile;
    return {from, to};
}

std::string UCINotation::squareToAlgebraic(int square) {
    if (square < 0 || square > kMaxSquareIndex) {
        return "";
    }

    int file{square % kBoardDimension};
    int rank{square / kBoardDimension};
    std::string result;
    result += static_cast<char>(kMinFileChar + file);
    result += static_cast<char>(kMinRankChar + rank);
    return result;
}

int UCINotation::algebraicToSquare(const std::string& algebraic) {
    if (algebraic.length() != kAlgebraicSquareLength) {
        return kInvalidSquare;
    }

    int file{algebraic[0] - kMinFileChar};
    int rank{algebraic[1] - kMinRankChar};

    if (file < 0 || file >= kBoardDimension || rank < 0 || rank >= kBoardDimension) {
        return kInvalidSquare;
    }

    return (rank * kBoardDimension) + file;
}

bool UCINotation::isValidUCIMove(const std::string& uciMove) {
    return uciMove.length() == kUciMoveLength && uciToMove(uciMove).first != kInvalidSquare;
}

std::string UCINotation::getMoveType(const Board& board, const std::pair<int, int>& move) {
    const Piece& piece = board.squares[move.first].piece;
    const Piece& target = board.squares[move.second].piece;

    if (target.PieceType != ChessPieceType::NONE) {
        return "capture";
    }

    if (piece.PieceType == ChessPieceType::KING) {
        int fileDiff = abs((move.second % kBoardDimension) - (move.first % kBoardDimension));
        if (fileDiff == 2) {
            return "castling";
        }
    }

    if (piece.PieceType == ChessPieceType::PAWN) {
        int rank = move.second / kBoardDimension;
        if ((piece.PieceColor == ChessPieceColor::WHITE && rank == kBoardDimension - 1) ||
            (piece.PieceColor == ChessPieceColor::BLACK && rank == 0)) {
            return "promotion";
        }
    }

    if (piece.PieceType == ChessPieceType::PAWN) {
        int fileDiff = abs((move.second % kBoardDimension) - (move.first % kBoardDimension));
        if (fileDiff == 1 && target.PieceType == ChessPieceType::NONE) {
            return "enpassant";
        }
    }

    return "normal";
}

void UCIPosition::parseFEN(const std::string& fen, Board& board) {
    board.InitializeFromFEN(fen);
}

void UCIPosition::parseMoves(const std::string& moves, Board& board) {
    std::istringstream iss(moves);
    std::string move;
    while (iss >> move) {
        std::pair<int, int> movePos = UCINotation::uciToMove(move);
        if (movePos.first != kInvalidSquare && movePos.second != kInvalidSquare) {
            if (!applySearchMove(board, movePos.first, movePos.second)) {
                continue;
            }
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
        }
    }
}

std::string UCIPosition::generateFEN(const Board& board) {
    (void)board;
    return kStartingFen;
}

bool UCIPosition::isValidFEN(const std::string& fen) {
    if (fen.empty()) {
        return false;
    }

    std::istringstream iss(fen);
    std::string token;

    if (!(iss >> token)) {
        return false;
    }

    int rank = kBoardDimension - 1;
    int file = 0;
    for (char c : token) {
        if (c == '/') {
            if (file != kBoardDimension) {
                return false;
            }
            rank--;
            file = 0;
            if (rank < 0) {
                return false;
            }
        } else if (isdigit(c)) {
            int count = c - '0';
            if (count == 0 || file + count > kBoardDimension) {
                return false;
            }
            file += count;
        } else if (isalpha(c)) {
            if (file >= kBoardDimension) {
                return false;
            }
            char piece = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (piece != 'p' && piece != 'n' && piece != 'b' && piece != 'r' && piece != 'q' &&
                piece != 'k') {
                return false;
            }
            file++;
        } else {
            return false;
        }
    }

    if (rank != 0 || file != kBoardDimension) {
        return false;
    }

    if (!(iss >> token)) {
        return false;
    }
    if (token != "w" && token != "b") {
        return false;
    }

    if (!(iss >> token)) {
        return false;
    }
    for (char c : token) {
        if (c != 'K' && c != 'Q' && c != 'k' && c != 'q' && c != '-') {
            return false;
        }
    }

    if (!(iss >> token)) {
        return false;
    }
    if (token != "-") {
        if (token.length() != kEnPassantTokenLength) {
            return false;
        }
        if (token[0] < kMinFileChar || token[0] > kMaxFileChar) {
            return false;
        }
        if (token[1] != kEnPassantWhiteRankChar && token[1] != kEnPassantBlackRankChar) {
            return false;
        }
    }

    if (!(iss >> token)) {
        return false;
    }
    if (!isdigit(token[0])) {
        return false;
    }

    if (iss >> token) {
        if (!isdigit(token[0])) {
            return false;
        }
    }

    return true;
}

uint64_t UCIPosition::getPositionHash(const Board& board) {
    return ComputeZobrist(board);
}

std::string UCISearchInfo::formatInfo(int depth, int seldepth, int time, int nodes, int nps,
                                      const std::vector<std::pair<int, int>>& pv, int score,
                                      int hashfull) {
    std::ostringstream oss;
    oss << "info depth " << depth;
    if (seldepth > 0) {
        oss << " seldepth " << seldepth;
    }
    if (time > 0) {
        oss << " time " << time;
    }
    if (nodes > 0) {
        oss << " nodes " << nodes;
    }
    if (nps > 0) {
        oss << " nps " << nps;
    }
    if (hashfull > 0) {
        oss << " hashfull " << hashfull;
    }
    if (score != 0) {
        oss << " " << formatScore(score);
    }
    if (!pv.empty()) {
        oss << " pv" << formatPV(pv);
    }
    return oss.str();
}

std::string UCISearchInfo::formatScore(int score, bool isMate) {
    (void)isMate;
    std::ostringstream oss;
    oss << "score ";
    if (score > kMateScoreThreshold) {
        oss << "mate " << (kMateScoreThreshold - score + kMatePlyOffset) / kMatePlyDivisor;
    } else if (score < -kMateScoreThreshold) {
        oss << "mate " << -(kMateScoreThreshold + score + kMatePlyOffset) / kMatePlyDivisor;
    } else {
        oss << "cp " << score;
    }
    return oss.str();
}

std::string UCISearchInfo::formatPV(const std::vector<std::pair<int, int>>& pv) {
    std::ostringstream oss;
    for (const auto& move : pv) {
        oss << " " << UCINotation::moveToUCI(move);
    }
    return oss.str();
}

std::string UCISearchInfo::formatTime(int timeMs) {
    return std::to_string(timeMs);
}

std::string UCISearchInfo::formatNPS(int nodes, int timeMs) {
    if (timeMs == 0) {
        return "0";
    }
    return std::to_string(nodes * kNpsMillisScale / timeMs);
}

int runUCIEngine() {
    UCIEngine engine;
    engine.run();
    return 0;
}
