#include "uci.h"
#include "search.h"
#include "Evaluation.h"
#include "ValidMoves.h"
#include "BitboardMoves.h"
#include "engine_globals.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <future>
#include <chrono>

// Forward declarations for global variables
extern Board ChessBoard;
extern Board PrevBoard;

UCIEngine::UCIEngine() : isSearching(false) {
    // Initialize attack tables for bitboard move generation
    initKnightAttacks();
    initKingAttacks();
    
    // Initialize Zobrist hashing for transposition table
    InitZobrist();
    
    // Initialize the board to starting position with proper FEN
    board = Board();
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    // Initialize components
    nnEvaluator = std::make_unique<NeuralNetworkEvaluator>();
    tablebase = std::make_unique<EndgameTablebase>();
    openingBook = std::make_unique<EnhancedOpeningBook>();
    
    // Initialize time manager with default settings
    TimeManager::TimeControl tc;
    tc.baseTime = 300000; // 5 minutes
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
    } else {
        // Unknown command
        std::cout << "info string Unknown command: " << cmd << std::endl;
    }
}

void UCIEngine::handleUCI() {
    std::cout << "id name ModernChess v2.0" << std::endl;
    std::cout << "id author Chess Engine Team" << std::endl;
    
    // Send options
    std::cout << "option name Hash type spin default 32 min 1 max 1024" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 16" << std::endl;
    std::cout << "option name MultiPV type spin default 1 min 1 max 10" << std::endl;
    std::cout << "option name Ponder type check default false" << std::endl;
    std::cout << "option name OwnBook type check default true" << std::endl;
    std::cout << "option name Move Overhead type spin default 10 min 0 max 5000" << std::endl;
    std::cout << "option name Minimum Thinking Time type spin default 20 min 0 max 5000" << std::endl;
    std::cout << "option name Use Neural Network type check default true" << std::endl;
    std::cout << "option name Neural Network Weight type spin default 70 min 0 max 100" << std::endl;
    std::cout << "option name Use Tablebases type check default true" << std::endl;
    std::cout << "option name Debug type check default false" << std::endl;
    std::cout << "option name Show Current Line type check default false" << std::endl;
    
    std::cout << "uciok" << std::endl;
}

void UCIEngine::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCIEngine::handleSetOption(const std::string& command) {
    std::istringstream iss(command);
    std::string word;
    iss >> word; // consume "setoption"
    
    std::string name, value;
    if (iss >> word && word == "name") {
        std::getline(iss, name);
        // Remove leading space
        if (!name.empty() && name[0] == ' ') {
            name = name.substr(1);
        }
        
        if (iss >> word && word == "value") {
            std::getline(iss, value);
            if (!value.empty() && value[0] == ' ') {
                value = value.substr(1);
            }
            
            // Handle different options
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
            }
        }
    }
}

void UCIEngine::handleNewGame() {
    // Reset the board to starting position
    board = Board();
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    TransTable.clear();
    std::cout << "info string New game started" << std::endl;
}

void UCIEngine::handlePosition(const std::string& command) {
    std::istringstream iss(command);
    std::string word;
    iss >> word; // consume "position"
    
    if (!(iss >> word)) {
        std::cout << "info string Error: Invalid position command" << std::endl;
        return;
    }
    
    if (word == "startpos") {
        // Reset to starting position
        board = Board();
        board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    } else if (word == "fen") {
        // Parse FEN string
        std::string fen;
        for (int i = 0; i < 6; i++) { // FEN has 6 parts
            std::string part;
            if (iss >> part) {
                if (i > 0) fen += " ";
                fen += part;
            }
        }
        
        if (!fen.empty()) {
            // Initialize board with the provided FEN
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
    
    // Check for moves
    if (iss >> word && word == "moves") {
        std::string move;
        while (iss >> move) {
            std::pair<int, int> movePos = uciToMove(move);
            if (movePos.first != -1 && movePos.second != -1) {
                // Validate and make the move
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
                    board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
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
    iss >> word; // consume "go"
    
    int wtime = -1, btime = -1, winc = 0, binc = 0;
    int movestogo = -1, searchDepth = 8, movetime = -1;
    
    // Parse go command parameters
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
    
    // Calculate time for this move
    int timeForMove = 5000; // default 5 seconds
    
    if (movetime > 0) {
        timeForMove = movetime;
    } else if (wtime > 0 || btime > 0) {
        int currentTime = (board.turn == ChessPieceColor::WHITE) ? wtime : btime;
        int increment = (board.turn == ChessPieceColor::WHITE) ? winc : binc;
        
        if (currentTime > 0) {
            timeForMove = currentTime / 30; // Simple time management
            if (increment > 0) {
                timeForMove += increment;
            }
        }
    }
    
    // Start search in a separate thread
    isSearching = true;
    searchStartTime = std::chrono::steady_clock::now();
    searchTimeLimit = timeForMove;
    searchDepthLimit = searchDepth;
    
    std::thread searchThread([this, searchDepth, timeForMove]() {
        try {
            SearchResult result = performSearch(board, searchDepth, timeForMove);
            
            if (isSearching) {
                reportBestMove(result.bestMove);
                reportInfo(result.depth, result.depth, result.timeMs, result.nodes, 
                          result.nodes * 1000 / result.timeMs, {}, result.score, 0);
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
    (void)command; // Suppress unused parameter warning
    // Handle debug command
    std::cout << "info string Debug mode: " << (options.debug ? "enabled" : "disabled") << std::endl;
}

void UCIEngine::handleRegister(const std::string& command) {
    (void)command; // Suppress unused parameter warning
    // Handle register command
    std::cout << "info string Registration not required" << std::endl;
}

void UCIEngine::handleInfo(const std::string& command) {
    (void)command; // Suppress unused parameter warning
    // Handle info command
    std::cout << "info string Info command received" << std::endl;
}

void UCIEngine::reportBestMove(const std::pair<int, int>& move, const std::pair<int, int>& ponderMove) {
    std::string moveStr = moveToUCI(move);
    std::string ponderStr = (ponderMove.first != -1) ? " ponder " + moveToUCI(ponderMove) : "";
    std::cout << "bestmove " << moveStr << ponderStr << std::endl;
}

void UCIEngine::reportInfo(int depth, int seldepth, int time, int nodes, int nps, 
                          const std::vector<std::pair<int, int>>& pv, int score, int hashfull) {
    std::cout << "info depth " << depth;
    if (seldepth > 0) std::cout << " seldepth " << seldepth;
    if (time > 0) std::cout << " time " << time;
    if (nodes > 0) std::cout << " nodes " << nodes;
    if (nps > 0) std::cout << " nps " << nps;
    if (hashfull > 0) std::cout << " hashfull " << hashfull;
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
    
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
        return {-1, -1};
    }
    
    int from = fromRank * 8 + fromFile;
    int to = toRank * 8 + toFile;
    
    return {from, to};
}

std::string UCIEngine::boardToFEN(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // Simple FEN generation - this would need to be implemented properly
    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
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
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        }
    }
}

std::vector<std::pair<int, int>> UCIEngine::getPrincipalVariation(const Board& board, int depth) {
    (void)board; // Suppress unused parameter warning
    (void)depth; // Suppress unused parameter warning
    // This would need to be implemented to return the principal variation
    return {};
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
    
    // Create a copy of the board for search
    Board searchBoard = board;
    
    // Use the existing search function with default parameters
    result = iterativeDeepeningParallel(searchBoard, depth, timeLimit, 1);
    
    return result;
}

// Option management functions
void UCIEngine::setHashSize(int size) {
    options.hashSize = size;
    // Resize transposition table
    TransTable.clear();
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

// UCI utility functions
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
    
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
        return {-1, -1};
    }
    
    int from = fromRank * 8 + fromFile;
    int to = toRank * 8 + toFile;
    
    return {from, to};
}

std::string UCINotation::squareToAlgebraic(int square) {
    if (square < 0 || square > 63) return "";
    
    int file = square % 8;
    int rank = square / 8;
    
    std::string result;
    result += static_cast<char>('a' + file);
    result += static_cast<char>('1' + rank);
    
    return result;
}

int UCINotation::algebraicToSquare(const std::string& algebraic) {
    if (algebraic.length() != 2) return -1;
    
    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';
    
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return -1;
    
    return rank * 8 + file;
}

bool UCINotation::isValidUCIMove(const std::string& uciMove) {
    return uciMove.length() == 4 && uciToMove(uciMove).first != -1;
}

std::string UCINotation::getMoveType(const Board& board, const std::pair<int, int>& move) {
    (void)board; // Suppress unused parameter warning
    (void)move;  // Suppress unused parameter warning
    // This would need to be implemented to determine move type
    return "normal";
}

// UCI position parsing
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
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        }
    }
}

std::string UCIPosition::generateFEN(const Board& board) {
    (void)board; // Suppress unused parameter warning
    // This would need to be implemented properly
    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

bool UCIPosition::isValidFEN(const std::string& fen) {
    (void)fen; // Suppress unused parameter warning
    // This would need to be implemented to validate FEN
    return true;
}

uint64_t UCIPosition::getPositionHash(const Board& board) {
    return ComputeZobrist(board);
}

// UCI search info formatting
std::string UCISearchInfo::formatInfo(int depth, int seldepth, int time, int nodes, int nps,
                                     const std::vector<std::pair<int, int>>& pv, int score, int hashfull) {
    std::ostringstream oss;
    oss << "info depth " << depth;
    if (seldepth > 0) oss << " seldepth " << seldepth;
    if (time > 0) oss << " time " << time;
    if (nodes > 0) oss << " nodes " << nodes;
    if (nps > 0) oss << " nps " << nps;
    if (hashfull > 0) oss << " hashfull " << hashfull;
    if (score != 0) oss << " " << formatScore(score);
    if (!pv.empty()) oss << " pv" << formatPV(pv);
    return oss.str();
}

std::string UCISearchInfo::formatScore(int score, bool isMate) {
    (void)isMate; // Suppress unused parameter warning
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
    if (timeMs == 0) return "0";
    return std::to_string(nodes * 1000 / timeMs);
}

// Main function for UCI engine
int runUCIEngine() {
    UCIEngine engine;
    engine.run();
    return 0;
} 