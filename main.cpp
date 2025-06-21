#include "ChessEngine.h"
#include "MoveContent.h"
#include "search.h"
#include <iostream>
#include <string>
#include <string_view>
#include <chrono>
#include <unordered_map>
#include <cstdint>
#include <map>
#include <thread>
#include "engine_globals.h"
        
using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

PieceMoving MovingPiece(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
PieceMoving MovingPieceSecondary(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
bool PawnPromoted = false;

bool parseMove(std::string_view move, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    if (move.length() < 4) {
        return false;
    }
    srcCol = move[0] - 'a';
    srcRow = move[1] - '1';
    destCol = move[2] - 'a';
    destRow = move[3] - '1';
    bool valid = srcCol >= 0 && srcCol < 8 && srcRow >= 0 && srcRow < 8 &&
           destCol >= 0 && destCol < 8 && destRow >= 0 && destRow < 8;
    return valid;
}

void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    for (int row = 7; row >= 0; --row) {
        std::cout << (row + 1) << " ";
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                std::cout << ". ";
            } else {
                char pieceChar = ' ';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: pieceChar = 'P'; break;
                    case ChessPieceType::KNIGHT: pieceChar = 'N'; break;
                    case ChessPieceType::BISHOP: pieceChar = 'B'; break;
                    case ChessPieceType::ROOK: pieceChar = 'R'; break;
                    case ChessPieceType::QUEEN: pieceChar = 'Q'; break;
                    case ChessPieceType::KING: pieceChar = 'K'; break;
                    default: pieceChar = '?'; break;
                }
                if (piece.PieceColor == ChessPieceColor::BLACK) {
                    pieceChar = tolower(pieceChar);
                }
                std::cout << pieceChar << " ";
            }
        }
        std::cout << (row + 1) << "\n";
    }
    std::cout << "  a b c d e f g h\n";
    std::cout << "Turn: " << (board.turn == ChessPieceColor::WHITE ? "White" : "Black") << "\n";
    
    auto timeSinceLastMove = board.getTimeSinceLastMove();
    std::cout << "Time since last move: " << timeSinceLastMove.count() << "ms\n";
}

std::pair<int, int> getComputerMove(Board& board, int timeLimitMs = 5000) {
    ChessTimePoint startTime = ChessClock::now();
    
    // Use parallel search for better move quality
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4; // Default fallback
    
    std::cout << "Using " << numThreads << " threads for search...\n";
    
    SearchResult result = iterativeDeepeningParallel(board, 10, timeLimitMs, numThreads);
    
    if (result.bestMove.first != -1 && result.bestMove.second != -1) {
        std::cout << "Search completed: depth=" << result.depth 
                  << ", nodes=" << result.nodes 
                  << ", score=" << result.score 
                  << ", time=" << result.timeMs << "ms\n";
        return result.bestMove;
    }
    
    // Fallback to simple move generation if search fails
    std::vector<std::pair<int, int>> validMoves;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceColor == board.turn) {
            switch (piece.PieceType) {
                case ChessPieceType::PAWN: {
                    int row = i / 8;
                    int col = i % 8;
                    
                    if (board.turn == ChessPieceColor::WHITE) {
                        if (row < 7 && board.squares[i + 8].Piece.PieceType == ChessPieceType::NONE) {
                            validMoves.emplace_back(i, i + 8);
                            if (row == 1 && board.squares[i + 16].Piece.PieceType == ChessPieceType::NONE) {
                                validMoves.emplace_back(i, i + 16);
                            }
                        }
                        if (row < 7) {
                            if (col > 0 && board.squares[i + 7].Piece.PieceType != ChessPieceType::NONE && 
                                board.squares[i + 7].Piece.PieceColor == ChessPieceColor::BLACK) {
                                validMoves.emplace_back(i, i + 7);
                            }
                            if (col < 7 && board.squares[i + 9].Piece.PieceType != ChessPieceType::NONE && 
                                board.squares[i + 9].Piece.PieceColor == ChessPieceColor::BLACK) {
                                validMoves.emplace_back(i, i + 9);
                            }
                        }
                    } else {
                        if (row > 0 && board.squares[i - 8].Piece.PieceType == ChessPieceType::NONE) {
                            validMoves.emplace_back(i, i - 8);
                            if (row == 6 && board.squares[i - 16].Piece.PieceType == ChessPieceType::NONE) {
                                validMoves.emplace_back(i, i - 16);
                            }
                        }
                        if (row > 0) {
                            if (col > 0 && board.squares[i - 9].Piece.PieceType != ChessPieceType::NONE && 
                                board.squares[i - 9].Piece.PieceColor == ChessPieceColor::WHITE) {
                                validMoves.emplace_back(i, i - 9);
                            }
                            if (col < 7 && board.squares[i - 7].Piece.PieceType != ChessPieceType::NONE && 
                                board.squares[i - 7].Piece.PieceColor == ChessPieceColor::WHITE) {
                                validMoves.emplace_back(i, i - 7);
                            }
                        }
                    }
                    break;
                }
                case ChessPieceType::KNIGHT: {
                    int row = i / 8;
                    int col = i % 8;
                    int knightMoves[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
                    
                    for (int k = 0; k < 8; k++) {
                        int newRow = row + knightMoves[k][0];
                        int newCol = col + knightMoves[k][1];
                        if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                            int dest = newRow * 8 + newCol;
                            const Piece& destPiece = board.squares[dest].Piece;
                            if (destPiece.PieceType == ChessPieceType::NONE || 
                                destPiece.PieceColor != board.turn) {
                                validMoves.emplace_back(i, dest);
                            }
                        }
                    }
                    break;
                }
                case ChessPieceType::BISHOP:
                case ChessPieceType::ROOK:
                case ChessPieceType::QUEEN: {
                    int row = i / 8;
                    int col = i % 8;
                    int directions[8][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {-1,1}, {1,-1}, {1,1}};
                    int maxDirections = (piece.PieceType == ChessPieceType::BISHOP) ? 4 : 
                                       (piece.PieceType == ChessPieceType::ROOK) ? 4 : 8;
                    int startDir = (piece.PieceType == ChessPieceType::BISHOP) ? 4 : 0;
                    
                    for (int d = startDir; d < startDir + maxDirections; d++) {
                        for (int step = 1; step < 8; step++) {
                            int newRow = row + directions[d][0] * step;
                            int newCol = col + directions[d][1] * step;
                            if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                            
                            int dest = newRow * 8 + newCol;
                            const Piece& destPiece = board.squares[dest].Piece;
                            if (destPiece.PieceType == ChessPieceType::NONE) {
                                validMoves.emplace_back(i, dest);
                            } else {
                                if (destPiece.PieceColor != board.turn) {
                                    validMoves.emplace_back(i, dest);
                                }
                                break;
                            }
                        }
                    }
                    break;
                }
                case ChessPieceType::KING: {
                    int row = i / 8;
                    int col = i % 8;
                    int kingMoves[8][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
                    
                    for (int k = 0; k < 8; k++) {
                        int newRow = row + kingMoves[k][0];
                        int newCol = col + kingMoves[k][1];
                        if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                            int dest = newRow * 8 + newCol;
                            const Piece& destPiece = board.squares[dest].Piece;
                            if (destPiece.PieceType == ChessPieceType::NONE || 
                                destPiece.PieceColor != board.turn) {
                                validMoves.emplace_back(i, dest);
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    
    if (validMoves.empty()) {
        return {-1, -1};
    }
    
    auto currentTime = ChessClock::now();
    if (currentTime - startTime >= std::chrono::milliseconds(timeLimitMs)) {
        return validMoves[0];
    }
    
    std::pair<int, int> bestMove = validMoves[0];
    int bestScore = -10000;
    
    for (const auto& move : validMoves) {
        int score = 0;
        int from = move.first;
        int to = move.second;

        int toRow = to / 8;
        int toCol = to % 8;
        if (toRow >= 3 && toRow <= 4 && toCol >= 3 && toCol <= 4) {
            score += 10;
        }
        
        const Piece& destPiece = board.squares[to].Piece;
        if (destPiece.PieceType != ChessPieceType::NONE) {
            switch (destPiece.PieceType) {
                case ChessPieceType::PAWN: score += 100; break;
                case ChessPieceType::KNIGHT: score += 320; break;
                case ChessPieceType::BISHOP: score += 330; break;
                case ChessPieceType::ROOK: score += 500; break;
                case ChessPieceType::QUEEN: score += 900; break;
                case ChessPieceType::KING: score += 20000; break;
                default: break;
            }
        }
        
        const Piece& fromPiece = board.squares[from].Piece;
        if (fromPiece.PieceType == ChessPieceType::PAWN) {
            if (toRow >= 3 && toRow <= 4 && toCol >= 3 && toCol <= 4) {
                score += 20;
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
    }
    
    return bestMove;
}

std::string positionToNotation(int pos) {
    if (pos < 0 || pos >= 64) return "??";
    int row = pos / 8;
    int col = pos % 8;
    return std::string(1, 'a' + col) + std::to_string(row + 1);
}

std::string to_string(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN: return "Pawn";
        case ChessPieceType::KNIGHT: return "Knight";
        case ChessPieceType::BISHOP: return "Bishop";
        case ChessPieceType::ROOK: return "Rook";
        case ChessPieceType::QUEEN: return "Queen";
        case ChessPieceType::KING: return "King";
        default: return "None";
    }
}

std::string to_string(ChessPieceColor color) {
    return color == ChessPieceColor::WHITE ? "White" : "Black";
}

int main() {
    ChessTimePoint startTime = ChessClock::now();
    
    std::cout << "Chess Engine\n";
    std::cout << "========================\n\n";
    
    ChessBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::string input;
    std::string input2;
    while (true) {
        printBoard(ChessBoard);
        
        std::cout << "\nEnter move (e.g., e4, Nf3, O-O) or 'quit': ";
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        ChessTimePoint moveStartTime = ChessClock::now();
        
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
            int from = srcRow * 8 + srcCol;
            int to = destRow * 8 + destCol;
            
            if (ChessBoard.isValidIndex(from) && ChessBoard.isValidIndex(to)) {
                PrevBoard = ChessBoard;
                
                if (ChessBoard.movePiece(from, to)) {
                    ChessBoard.recordMoveTime();
                    
                    MoveHistory.push(from);
                    MoveHistory.push(to);
                    
                    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? 
                                     ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                    
                    // UpdateValidMoves(ChessBoard);
                    
                    if (ChessBoard.turn == ChessPieceColor::BLACK) {
                        std::cout << "\nComputer is thinking...\n";
                        
                        ChessTimePoint computerStartTime = ChessClock::now();
                        auto computerMove = getComputerMove(ChessBoard, 3000);
                        auto computerTime = ChessClock::now() - computerStartTime;
                        
                        std::cout << "Debug: Computer move result: from=" << computerMove.first 
                                  << " to=" << computerMove.second << "\n";
                        
                        if (computerMove.first != -1 && computerMove.second != -1) {
                            PrevBoard = ChessBoard;
                            
                            if (ChessBoard.movePiece(computerMove.first, computerMove.second)) {
                                ChessBoard.recordMoveTime();
                                
                                MoveHistory.push(computerMove.first);
                                MoveHistory.push(computerMove.second);
                                
                                ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? 
                                                 ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                                
                                std::cout << "Computer played: " 
                                          << positionToNotation(computerMove.first) 
                                          << " to " 
                                          << positionToNotation(computerMove.second)
                                          << " (took " << std::chrono::duration_cast<ChessDuration>(computerTime).count() << "ms)\n";
                            } else {
                                std::cout << "Computer move failed!\n";
                            }
                        } else {
                            std::cout << "Computer couldn't find a valid move!\n";
                        }
                    }
                } else {
                    std::cout << "Invalid move!\n";
                }
            } else {
                std::cout << "Invalid position!\n";
            }
        } else {
            if (input.length() == 2 && input[0] >= 'a' && input[0] <= 'h' && input[1] >= '1' && input[1] <= '8') {
                int destCol = input[0] - 'a';
                int destRow = input[1] - '1';
                
                int srcCol = destCol;
                int srcRow = -1;
                
                for (int row = 0; row < 8; row++) {
                    int pos = row * 8 + srcCol;
                    const Piece& piece = ChessBoard.squares[pos].Piece;
                    if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == ChessBoard.turn) {
                        if ((ChessBoard.turn == ChessPieceColor::WHITE && destRow == row + 1) ||
                            (ChessBoard.turn == ChessPieceColor::BLACK && destRow == row - 1)) {
                            srcRow = row;
                            break;
                        }
                        if ((ChessBoard.turn == ChessPieceColor::WHITE && destRow == row + 2 && row == 1) ||
                            (ChessBoard.turn == ChessPieceColor::BLACK && destRow == row - 2 && row == 6)) {
                            srcRow = row;
                            break;
                        }
                    }
                }
                
                if (srcRow != -1) {
                    int from = srcRow * 8 + srcCol;
                    int to = destRow * 8 + destCol;
                    
                    PrevBoard = ChessBoard;
                    
                    if (ChessBoard.movePiece(from, to)) {
                        ChessBoard.recordMoveTime();
                        
                        MoveHistory.push(from);
                        MoveHistory.push(to);
                        
                        ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? 
                                         ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                        
                        std::cout << "Move " << input << " executed successfully!\n";
                        
                        if (ChessBoard.turn == ChessPieceColor::BLACK) {
                            std::cout << "\nComputer is thinking...\n";
                            
                            ChessTimePoint computerStartTime = ChessClock::now();
                            auto computerMove = getComputerMove(ChessBoard, 3000);
                            auto computerTime = ChessClock::now() - computerStartTime;
                            
                            std::cout << "Debug: Computer move result: from=" << computerMove.first 
                                      << " to=" << computerMove.second << "\n";
                            
                            if (computerMove.first != -1 && computerMove.second != -1) {
                                PrevBoard = ChessBoard;
                                
                                if (ChessBoard.movePiece(computerMove.first, computerMove.second)) {
                                    ChessBoard.recordMoveTime();
                                    
                                    MoveHistory.push(computerMove.first);
                                    MoveHistory.push(computerMove.second);
                                    
                                    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? 
                                                     ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                                    
                                    std::cout << "Computer played: " 
                                              << positionToNotation(computerMove.first) 
                                              << " to " 
                                              << positionToNotation(computerMove.second)
                                              << " (took " << std::chrono::duration_cast<ChessDuration>(computerTime).count() << "ms)\n";
                                } else {
                                    std::cout << "Computer move failed!\n";
                                }
                            } else {
                                std::cout << "Computer couldn't find a valid move!\n";
                            }
                        }
                    } else {
                        std::cout << "Invalid move!\n";
                    }
                } else {
                    std::cout << "No valid pawn found for move " << input << "\n";
                }
            } else {
                std::cout << "Invalid move format! Use algebraic notation (e.g., e4, Nf3, O-O)\n";
            }
        }
        
        auto moveTime = ChessClock::now() - moveStartTime;
        std::cout << "Move completed in " << std::chrono::duration_cast<ChessDuration>(moveTime).count() << "ms\n";
    }
    
    auto totalTime = ChessClock::now() - startTime;
    std::cout << "\nGame completed in " << std::chrono::duration_cast<ChessDuration>(totalTime).count() << "ms\n";
    std::cout << "Thanks for playing!\n";
    
    return 0;
} 