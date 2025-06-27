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
    // More generous time allocation - assume 50 moves per game with extra buffer
    int baseTime = totalTimeMs / std::max(1, 50 - movesPlayed);
    
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
    // Middlegame: Use significantly more time (complex positions)
    else if (totalMaterial > 3000) {
        complexityMultiplier = 2.0f;  // Increased from 1.5f
    }
    // Endgame: Use more time (precision needed)
    else if (totalMaterial < 1500) {
        complexityMultiplier = 1.8f;  // Increased from 1.3f
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
    
    // DRAMATICALLY increased adaptive depth for superior tactical vision
    int searchDepth = 10; // Base depth significantly increased
    if (adaptiveTime > 8000) searchDepth = 16;       // 8+ seconds: very deep search
    else if (adaptiveTime > 5000) searchDepth = 14;  // 5+ seconds: deep search
    else if (adaptiveTime > 3000) searchDepth = 12;  // 3+ seconds: deeper search
    else if (adaptiveTime > 1500) searchDepth = 11;  // 1.5+ seconds: good depth
    else if (adaptiveTime < 500) searchDepth = 9;    // Under 0.5s: still decent depth
    
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
    
    // Ensure minimum and maximum bounds - significantly increased
    searchDepth = std::max(9, std::min(searchDepth, 20));
    
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

// Enhanced game state detection with appropriate announcements
enum class GameState {
    ONGOING,
    CHECKMATE_WHITE_WINS,
    CHECKMATE_BLACK_WINS, 
    STALEMATE,
    DRAW_INSUFFICIENT_MATERIAL
};

GameState checkGameState(Board& board) {
    ChessPieceColor currentPlayer = board.turn;
    
    // Generate all valid moves for current player
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, currentPlayer);
    
    // Filter moves to only include legal moves (not leaving king in check)
    std::vector<std::pair<int, int>> legalMoves;
    for (const auto& move : moves) {
        Board testBoard = board;
        if (testBoard.movePiece(move.first, move.second)) {
            // Check if this move leaves our king in check (illegal)
            if (!IsKingInCheck(testBoard, currentPlayer)) {
                legalMoves.push_back(move);
            }
        }
    }
    
    bool isInCheck = IsKingInCheck(board, currentPlayer);
    
    // No legal moves available
    if (legalMoves.empty()) {
        if (isInCheck) {
            // Checkmate!
            if (currentPlayer == ChessPieceColor::WHITE) {
                return GameState::CHECKMATE_BLACK_WINS;
            } else {
                return GameState::CHECKMATE_WHITE_WINS;
            }
        } else {
            // Stalemate!
            return GameState::STALEMATE;
        }
    }
    
    // Check for insufficient material draw
    std::vector<ChessPieceType> whitePieces, blackPieces;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whitePieces.push_back(piece.PieceType);
            } else {
                blackPieces.push_back(piece.PieceType);
            }
        }
    }
    
    // Check for insufficient material patterns
    bool insufficientMaterial = false;
    
    // King vs King
    if (whitePieces.empty() && blackPieces.empty()) {
        insufficientMaterial = true;
    }
    // King and Bishop vs King, King and Knight vs King
    else if ((whitePieces.size() == 1 && blackPieces.empty() && 
              (whitePieces[0] == ChessPieceType::BISHOP || whitePieces[0] == ChessPieceType::KNIGHT)) ||
             (blackPieces.size() == 1 && whitePieces.empty() && 
              (blackPieces[0] == ChessPieceType::BISHOP || blackPieces[0] == ChessPieceType::KNIGHT))) {
        insufficientMaterial = true;
    }
    // King and Bishop vs King and Bishop (same color squares)
    else if (whitePieces.size() == 1 && blackPieces.size() == 1 &&
             whitePieces[0] == ChessPieceType::BISHOP && blackPieces[0] == ChessPieceType::BISHOP) {
        // Check if bishops are on same color squares (simplified check)
        insufficientMaterial = true; // Assume same color for simplicity
    }
    
    if (insufficientMaterial) {
        return GameState::DRAW_INSUFFICIENT_MATERIAL;
    }
    
    return GameState::ONGOING;
}

void announceGameResult(GameState state) {
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "                GAME OVER                \n";
    std::cout << std::string(50, '=') << "\n";
    
    switch (state) {
        case GameState::CHECKMATE_WHITE_WINS:
            std::cout << "🏆 CHECKMATE! WHITE WINS! 🏆\n";
            std::cout << "Black king is in checkmate.\n";
            std::cout << "White has successfully cornered the black king!\n";
            break;
            
        case GameState::CHECKMATE_BLACK_WINS:
            std::cout << "🏆 CHECKMATE! BLACK WINS! 🏆\n";
            std::cout << "White king is in checkmate.\n";
            std::cout << "Black has successfully cornered the white king!\n";
            break;
            
        case GameState::STALEMATE:
            std::cout << "🤝 STALEMATE - DRAW! 🤝\n";
            std::cout << "The current player has no legal moves but is not in check.\n";
            std::cout << "The game ends in a draw by stalemate.\n";
            break;
            
        case GameState::DRAW_INSUFFICIENT_MATERIAL:
            std::cout << "🤝 DRAW - INSUFFICIENT MATERIAL! 🤝\n";
            std::cout << "Neither side has enough material to force checkmate.\n";
            std::cout << "The game ends in a draw.\n";
            break;
            
        case GameState::ONGOING:
            break;
    }
    
    if (state != GameState::ONGOING) {
        std::cout << std::string(50, '=') << "\n";
        std::cout << "Thank you for playing!\n";
        std::cout << "Press Enter to exit...\n";
    }
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
        
        // Check for game-ending conditions
        GameState gameState = checkGameState(ChessBoard);
        if (gameState != GameState::ONGOING) {
            announceGameResult(gameState);
            std::string dummy;
            std::getline(std::cin, dummy);
            break;
        }
        
        // Add check indicator to the prompt
        std::string checkIndicator = "";
        if (IsKingInCheck(ChessBoard, ChessBoard.turn)) {
            checkIndicator = " [CHECK!] ";
        }
        
        std::cout << "\nEnter move (e.g., e4, Nf3, O-O) or 'quit':" << checkIndicator << " ";
        std::getline(std::cin, input);
        
        if (input == "quit" || input == "exit") {
            break;
        }
        
        ChessTimePoint moveStartTime = ChessClock::now();
        
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(input, ChessBoard, srcCol, srcRow, destCol, destRow)) {
            // ENHANCED: Handle pawn promotion with piece selection
            ChessPieceType promotionPiece = ChessPieceType::QUEEN; // Default
            if (input.find('=') != std::string::npos) {
                promotionPiece = getPromotionPiece(input);
            }
            
            if (MovePiece(srcCol, srcRow, destCol, destRow, promotionPiece)) {
                std::cout << "✓ Move played successfully!\n";
            } else {
                std::cout << "❌ Invalid move. Try again.\n";
            }
        } else {
            std::cout << "❌ Could not parse move. Use algebraic notation (e.g., e4, Nf3, O-O, e8=Q).\n";
        }
        
        // Add computer move logic after player move
        if (ChessBoard.turn == ChessPieceColor::BLACK) {
            std::cout << "\nComputer is thinking...\n";
            
            ChessTimePoint computerStartTime = ChessClock::now();
            auto computerMove = getComputerMove(ChessBoard, 8000);
            auto computerTime = ChessClock::now() - computerStartTime;
            
            if (computerMove.first != -1 && computerMove.second != -1) {
                int from = computerMove.first;
                int to = computerMove.second;
                int srcCol = from % 8;
                int srcRow = from / 8;
                int destCol = to % 8;
                int destRow = to / 8;
                
                // ENHANCED: Handle computer promotion intelligently
                ChessPieceType computerPromotionPiece = ChessPieceType::QUEEN;
                const Piece& movingPiece = ChessBoard.squares[from].Piece;
                if (movingPiece.PieceType == ChessPieceType::PAWN && (destRow == 0 || destRow == 7)) {
                    // Computer promotes to Queen by default (could be enhanced with tactical analysis)
                    computerPromotionPiece = ChessPieceType::QUEEN;
                }
                
                if (MovePiece(srcCol, srcRow, destCol, destRow, computerPromotionPiece)) {
                    std::cout << "Computer played: " 
                              << positionToNotation(computerMove.first) 
                              << " to " 
                              << positionToNotation(computerMove.second)
                              << " (took " << std::chrono::duration_cast<ChessDuration>(computerTime).count() << "ms)\n";
                    
                    // Check if computer's move resulted in mate/stalemate
                    GameState postMoveState = checkGameState(ChessBoard);
                    if (postMoveState != GameState::ONGOING) {
                        printBoard(ChessBoard);
                        announceGameResult(postMoveState);
                        std::string dummy;
                        std::getline(std::cin, dummy);
                        return 0; // Exit the game
                    }
                } else {
                    std::cout << "Computer move failed!\n";
                }
            } else {
                std::cout << "Computer couldn't find a valid move!\n";
                // This might indicate mate or stalemate
                GameState state = checkGameState(ChessBoard);
                if (state != GameState::ONGOING) {
                    announceGameResult(state);
                    std::string dummy;
                    std::getline(std::cin, dummy);
                    return 0;
                }
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