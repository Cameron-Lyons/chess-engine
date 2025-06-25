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

int calculateTimeForMove(Board& board, int totalTimeMs, int movesPlayed) {
    // Base time allocation - assume 40 moves per game
    int baseTime = totalTimeMs / std::max(1, 40 - movesPlayed);
    
    // Count material to determine game phase
    int totalMaterial = 0;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType != ChessPieceType::NONE) {
            totalMaterial += board.squares[i].Piece.PieceValue;
        }
    }
    
    // Complexity factors
    float complexityMultiplier = 1.0f;
    
    // Opening: Use less time (book moves)
    if (movesPlayed < 10) {
        complexityMultiplier = 0.5f;
    }
    // Middlegame: Use more time (complex positions)
    else if (totalMaterial > 3000) {
        complexityMultiplier = 1.5f;
    }
    // Endgame: Use more time (precision needed)
    else if (totalMaterial < 1500) {
        complexityMultiplier = 1.3f;
    }
    
    // Check if in check (need more time to find safe moves)
    if (IsKingInCheck(board, board.turn)) {
        complexityMultiplier *= 1.2f;
    }
    
    return static_cast<int>(baseTime * complexityMultiplier);
}

std::pair<int, int> getComputerMove(Board& board, int timeLimitMs = 5000) {
    // Check opening book first
    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        std::cout << "Using opening book move: " << bookMove << "\n";
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            return {srcRow * 8 + srcCol, destRow * 8 + destCol};
        }
    }
    
    // Calculate adaptive time allocation
    static int movesPlayed = 0;
    movesPlayed++;
    int adaptiveTime = calculateTimeForMove(board, timeLimitMs * 10, movesPlayed);
    adaptiveTime = std::min(adaptiveTime, timeLimitMs);
    
    std::cout << "Allocated " << adaptiveTime << "ms for this move\n";
    
    // Use optimized single-threaded search for stability and strength
    std::cout << "Using optimized single-threaded search...\n";
    
    // Significantly increased adaptive depth based on time available and position complexity
    int searchDepth = 8; // Base depth increased
    if (adaptiveTime > 5000) searchDepth = 12;       // 5+ seconds: deep search
    else if (adaptiveTime > 3000) searchDepth = 10;  // 3+ seconds: deeper search
    else if (adaptiveTime > 1500) searchDepth = 9;   // 1.5+ seconds: good depth
    else if (adaptiveTime < 500) searchDepth = 7;    // Under 0.5s: reduce depth
    
    // Complexity-based depth adjustment
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
    int numMoves = moves.size();
    
    // More moves = more complex position = need deeper search
    if (numMoves > 35) searchDepth += 1;  // Very complex position
    else if (numMoves < 15) searchDepth -= 1; // Simple position
    
    // Check if in tactical position (many captures available)
    int numCaptures = 0;
    for (const auto& move : moves) {
        if (isCapture(board, move.first, move.second)) {
            numCaptures++;
        }
    }
    if (numCaptures > 5) searchDepth += 1; // Tactical position needs deeper search
    
    // Ensure minimum and maximum bounds
    searchDepth = std::max(6, std::min(searchDepth, 15));
    
    std::cout << "Search depth: " << searchDepth << " (moves: " << numMoves 
              << ", captures: " << numCaptures << ")\n";
    
    return findBestMove(board, searchDepth);
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