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

// Function to parse algebraic notation (e.g., "e4", "Nf3", "O-O", "exd5")
bool parseAlgebraicMove(const std::string& move, Board& board, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    std::string cleanMove = move;
    // Remove check/checkmate symbols
    if (cleanMove.back() == '+' || cleanMove.back() == '#') {
        cleanMove.pop_back();
    }
    
    // Handle castling
    if (cleanMove == "O-O" || cleanMove == "0-0") {
        // Kingside castling
        if (board.turn == WHITE) {
            srcCol = 4; srcRow = 0; destCol = 6; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 6; destRow = 7;
        }
        return true;
    }
    if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
        // Queenside castling
        if (board.turn == WHITE) {
            srcCol = 4; srcRow = 0; destCol = 2; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 2; destRow = 7;
        }
        return true;
    }
    
    // Parse piece moves
    ChessPieceType pieceType = PAWN;
    size_t startPos = 0;
    int disambigCol = -1, disambigRow = -1;
    
    // Check for piece type
    if (cleanMove.length() > 0) {
        switch (cleanMove[0]) {
            case 'N': pieceType = KNIGHT; startPos = 1; break;
            case 'B': pieceType = BISHOP; startPos = 1; break;
            case 'R': pieceType = ROOK; startPos = 1; break;
            case 'Q': pieceType = QUEEN; startPos = 1; break;
            case 'K': pieceType = KING; startPos = 1; break;
            default: pieceType = PAWN; startPos = 0; break;
        }
    }
    
    // Handle pawn captures (e.g., "exd5")
    if (pieceType == PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';
        
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || 
            srcCol < 0 || srcCol >= 8) return false;
        
        // Find the pawn in the source column
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            Piece& piece = board.squares[pos].Piece;
            
            if (piece.PieceType == PAWN && piece.PieceColor == board.turn) {
                // Check if this pawn can capture to destination
                for (int validDest : piece.ValidMoves) {
                    if (validDest == destRow * 8 + destCol) {
                        srcRow = row;
                        return true;
                    }
                }
            }
        }
        return false;
    }
    
    // Handle piece disambiguation (e.g., "Nbd7")
    if (cleanMove.length() >= 4 && pieceType != PAWN) {
        char secondChar = cleanMove[1];
        if (secondChar >= 'a' && secondChar <= 'h') {
            disambigCol = secondChar - 'a';
            startPos = 2;
        } else if (secondChar >= '1' && secondChar <= '8') {
            disambigRow = secondChar - '1';
            startPos = 2;
        }
    }
    
    // Parse destination
    if (cleanMove.length() < startPos + 2) return false;
    
    // Check if this is a capture move
    if (cleanMove[startPos] == 'x') {
        destCol = cleanMove[startPos + 1] - 'a';
        destRow = cleanMove[startPos + 2] - '1';
    } else {
        destCol = cleanMove[startPos] - 'a';
        destRow = cleanMove[startPos + 1] - '1';
    }
    
    if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) return false;
    
    // Find source piece
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece& piece = board.squares[pos].Piece;
            
            if (piece.PieceType == pieceType && piece.PieceColor == board.turn) {
                // Check disambiguation
                if (disambigCol != -1 && col != disambigCol) continue;
                if (disambigRow != -1 && row != disambigRow) continue;
                
                std::cout << "Debug: Found " << (pieceType == BISHOP ? "bishop" : "piece") 
                          << " at (" << col << "," << row << ") with " 
                          << piece.ValidMoves.size() << " valid moves: ";
                for (int move : piece.ValidMoves) {
                    int moveCol = move % 8;
                    int moveRow = move / 8;
                    std::cout << "(" << moveCol << "," << moveRow << ") ";
                }
                std::cout << "\n";
                
                // Check if this piece can move to destination
                for (int validDest : piece.ValidMoves) {
                    if (validDest == destRow * 8 + destCol) {
                        srcCol = col;
                        srcRow = row;
                        std::cout << "Debug: Found valid move from (" << col << "," << row 
                                  << ") to (" << destCol << "," << destRow << ")\n";
                        return true;
                    }
                }
            }
        }
    }
    
    std::cout << "Debug: No valid piece found for move " << move << "\n";
    return false;
}

// Function to convert chess notation to board coordinates (legacy support)
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
    
    // Generate initial valid moves
    std::cout << "Debug: Generating initial valid moves...\n";
    GenValidMoves(ChessBoard);
    std::cout << "Debug: Initial valid moves generated.\n";
    
    // Debug: Check pawn at e2 (position 12)
    int e2Pos = 12;
    std::cout << "Debug: Pawn at e2 (pos 12): Type=" << ChessBoard.squares[e2Pos].Piece.PieceType 
             << ", Color=" << ChessBoard.squares[e2Pos].Piece.PieceColor 
             << ", ValidMoves count=" << ChessBoard.squares[e2Pos].Piece.ValidMoves.size() << "\n";
    
    // Debug: Check all positions in rank 1 and 2
    std::cout << "Debug: Checking all positions in ranks 1-2:\n";
    for (int row = 0; row <= 1; row++) {
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece& piece = ChessBoard.squares[pos].Piece;
            if (piece.PieceType != NONE) {
                std::cout << "  Pos " << pos << " (" << col << "," << row << "): Type=" << piece.PieceType 
                         << ", Color=" << piece.PieceColor << "\n";
            }
        }
    }
    
    std::string input;
    bool gameOver = false;
    
    while (!gameOver) {
        std::cout << "\nCurrent turn: " << (ChessBoard.turn == WHITE ? "White" : "Black") << "\n";
        
        if (ChessBoard.turn == WHITE) {
            // Human player (White)
            std::cout << "Enter your move (e.g., e4, Nf3, O-O): ";
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                break;
            }
            
            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t\r\n"));
            input.erase(input.find_last_not_of(" \t\r\n") + 1);
            
            std::cout << "Debug: Input received: '" << input << "'\n";
            
            int srcCol, srcRow, destCol, destRow;
            if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
                std::cout << "Debug: Attempting move from (" << srcCol << "," << srcRow << ") to (" << destCol << "," << destRow << ")\n";
                
                // Check if there's a piece at the source position
                int srcPos = srcRow * 8 + srcCol;
                std::cout << "Debug: Source position calculated as: " << srcPos << " (row=" << srcRow << ", col=" << srcCol << ")\n";
                std::cout << "Debug: Piece at source position: Type=" << ChessBoard.squares[srcPos].Piece.PieceType 
                         << ", Color=" << ChessBoard.squares[srcPos].Piece.PieceColor << "\n";
                if (ChessBoard.squares[srcPos].Piece.PieceType == NONE) {
                    std::cout << "Error: No piece at source position!\n";
                    continue;
                }
                
                // Check if it's the player's piece
                if (ChessBoard.squares[srcPos].Piece.PieceColor != WHITE) {
                    std::cout << "Error: That's not your piece!\n";
                    continue;
                }
                
                // Debug: Show valid moves for this piece
                std::cout << "Debug: Valid moves for piece at " << srcCol << "," << srcRow << ": ";
                for (int validDest : ChessBoard.squares[srcPos].Piece.ValidMoves) {
                    int validDestCol = validDest % 8;
                    int validDestRow = validDest / 8;
                    std::cout << "(" << validDestCol << "," << validDestRow << ") ";
                }
                std::cout << "\n";
                
                if (MovePiece(srcCol, srcRow, destCol, destRow)) {
                    std::cout << "Move: " << input << "\n";
                    printBoard(ChessBoard);
                    // Switch turn
                    ChessBoard.turn = (ChessBoard.turn == WHITE) ? BLACK : WHITE;
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
                std::cout << "Invalid format! Use format like 'e4'.\n";
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
                    // Switch turn
                    ChessBoard.turn = (ChessBoard.turn == WHITE) ? BLACK : WHITE;
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