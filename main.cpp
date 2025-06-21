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
        
using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

Board ChessBoard;
Board PrevBoard;
std::stack<int> MoveHistory;

PieceMoving MovingPiece(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
PieceMoving MovingPieceSecondary(ChessPieceColor::WHITE, ChessPieceType::PAWN, false);
bool PawnPromoted = false;

uint64_t ZobristTable[64][12];
uint64_t ZobristBlackToMove;

std::unordered_map<uint64_t, TTEntry> TransTable;

std::map<std::string, std::string> OpeningBook = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"},
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "e7e5"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", "g1f3"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", "b8c6"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "f1c4"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", "f8c5"},
};

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

bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    std::string cleanMove(move);
    if (!cleanMove.empty() && (cleanMove.back() == '+' || cleanMove.back() == '#')) {
        cleanMove.pop_back();
    }
    
    if (cleanMove == "O-O" || cleanMove == "0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 6; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 6; destRow = 7;
        }
        return true;
    }
    if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 2; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 2; destRow = 7;
        }
        return true;
    }
    
    ChessPieceType pieceType = ChessPieceType::PAWN;
    size_t startPos = 0;
    int disambigCol = -1, disambigRow = -1;
    
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
    
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';

        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || 
            srcCol < 0 || srcCol >= 8) return false;

        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                int from = pos;
                int to = destRow * 8 + destCol;
                if (board.turn == ChessPieceColor::WHITE) {
                    if ((to == from + 7 && srcCol > 0) || (to == from + 9 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::BLACK) {
                            srcRow = row;
                            return true;
                        }
                    }
                } else {
                    if ((to == from - 9 && srcCol > 0) || (to == from - 7 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::WHITE) {
                            srcRow = row;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    
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
    
    if (cleanMove.length() < startPos + 2) return false;
    
    if (cleanMove[startPos] == 'x') {
        destCol = cleanMove[startPos + 1] - 'a';
        destRow = cleanMove[startPos + 2] - '1';
    } else {
        destCol = cleanMove[startPos] - 'a';
        destRow = cleanMove[startPos + 1] - '1';
    }
    
    if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) return false;
    
    bool found = false;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == pieceType && piece.PieceColor == board.turn) {
                if (disambigCol != -1 && col != disambigCol) continue;
                if (disambigRow != -1 && row != disambigRow) continue;
                for (int validDest : piece.ValidMoves) {
                    if (validDest == destRow * 8 + destCol) {
                        srcCol = col;
                        srcRow = row;
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
        if (found) break;
    }
    if (found) return true;
    
    if (cleanMove.length() == 2 && pieceType == ChessPieceType::PAWN) {
        destCol = cleanMove[0] - 'a';
        destRow = cleanMove[1] - '1';
        
        if (destCol >= 0 && destCol < 8 && destRow >= 0 && destRow < 8) {
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int pos = row * 8 + col;
                    Piece& piece = board.squares[pos].Piece;
                    if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                        int pawnRow = pos / 8;
                        int pawnCol = pos % 8;
                        
                        if (board.turn == ChessPieceColor::WHITE) {
                            if (pawnCol == destCol && destRow > pawnRow) {
                                bool pathClear = true;
                                for (int r = pawnRow + 1; r <= destRow; r++) {
                                    if (board.squares[r * 8 + pawnCol].Piece.PieceType != ChessPieceType::NONE) {
                                        pathClear = false;
                                        break;
                                    }
                                }
                                if (pathClear && (destRow == pawnRow + 1 || (destRow == pawnRow + 2 && pawnRow == 1))) {
                                    srcCol = pawnCol;
                                    srcRow = pawnRow;
                                    return true;
                                }
                            }
                        } else {
                            if (pawnCol == destCol && destRow < pawnRow) {
                                bool pathClear = true;
                                for (int r = pawnRow - 1; r >= destRow; r--) {
                                    if (board.squares[r * 8 + pawnCol].Piece.PieceType != ChessPieceType::NONE) {
                                        pathClear = false;
                                        break;
                                    }
                                }
                                if (pathClear && (destRow == pawnRow - 1 || (destRow == pawnRow - 2 && pawnRow == 6))) {
                                    srcCol = pawnCol;
                                    srcRow = pawnRow;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (move.length() == 4) {
        int sCol, sRow, dCol, dRow;
        if (parseMove(move, sCol, sRow, dCol, dRow)) {
            srcCol = sCol;
            srcRow = sRow;
            destCol = dCol;
            destRow = dRow;
            return true;
        }
    }
    return false;
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
    ChessDuration timeLimit = std::chrono::milliseconds(timeLimitMs);
    
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
    if (currentTime - startTime >= timeLimit) {
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

std::string getFEN(const Board& board) {
    std::string fen;
    
    for (int row = 7; row >= 0; --row) {
        int emptyCount = 0;
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
            
            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                
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
                fen += pieceChar;
            }
        }
        
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        
        if (row > 0) {
            fen += "/";
        }
    }
    
    fen += " " + std::string(board.turn == ChessPieceColor::WHITE ? "w" : "b");
    
    std::string castling;
    if (board.whiteCanCastle) castling += "K";
    if (board.whiteCanCastle) castling += "Q";
    if (board.blackCanCastle) castling += "k";
    if (board.blackCanCastle) castling += "q";
    fen += " " + (castling.empty() ? "-" : castling);
    
    fen += " -";
    
    fen += " 0 1";
    
    return fen;
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