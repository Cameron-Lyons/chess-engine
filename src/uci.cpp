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

UCIEngine::UCIEngine() : isRunning(false), isSearching(false), thinkingTime(5000), depth(15) {
    // Initialize attack tables for bitboard move generation
    initKnightAttacks();
    initKingAttacks();
    
    // Initialize Zobrist hashing for transposition table
    InitZobrist();
    
    // Initialize the board to starting position with proper FEN
    board = Board();
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void UCIEngine::run() {
    isRunning = true;
    std::string input;
    
    while (isRunning && std::getline(std::cin, input)) {
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
    } else if (cmd == "ucinewgame") {
        handleUCINewGame();
    } else if (cmd == "position") {
        handlePosition(command);
    } else if (cmd == "go") {
        handleGo(command);
    } else if (cmd == "stop") {
        handleStop();
    } else if (cmd == "quit") {
        handleQuit();
    } else if (cmd == "debug") {
        // Handle debug command - for now just acknowledge
        std::cout << "info string Debug mode acknowledged" << std::endl;
    } else if (cmd == "setoption") {
        // Handle setoption command - basic implementation
        std::cout << "info string Option setting acknowledged" << std::endl;
    } else {
        // Unknown command
        std::cout << "info string Unknown command: " << cmd << std::endl;
    }
}

void UCIEngine::handleUCI() {
    std::cout << "id name ChessEngine v1.0" << std::endl;
    std::cout << "id author Cameron Lyons" << std::endl;
    
    // Send options
    std::cout << "option name Hash type spin default 64 min 1 max 1024" << std::endl;
    std::cout << "option name Threads type spin default 1 min 1 max 8" << std::endl;
    std::cout << "option name Depth type spin default 15 min 1 max 30" << std::endl;
    std::cout << "option name TimePerMove type spin default 5000 min 100 max 60000" << std::endl;
    
    std::cout << "uciok" << std::endl;
}

void UCIEngine::handleIsReady() {
    std::cout << "readyok" << std::endl;
}

void UCIEngine::handleUCINewGame() {
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
    int movestogo = -1, searchDepth = depth, movetime = -1;
    
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
    int timeForMove = thinkingTime; // default
    
    if (movetime > 0) {
        timeForMove = movetime;
    } else if (board.turn == ChessPieceColor::WHITE && wtime > 0) {
        timeForMove = wtime / std::max(1, (movestogo > 0 ? movestogo : 25)) + winc * 0.8;
    } else if (board.turn == ChessPieceColor::BLACK && btime > 0) {
        timeForMove = btime / std::max(1, (movestogo > 0 ? movestogo : 25)) + binc * 0.8;
    }
    
    // Ensure reasonable time limits
    timeForMove = std::max(100, std::min(timeForMove, 30000));
    
    isSearching = true;
    
    // Synchronize global board state with UCI board (needed for search functions)
    ChessBoard = board;
    GenValidMoves(ChessBoard);  // This updates the global king positions and attack boards
    
    // Run search synchronously for now (can be made async later if needed)
    auto startTime = std::chrono::steady_clock::now();
    
    // Use the same search function as normal mode
    std::pair<int, int> bestMove = findBestMove(ChessBoard, searchDepth);
    
    auto endTime = std::chrono::steady_clock::now();
    int searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // Send final info (using placeholder values since findBestMove doesn't return SearchResult)
    sendInfo(searchDepth, 0, 0, searchTime, moveToUCI(bestMove));
    
    // Send best move
    sendBestMove(bestMove);
    
    isSearching = false;
}

void UCIEngine::handleStop() {
    // Signal search to stop
    // This would need to be integrated with the search function
    std::cout << "info string Search stopped" << std::endl;
}

void UCIEngine::handleQuit() {
    isRunning = false;
    std::cout << "info string Engine shutting down" << std::endl;
}

std::string UCIEngine::moveToUCI(std::pair<int, int> move) {
    if (move.first == -1 || move.second == -1) {
        return "0000"; // null move
    }
    
    int fromFile = move.first % 8;
    int fromRank = move.first / 8;
    int toFile = move.second % 8;
    int toRank = move.second / 8;
    
    std::string uciMove;
    uciMove += static_cast<char>('a' + fromFile);
    uciMove += static_cast<char>('1' + fromRank);
    uciMove += static_cast<char>('a' + toFile);
    uciMove += static_cast<char>('1' + toRank);
    
    // Check for pawn promotion
    const Piece& movingPiece = board.squares[move.first].Piece;
    if (movingPiece.PieceType == ChessPieceType::PAWN) {
        if ((movingPiece.PieceColor == ChessPieceColor::WHITE && toRank == 7) ||
            (movingPiece.PieceColor == ChessPieceColor::BLACK && toRank == 0)) {
            uciMove += "q"; // Default to queen promotion
        }
    }
    
    return uciMove;
}

std::pair<int, int> UCIEngine::uciToMove(const std::string& uciMove) {
    if (uciMove.length() < 4) {
        return {-1, -1};
    }
    
    int fromFile = uciMove[0] - 'a';
    int fromRank = uciMove[1] - '1';
    int toFile = uciMove[2] - 'a';
    int toRank = uciMove[3] - '1';
    
    // Validate coordinates
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile < 0 || toFile > 7 || toRank < 0 || toRank > 7) {
        return {-1, -1};
    }
    
    int fromPos = fromRank * 8 + fromFile;
    int toPos = toRank * 8 + toFile;
    
    return {fromPos, toPos};
}

void UCIEngine::sendBestMove(std::pair<int, int> move) {
    std::string moveStr = moveToUCI(move);
    std::cout << "bestmove " << moveStr << std::endl;
    std::cout.flush();
}

void UCIEngine::sendInfo(int depth, int score, int nodes, int time, const std::string& pv) {
    std::cout << "info";
    
    if (depth > 0) {
        std::cout << " depth " << depth;
    }
    
    if (score != 0) {
        // Convert centipawn score
        if (abs(score) > 9000) {
            // Mate score
            int mateIn = (10000 - abs(score));
            if (score < 0) mateIn = -mateIn;
            std::cout << " score mate " << mateIn;
        } else {
            std::cout << " score cp " << score;
        }
    }
    
    if (nodes > 0) {
        std::cout << " nodes " << nodes;
    }
    
    if (time > 0) {
        std::cout << " time " << time;
        if (nodes > 0 && time > 0) {
            std::cout << " nps " << (nodes * 1000 / std::max(1, time));
        }
    }
    
    if (!pv.empty() && pv != "0000") {
        std::cout << " pv " << pv;
    }
    
    std::cout << std::endl;
    std::cout.flush();
}

// Main function to run UCI engine
int runUCIEngine() {
    UCIEngine engine;
    engine.run();
    return 0;
} 