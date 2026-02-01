#include "uci.h"
#include "../core/BitboardMoves.h"
#include "../evaluation/Evaluation.h"
#include "../search/ValidMoves.h"
#include "../search/search.h"
#include "../utils/engine_globals.h"
#include <chrono>
#include <future>
#include <iostream>
#include <sstream>
#include <thread>

extern Board ChessBoard;
extern Board PrevBoard;

UCIEngine::UCIEngine() : isSearching(false) {

    initKnightAttacks();
    initKingAttacks();

    InitZobrist();

    board = Board();
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    nnEvaluator = std::make_unique<NeuralNetworkEvaluator>();
    tablebase = std::make_unique<EndgameTablebase>();
    openingBook = std::make_unique<EnhancedOpeningBook>();

    TimeManager::TimeControl tc;
    tc.baseTime = 300000;
    tc.increment = 0;
    tc.movesToGo = 0;
    tc.isInfinite = false;
    timeManager = std::make_unique<TimeManager>(tc);
}

UCIEngine::~UCIEngine() = default;

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

        std::cout << "info string Unknown command: " << cmd << std::endl;
    }
}

void UCIEngine::handleUCI() {
    std::cout << "id name ModernChess v2.0" << std::endl;
    std::cout << "id author Chess Engine Team" << std::endl;

    std::cout << "option name Hash type spin default 32 min 1 max 1024" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 16" << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 10" << std::endl;
    std::cout << "option name Ponder type check default false" << std::endl;
    std::cout << "option name OwnBook type check default true" << std::endl;
    std::cout << "option name Move Overhead type spin default 10 min 0 max 5000" << std::endl;
    std::cout << "option name Minimum Thinking Time type spin default 20 min 0 max 5000"
              << std::endl;
    std::cout << "option name Use Neural Network type check default true" << std::endl;
    std::cout << "option name Neural Network Weight type spin default 70 min 0 max 100"
              << std::endl;
    std::cout << "option name Use Tablebases type check default true" << std::endl;
    std::cout << "option name Debug type check default false" << std::endl;
    std::cout << "option name Show Current Line type check default false" << std::endl;
    std::cout << "option name Contempt type spin default 0 min -100 max 100" << std::endl;

    std::cout << "uciok" << std::endl;
}

void UCIEngine::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCIEngine::handleSetOption(const std::string& command) {
    std::istringstream iss(command);
    std::string word;
    iss >> word;

    std::string name, value;
    if (iss >> word && word == "name") {
        std::getline(iss, name);

        if (!name.empty() && name[0] == ' ') {
            name = name.substr(1);
        }

        if (iss >> word && word == "value") {
            std::getline(iss, value);
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
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
                setNNWeight(std::stof(value) / 100.0f);
            } else if (name == "Use Tablebases") {
                setUseTablebases(value == "true");
            } else if (name == "Debug") {
                setDebug(value == "true");
            } else if (name == "Show Current Line") {
                setShowCurrLine(value == "true");
            } else if (name == "Contempt") {
                options.contempt = std::stoi(value);
            }
        }
    }
}

void UCIEngine::handleNewGame() {

    board = Board();
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    TransTable.clear();
    std::cout << "info string New game started" << std::endl;
}

void UCIEngine::handlePosition(const std::string& command) {
    std::istringstream iss(command);
    std::string word;
    iss >> word;

    if (!(iss >> word)) {
        std::cout << "info string Error: Invalid position command" << std::endl;
        return;
    }

    if (word == "startpos") {

        board = Board();
        board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    } else if (word == "fen") {

        std::string fen;
        for (int i = 0; i < 6; i++) {
            std::string part;
            if (iss >> part) {
                if (i > 0)
                    fen += " ";
                fen += part;
            }
        }

        if (!fen.empty()) {

            board = Board();
            board.InitializeFromFEN(fen);
            std::cout << "info string FEN position set: " << fen << std::endl;
        } else {
            std::cout << "info string Error: Invalid FEN string" << std::endl;
            return;
        }
    } else {
        std::cout << "info string Error: Expected 'startpos' or 'fen'" << std::endl;
        return;
    }

    if (iss >> word && word == "moves") {
        std::string move;
        while (iss >> move) {
            std::pair<int, int> movePos = uciToMove(move);
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
                    board.movePiece(movePos.first, movePos.second);
                    board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                        : ChessPieceColor::WHITE;
                    board.updateBitboards();
                } else {
                    std::cout << "info string Warning: Invalid move " << move << std::endl;
                }
            } else {
                std::cout << "info string Warning: Could not parse move " << move << std::endl;
            }
        }
    }
}

void UCIEngine::handleGo(const std::string& command) {
    if (isSearching) {
        std::cout << "info string Search already in progress" << std::endl;
        return;
    }

    std::istringstream iss(command);
    std::string word;
    iss >> word;

    int wtime = -1, btime = -1, winc = 0, binc = 0;
    int movestogo = -1, searchDepth = 8, movetime = -1;

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
        }
    }

    int timeForMove = 5000;

    if (movetime > 0) {
        timeForMove = movetime;
    } else if (wtime > 0 || btime > 0) {
        int currentTime = (board.turn == ChessPieceColor::WHITE) ? wtime : btime;
        int increment = (board.turn == ChessPieceColor::WHITE) ? winc : binc;

        if (currentTime > 0) {
            timeForMove = currentTime / 30;
            if (increment > 0) {
                timeForMove += increment;
            }
        }
    }

    isSearching = true;
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = timeForMove;
    searchDepthLimit = searchDepth;

    std::thread searchThread([this, searchDepth, timeForMove]() {
        try {
            SearchResult result = performSearch(board, searchDepth, timeForMove);

            if (isSearching) {
                reportBestMove(result.bestMove);
                int nps = result.timeMs > 0 ? result.nodes * 1000 / result.timeMs : 0;
                reportInfo(result.depth, result.depth, result.timeMs, result.nodes,
                           nps, {}, result.score, 0);
            }
        } catch (const std::exception& e) {
            std::cout << "info string Search error: " << e.what() << std::endl;
        }

        isSearching = false;
    });

    searchThread.detach();
}

void UCIEngine::handleStop() {
    isSearching = false;
}

void UCIEngine::handleQuit() {
    isSearching = false;
    std::exit(0);
}

void UCIEngine::handleDebug(const std::string& command) {
    (void)command;

    std::cout << "info string Debug mode: " << (options.debug ? "enabled" : "disabled")
              << std::endl;
}

void UCIEngine::handleRegister(const std::string& command) {
    (void)command;

    std::cout << "info string Registration not required" << std::endl;
}

void UCIEngine::handleInfo(const std::string& command) {
    (void)command;

    std::cout << "info string Info command received" << std::endl;
}

void UCIEngine::handleBookStats() {
    if (!openingBook) {
        std::cout << "info string Opening book not available" << std::endl;
        return;
    }

    EnhancedOpeningBook::BookStats stats = openingBook->getStats();
    std::cout << "info string Book Statistics:" << std::endl;
    std::cout << "info string   Positions: " << stats.totalPositions << std::endl;
    std::cout << "info string   Moves: " << stats.totalMoves << std::endl;
    std::cout << "info string   Total Games: " << stats.totalGames << std::endl;
    std::cout << "info string   Average Win Rate: " << stats.averageWinRate << std::endl;
    std::cout << "info string   Average Rating: " << stats.averageRating << std::endl;
}

void UCIEngine::reportBestMove(const std::pair<int, int>& move,
                               const std::pair<int, int>& ponderMove) {
    std::string moveStr = moveToUCI(move);
    std::string ponderStr = (ponderMove.first != -1) ? " ponder " + moveToUCI(ponderMove) : "";
    std::cout << "bestmove " << moveStr << ponderStr << std::endl;
}

void UCIEngine::reportInfo(int depth, int seldepth, int time, int nodes, int nps,
                           const std::vector<std::pair<int, int>>& pv, int score, int hashfull) {
    std::cout << "info depth " << depth;
    if (seldepth > 0)
        std::cout << " seldepth " << seldepth;
    if (time > 0)
        std::cout << " time " << time;
    if (nodes > 0)
        std::cout << " nodes " << nodes;
    if (nps > 0)
        std::cout << " nps " << nps;
    if (hashfull > 0)
        std::cout << " hashfull " << hashfull;
    if (score != 0) {
        std::cout << " score ";
        if (score > 30000) {
            std::cout << "mate " << (30000 - score + 1) / 2;
        } else if (score < -30000) {
            std::cout << "mate " << -(30000 + score + 1) / 2;
        } else {
            std::cout << "cp " << score;
        }
    }
    if (!pv.empty()) {
        std::cout << " pv";
        for (const auto& move : pv) {
            std::cout << " " << moveToUCI(move);
        }
    }
    std::cout << std::endl;
}

void UCIEngine::reportInfo(const std::string& info) {
    std::cout << "info string " << info << std::endl;
}

std::string UCIEngine::moveToUCI(const std::pair<int, int>& move) {
    if (move.first == -1 || move.second == -1) {
        return "0000";
    }

    int fromFile = move.first % 8;
    int fromRank = move.first / 8;
    int toFile = move.second % 8;
    int toRank = move.second / 8;

    std::string result;
    result += static_cast<char>('a' + fromFile);
    result += static_cast<char>('1' + fromRank);
    result += static_cast<char>('a' + toFile);
    result += static_cast<char>('1' + toRank);

    return result;
}

std::pair<int, int> UCIEngine::uciToMove(const std::string& uciMove) {
    if (uciMove.length() != 4) {
        return {-1, -1};
    }

    int fromFile = uciMove[0] - 'a';
    int fromRank = uciMove[1] - '1';
    int toFile = uciMove[2] - 'a';
    int toRank = uciMove[3] - '1';

    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 || toFile < 0 || toFile > 7 ||
        toRank < 0 || toRank > 7) {
        return {-1, -1};
    }

    int from = fromRank * 8 + fromFile;
    int to = toRank * 8 + toFile;

    return {from, to};
}

std::string UCIEngine::boardToFEN(const Board& board) {
    std::string fen;

    for (int rank = 7; rank >= 0; rank--) {
        int emptyCount = 0;
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            const Piece& piece = board.squares[square].piece;

            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }

                char pieceChar;
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        pieceChar = 'p';
                        break;
                    case ChessPieceType::KNIGHT:
                        pieceChar = 'n';
                        break;
                    case ChessPieceType::BISHOP:
                        pieceChar = 'b';
                        break;
                    case ChessPieceType::ROOK:
                        pieceChar = 'r';
                        break;
                    case ChessPieceType::QUEEN:
                        pieceChar = 'q';
                        break;
                    case ChessPieceType::KING:
                        pieceChar = 'k';
                        break;
                    default:
                        pieceChar = '?';
                        break;
                }

                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }
                fen += pieceChar;
            }
        }

        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }

        if (rank > 0) {
            fen += '/';
        }
    }

    fen += (board.turn == ChessPieceColor::WHITE) ? " w " : " b ";

    fen += "KQkq ";

    fen += "- ";

    fen += "0 1";

    return fen;
}

void UCIEngine::parseFEN(const std::string& fen, Board& board) {
    board.InitializeFromFEN(fen);
}

void UCIEngine::parseMoves(const std::string& moves, Board& board) {
    std::istringstream iss(moves);
    std::string move;
    while (iss >> move) {
        std::pair<int, int> movePos = uciToMove(move);
        if (movePos.first != -1 && movePos.second != -1) {
            board.movePiece(movePos.first, movePos.second);
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
        }
    }
}

std::vector<std::pair<int, int>> UCIEngine::getPrincipalVariation(const Board& board, int depth) {
    std::vector<std::pair<int, int>> pv;
    Board tempBoard = board;

    for (int i = 0; i < depth; i++) {
        uint64_t hash = ComputeZobrist(tempBoard);
        TTEntry entry;

        if (TransTable.find(hash, entry) && entry.bestMove.first != -1) {
            pv.push_back(entry.bestMove);

            tempBoard.movePiece(entry.bestMove.first, entry.bestMove.second);
            tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                        : ChessPieceColor::WHITE;
        } else {
            break;
        }
    }

    return pv;
}

void UCIEngine::startSearch() {
    isSearching = true;
    searchStartTime = std::chrono::steady_clock::now();
}

void UCIEngine::stopSearch() {
    isSearching = false;
}

SearchResult UCIEngine::performSearch(const Board& board, int depth, int timeLimit) {
    SearchResult result;

    if (options.useTablebases && tablebase) {
        EndgameTablebase::TablebaseResult tbResult;
        if (tablebase->probe(board, tbResult)) {
            if (!tbResult.bestMoves.empty()) {
                result.bestMove = tbResult.bestMoves[0];
                result.score =
                    tbResult.isExact ? (tbResult.distanceToMate > 0 ? 30000 : -30000) : 0;
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
            if (bookMove.first != -1 && bookMove.second != -1) {
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

    result = iterativeDeepeningParallel(searchBoard, depth, timeLimit, 1, options.contempt, options.multiPV);

    return result;
}

void UCIEngine::setHashSize(int size) {
    options.hashSize = size;
    TransTable.resize(size);
}

void UCIEngine::setThreads(int num) {
    options.threads = num;
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
    if (move.first == -1 || move.second == -1) {
        return "0000";
    }

    int fromFile = move.first % 8;
    int fromRank = move.first / 8;
    int toFile = move.second % 8;
    int toRank = move.second / 8;

    std::string result;
    result += static_cast<char>('a' + fromFile);
    result += static_cast<char>('1' + fromRank);
    result += static_cast<char>('a' + toFile);
    result += static_cast<char>('1' + toRank);

    return result;
}

std::pair<int, int> UCINotation::uciToMove(const std::string& uciMove) {
    if (uciMove.length() != 4) {
        return {-1, -1};
    }

    int fromFile = uciMove[0] - 'a';
    int fromRank = uciMove[1] - '1';
    int toFile = uciMove[2] - 'a';
    int toRank = uciMove[3] - '1';

    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 || toFile < 0 || toFile > 7 ||
        toRank < 0 || toRank > 7) {
        return {-1, -1};
    }

    int from = fromRank * 8 + fromFile;
    int to = toRank * 8 + toFile;

    return {from, to};
}

std::string UCINotation::squareToAlgebraic(int square) {
    if (square < 0 || square > 63)
        return "";

    int file = square % 8;
    int rank = square / 8;

    std::string result;
    result += static_cast<char>('a' + file);
    result += static_cast<char>('1' + rank);

    return result;
}

int UCINotation::algebraicToSquare(const std::string& algebraic) {
    if (algebraic.length() != 2)
        return -1;

    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';

    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;

    return rank * 8 + file;
}

bool UCINotation::isValidUCIMove(const std::string& uciMove) {
    return uciMove.length() == 4 && uciToMove(uciMove).first != -1;
}

std::string UCINotation::getMoveType(const Board& board, const std::pair<int, int>& move) {
    const Piece& piece = board.squares[move.first].piece;
    const Piece& target = board.squares[move.second].piece;

    if (target.PieceType != ChessPieceType::NONE) {
        return "capture";
    }

    if (piece.PieceType == ChessPieceType::KING) {
        int fileDiff = abs((move.second % 8) - (move.first % 8));
        if (fileDiff == 2) {
            return "castling";
        }
    }

    if (piece.PieceType == ChessPieceType::PAWN) {
        int rank = move.second / 8;
        if ((piece.PieceColor == ChessPieceColor::WHITE && rank == 7) ||
            (piece.PieceColor == ChessPieceColor::BLACK && rank == 0)) {
            return "promotion";
        }
    }

    if (piece.PieceType == ChessPieceType::PAWN) {
        int fileDiff = abs((move.second % 8) - (move.first % 8));
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
        if (movePos.first != -1 && movePos.second != -1) {
            board.movePiece(movePos.first, movePos.second);
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
        }
    }
}

std::string UCIPosition::generateFEN(const Board& board) {
    (void)board;

    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

bool UCIPosition::isValidFEN(const std::string& fen) {
    if (fen.empty())
        return false;

    std::istringstream iss(fen);
    std::string token;

    if (!(iss >> token))
        return false;

    int rank = 7, file = 0;
    for (char c : token) {
        if (c == '/') {
            if (file != 8)
                return false;
            rank--;
            file = 0;
            if (rank < 0)
                return false;
        } else if (isdigit(c)) {
            int count = c - '0';
            if (count == 0 || file + count > 8)
                return false;
            file += count;
        } else if (isalpha(c)) {
            if (file >= 8)
                return false;
            char piece = tolower(c);
            if (piece != 'p' && piece != 'n' && piece != 'b' && piece != 'r' && piece != 'q' &&
                piece != 'k') {
                return false;
            }
            file++;
        } else {
            return false;
        }
    }

    if (rank != 0 || file != 8)
        return false;

    if (!(iss >> token))
        return false;
    if (token != "w" && token != "b")
        return false;

    if (!(iss >> token))
        return false;
    for (char c : token) {
        if (c != 'K' && c != 'Q' && c != 'k' && c != 'q' && c != '-') {
            return false;
        }
    }

    if (!(iss >> token))
        return false;
    if (token != "-") {
        if (token.length() != 2)
            return false;
        if (token[0] < 'a' || token[0] > 'h')
            return false;
        if (token[1] != '3' && token[1] != '6')
            return false;
    }

    if (!(iss >> token))
        return false;
    if (!isdigit(token[0]))
        return false;

    if (iss >> token) {
        if (!isdigit(token[0]))
            return false;
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
    if (seldepth > 0)
        oss << " seldepth " << seldepth;
    if (time > 0)
        oss << " time " << time;
    if (nodes > 0)
        oss << " nodes " << nodes;
    if (nps > 0)
        oss << " nps " << nps;
    if (hashfull > 0)
        oss << " hashfull " << hashfull;
    if (score != 0)
        oss << " " << formatScore(score);
    if (!pv.empty())
        oss << " pv" << formatPV(pv);
    return oss.str();
}

std::string UCISearchInfo::formatScore(int score, bool isMate) {
    (void)isMate;
    std::ostringstream oss;
    oss << "score ";
    if (score > 30000) {
        oss << "mate " << (30000 - score + 1) / 2;
    } else if (score < -30000) {
        oss << "mate " << -(30000 + score + 1) / 2;
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
    if (timeMs == 0)
        return "0";
    return std::to_string(nodes * 1000 / timeMs);
}

int runUCIEngine() {
    UCIEngine engine;
    engine.run();
    return 0;
}