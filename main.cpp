#include "ChessEngine.h"
#include "ValidMoves.h"
#include "MoveContent.h"
#include "search.h"
#include <iostream>
#include <string>
#include <algorithm>

// Global variable definitions
Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

// Global variables for attack boards
bool BlackAttackBoard[64];
bool WhiteAttackBoard[64];
int BlackKingPosition;
int WhiteKingPosition;

// Global variables for move tracking
PieceMoving MovingPiece(WHITE, PAWN, false);
PieceMoving MovingPieceSecondary(WHITE, PAWN, false);
bool PawnPromoted = false;

// Function to convert chess notation to board coordinates
bool parseMove(const std::string& move, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    if (move.length() < 4) {
        std::cout << "Debug: Move too short: " << move << " (length: " << move.length() << ")\n";
        return false;
    }
    
    srcCol = move[0] - 'a';
    srcRow = move[1] - '1';
    destCol = move[2] - 'a';
    destRow = move[3] - '1';
    
    std::cout << "Debug: Parsed move " << move << " -> src(" << srcCol << "," << srcRow << ") dest(" << destCol << "," << destRow << ")\n";
    
    bool valid = srcCol >= 0 && srcCol < 8 && srcRow >= 0 && srcRow < 8 &&
           destCol >= 0 && destCol < 8 && destRow >= 0 && destRow < 8;
    
    if (!valid) {
        std::cout << "Debug: Invalid coordinates\n";
    }
    
    return valid;
}

// Function to print the board
void printBoard(const Board& board) {
    std::cout << "  a b c d e f g h\n";
    std::cout << "  ---------------\n";
    
    for (int row = 7; row >= 0; row--) {
        std::cout << row + 1 << " ";
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece piece = board.squares[pos].Piece;
            
            char symbol = '.';
            if (piece.PieceType != NONE) {
                switch (piece.PieceType) {
                    case PAWN: symbol = piece.PieceColor == WHITE ? 'P' : 'p'; break;
                    case KNIGHT: symbol = piece.PieceColor == WHITE ? 'N' : 'n'; break;
                    case BISHOP: symbol = piece.PieceColor == WHITE ? 'B' : 'b'; break;
                    case ROOK: symbol = piece.PieceColor == WHITE ? 'R' : 'r'; break;
                    case QUEEN: symbol = piece.PieceColor == WHITE ? 'Q' : 'q'; break;
                    case KING: symbol = piece.PieceColor == WHITE ? 'K' : 'k'; break;
                    default: symbol = '.'; break;
                }
            }
            std::cout << symbol << " ";
        }
        std::cout << row + 1 << "\n";
    }
    std::cout << "  ---------------\n";
    std::cout << "  a b c d e f g h\n";
}

// Function to get computer move using alpha-beta search
std::pair<int, int> getComputerMove(Board& board, ChessPieceColor color, int depth = 3) {
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, color);
    
    if (moves.empty()) {
        return {-1, -1}; // No moves available
    }
    
    int bestScore = (color == WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = moves[0];
    
    for (const auto& move : moves) {
        Board newBoard = board;
        newBoard.MovePiece(newBoard, move.first, move.second, false);
        
        int score = AlphaBetaSearch(newBoard, depth - 1, -10000, 10000, color == BLACK);
        
        if (color == WHITE) {
            if (score > bestScore) {
                bestScore = score;
                bestMove = move;
            }
        } else {
            if (score < bestScore) {
                bestScore = score;
                bestMove = move;
            }
        }
    }
    
    return bestMove;
}

// Function to convert board position to chess notation
std::string positionToNotation(int pos) {
    int col = pos % 8;
    int row = pos / 8;
    return std::string(1, 'a' + col) + std::string(1, '1' + row);
}

int main() {
    std::cout << "Chess Engine v1.0\n";
    std::cout << "================\n\n";
    
    // Initialize the engine
    Engine();
    
    std::cout << "Initial board position:\n";
    printBoard(ChessBoard);
    
    std::string input;
    bool gameOver = false;
    
    while (!gameOver) {
        std::cout << "\nCurrent turn: " << (ChessBoard.turn == WHITE ? "White" : "Black") << "\n";
        
        if (ChessBoard.turn == WHITE) {
            // Human player (White)
            std::cout << "Enter your move (e.g., e2e4): ";
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                break;
            }
            
            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t\r\n"));
            input.erase(input.find_last_not_of(" \t\r\n") + 1);
            
            std::cout << "Debug: Input received: '" << input << "'\n";
            
            int srcCol, srcRow, destCol, destRow;
            if (parseMove(input, srcCol, srcRow, destCol, destRow)) {
                std::cout << "Debug: Attempting move from (" << srcCol << "," << srcRow << ") to (" << destCol << "," << destRow << ")\n";
                
                // Check if there's a piece at the source position
                int srcPos = srcRow * 8 + srcCol;
                if (ChessBoard.squares[srcPos].Piece.PieceType == NONE) {
                    std::cout << "Error: No piece at source position!\n";
                    continue;
                }
                
                // Check if it's the player's piece
                if (ChessBoard.squares[srcPos].Piece.PieceColor != WHITE) {
                    std::cout << "Error: That's not your piece!\n";
                    continue;
                }
                
                if (MovePiece(srcCol, srcRow, destCol, destRow)) {
                    std::cout << "Move: " << input << "\n";
                    printBoard(ChessBoard);
                    
                    // Check for game end conditions
                    bool blackMate = false, whiteMate = false, staleMate = false;
                    if (SearchForMate(BLACK, ChessBoard, blackMate, whiteMate, staleMate)) {
                        if (whiteMate) {
                            std::cout << "Checkmate! Black wins!\n";
                            gameOver = true;
                        } else if (staleMate) {
                            std::cout << "Stalemate! Game is a draw.\n";
                            gameOver = true;
                        }
                    }
                } else {
                    std::cout << "Invalid move! Try again.\n";
                    continue;
                }
            } else {
                std::cout << "Invalid format! Use format like 'e2e4'.\n";
                continue;
            }
        } else {
            // Computer player (Black)
            std::cout << "Computer is thinking...\n";
            
            std::pair<int, int> computerMove = getComputerMove(ChessBoard, BLACK, 3);
            
            if (computerMove.first == -1) {
                std::cout << "Computer has no valid moves!\n";
                gameOver = true;
            } else {
                int srcCol = computerMove.first % 8;
                int srcRow = computerMove.first / 8;
                int destCol = computerMove.second % 8;
                int destRow = computerMove.second / 8;
                
                std::string moveNotation = positionToNotation(computerMove.first) + 
                                         positionToNotation(computerMove.second);
                
                if (MovePiece(srcCol, srcRow, destCol, destRow)) {
                    std::cout << "Computer move: " << moveNotation << "\n";
                    printBoard(ChessBoard);
                    
                    // Check for game end conditions
                    bool blackMate = false, whiteMate = false, staleMate = false;
                    if (SearchForMate(WHITE, ChessBoard, blackMate, whiteMate, staleMate)) {
                        if (blackMate) {
                            std::cout << "Checkmate! White wins!\n";
                            gameOver = true;
                        } else if (staleMate) {
                            std::cout << "Stalemate! Game is a draw.\n";
                            gameOver = true;
                        }
                    }
                }
            }
        }
    }
    
    std::cout << "\nGame ended. Thanks for playing!\n";
    return 0;
} 