#include "ChessEngine.h"
#include "ValidMoves.h"
#include "MoveContent.h"
#include "search.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <map>

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
PieceMoving MovingPiece(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
PieceMoving MovingPieceSecondary(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
bool PawnPromoted = false;

// Zobrist table definitions
uint64_t ZobristTable[64][12];
uint64_t ZobristBlackToMove;

// Transposition table definition
std::unordered_map<uint64_t, TTEntry> TransTable;

// Opening book: maps FEN (without move clocks) to best move in coordinate notation (e.g., "e2e4")
std::map<std::string, std::string> OpeningBook = {
    // Initial position
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"},
    // After 1.e4
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "e7e5"},
    // After 1.e4 e5
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", "g1f3"},
    // After 1.e4 e5 2.Nf3
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", "b8c6"},
    // After 1.e4 e5 2.Nf3 Nc6
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "f1c4"},
    // After 1.e4 e5 2.Nf3 Nc6 3.Bc4
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", "f8c5"},
    // Add more lines as desired
};

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
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 6; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 6; destRow = 7;
        }
        return true;
    }
    if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
        // Queenside castling
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 2; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 2; destRow = 7;
        }
        return true;
    }
    
    // Parse piece moves
    ChessPieceType pieceType = ChessPieceType::PAWN;
    size_t startPos = 0;
    int disambigCol = -1, disambigRow = -1;
    
    // Check for piece type
    if (cleanMove.length() > 0) {
        switch (cleanMove[0]) {
            case 'N': pieceType = ChessPieceType::KNIGHT; startPos = 1; break;
            case 'B': pieceType = ChessPieceType::BISHOP; startPos = 1; break;
            case 'R': pieceType = ChessPieceType::ROOK; startPos = 1; break;
            case 'Q': pieceType = ChessPieceType::QUEEN; startPos = 1; break;
            case 'K': pieceType = ChessPieceType::KING; startPos = 1; break;
            default: pieceType = ChessPieceType::PAWN; startPos = 0; break;
        }
    }
    
    // Handle pawn captures (e.g., "exd5")
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';
        
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || 
            srcCol < 0 || srcCol >= 8) return false;
        
        // Find the pawn in the source column
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            Piece& piece = board.squares[pos].Piece;
            
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
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
    if (cleanMove.length() >= 4 && pieceType != ChessPieceType::PAWN) {
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
                
                // Check if this piece can move to destination
                for (int validDest : piece.ValidMoves) {
                    if (validDest == destRow * 8 + destCol) {
                        srcCol = col;
                        srcRow = row;
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

// Function to convert chess notation to board coordinates (legacy support)
bool parseMove(const std::string& move, int& srcCol, int& srcRow, int& destCol, int& destRow) {
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
            if (piece.PieceType != ChessPieceType::NONE) {
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p'; break;
                    case ChessPieceType::KNIGHT: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n'; break;
                    case ChessPieceType::BISHOP: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b'; break;
                    case ChessPieceType::ROOK: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r'; break;
                    case ChessPieceType::QUEEN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q'; break;
                    case ChessPieceType::KING: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k'; break;
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

// Function to get computer move using iterative deepening with time management
std::pair<int, int> getComputerMove(Board& board, int timeLimitMs = 5000) {
    // Use iterative deepening with time management
    SearchResult result = iterativeDeepening(board, 10, timeLimitMs); // Max depth 10
    
    if (result.bestMove.first == -1) {
        return {-1, -1}; // No moves available
    }
    
    return result.bestMove;
}

// Function to convert board position to chess notation
std::string positionToNotation(int pos) {
    int col = pos % 8;
    int row = pos / 8;
    return std::string(1, 'a' + col) + std::string(1, '1' + row);
}

// Function to get the current FEN (without move clocks)
std::string getFEN(const Board& board) {
    // Piece placement
    std::string fen;
    for (int row = 7; row >= 0; --row) {
        int empty = 0;
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                ++empty;
            } else {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                char symbol = '.';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:   symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p'; break;
                    case ChessPieceType::KNIGHT: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n'; break;
                    case ChessPieceType::BISHOP: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b'; break;
                    case ChessPieceType::ROOK:   symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r'; break;
                    case ChessPieceType::QUEEN:  symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q'; break;
                    case ChessPieceType::KING:   symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k'; break;
                    default: symbol = '.'; break;
                }
                fen += symbol;
            }
        }
        if (empty > 0) fen += std::to_string(empty);
        if (row > 0) fen += '/';
    }
    // Active color
    fen += board.turn == ChessPieceColor::WHITE ? " w " : " b ";
    // Castling rights (not fully implemented, just placeholder)
    fen += "KQkq ";
    // En passant (not tracked, just '-')
    fen += "- ";
    // Move clocks (not tracked, just '0 1')
    fen += "0 1";
    return fen;
}

std::string to_string(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN: return "PAWN";
        case ChessPieceType::KNIGHT: return "KNIGHT";
        case ChessPieceType::BISHOP: return "BISHOP";
        case ChessPieceType::ROOK: return "ROOK";
        case ChessPieceType::QUEEN: return "QUEEN";
        case ChessPieceType::KING: return "KING";
        case ChessPieceType::NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

std::string to_string(ChessPieceColor color) {
    switch (color) {
        case ChessPieceColor::WHITE: return "WHITE";
        case ChessPieceColor::BLACK: return "BLACK";
        default: return "UNKNOWN";
    }
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
    std::cout << "Debug: Pawn at e2 (pos 12): Type=" << to_string(ChessBoard.squares[e2Pos].Piece.PieceType)
             << ", Color=" << to_string(ChessBoard.squares[e2Pos].Piece.PieceColor)
             << ", ValidMoves count=" << ChessBoard.squares[e2Pos].Piece.ValidMoves.size() << "\n";
    
    // Debug: Check all positions in rank 1 and 2
    std::cout << "Debug: Checking all positions in ranks 1-2:\n";
    for (int row = 0; row <= 1; row++) {
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece& piece = ChessBoard.squares[pos].Piece;
            if (piece.PieceType != ChessPieceType::NONE) {
                std::cout << "  Pos " << pos << " (" << col << "," << row << "): Type=" << to_string(piece.PieceType)
                          << ", Color=" << to_string(piece.PieceColor) << "\n";
            }
        }
    }
    
    std::string input;
    bool gameOver = false;
    
    while (!gameOver) {
        std::cout << "\nCurrent turn: " << (ChessBoard.turn == ChessPieceColor::WHITE ? "White" : "Black") << "\n";
        
        if (ChessBoard.turn == ChessPieceColor::WHITE) {
            // Human player (White)
            std::cout << "Enter your move (e.g., e4, Nf3, O-O): ";
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                break;
            }
            
            // Trim whitespace
            input.erase(0, input.find_first_not_of(" \t\r\n"));
            input.erase(input.find_last_not_of(" \t\r\n") + 1);
            
            int srcCol, srcRow, destCol, destRow;
            if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
                if (MovePiece(srcCol, srcRow, destCol, destRow)) {
                    std::cout << "Move: " << input << "\n";
                    printBoard(ChessBoard);
                    // Switch turn
                    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                    // Check for game end conditions
                    bool blackMate = false, whiteMate = false, staleMate = false;
                    if (SearchForMate(ChessPieceColor::BLACK, ChessBoard, blackMate, whiteMate, staleMate)) {
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
            
            std::pair<int, int> computerMove = getComputerMove(ChessBoard);
            
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
                    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                    // Check for game end conditions
                    bool blackMate = false, whiteMate = false, staleMate = false;
                    if (SearchForMate(ChessPieceColor::WHITE, ChessBoard, blackMate, whiteMate, staleMate)) {
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