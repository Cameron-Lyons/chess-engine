#include "Evaluation.h"
#include "EvaluationTuning.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace EvaluationParams;

namespace PieceSquareTables {
    
    const int PAWN_MG[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
        50, 50, 50, 50, 50, 50, 50, 50,
        10, 10, 20, 30, 30, 20, 10, 10,
         5,  5, 10, 25, 25, 10,  5,  5,
         0,  0,  0, 20, 20,  0,  0,  0,
         5, -5,-10,  0,  0,-10, -5,  5,
         5, 10, 10,-20,-20, 10, 10,  5,
         0,  0,  0,  0,  0,  0,  0,  0
    };
    
    const int PAWN_EG[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
        80, 80, 80, 80, 80, 80, 80, 80,
        50, 50, 50, 50, 50, 50, 50, 50,
        30, 30, 30, 30, 30, 30, 30, 30,
        20, 20, 20, 20, 20, 20, 20, 20,
        10, 10, 10, 10, 10, 10, 10, 10,
        10, 10, 10, 10, 10, 10, 10, 10,
         0,  0,  0,  0,  0,  0,  0,  0
    };
    
    const int KNIGHT_MG[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };
    
    const int KNIGHT_EG[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };
    
    const int BISHOP_MG[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };
    
    const int BISHOP_EG[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };
    
    const int ROOK_MG[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    };
    
    const int ROOK_EG[64] = {
         0,  0,  0,  0,  0,  0,  0,  0,
         5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
         0,  0,  0,  5,  5,  0,  0,  0
    };
    
    const int QUEEN_MG[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };
    
    const int QUEEN_EG[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
         -5,  0,  5,  5,  5,  5,  0, -5,
          0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };
    
    const int KING_MG[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
         20, 20,  0,  0,  0,  0, 20, 20,
         20, 30, 10,  0,  0, 10, 30, 20
    };
    
    const int KING_EG[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };
    
    int getPieceSquareValue(ChessPieceType piece, int square, ChessPieceColor color, int gamePhase) {
        int adjustedSquare = (color == ChessPieceColor::WHITE) ? square : (63 - square);
        
        int mgValue = 0, egValue = 0;
        
        switch (piece) {
            case ChessPieceType::PAWN:
                mgValue = PAWN_MG[adjustedSquare];
                egValue = PAWN_EG[adjustedSquare];
                break;
            case ChessPieceType::KNIGHT:
                mgValue = KNIGHT_MG[adjustedSquare];
                egValue = KNIGHT_EG[adjustedSquare];
                break;
            case ChessPieceType::BISHOP:
                mgValue = BISHOP_MG[adjustedSquare];
                egValue = BISHOP_EG[adjustedSquare];
                break;
            case ChessPieceType::ROOK:
                mgValue = ROOK_MG[adjustedSquare];
                egValue = ROOK_EG[adjustedSquare];
                break;
            case ChessPieceType::QUEEN:
                mgValue = QUEEN_MG[adjustedSquare];
                egValue = QUEEN_EG[adjustedSquare];
                break;
            case ChessPieceType::KING:
                mgValue = KING_MG[adjustedSquare];
                egValue = KING_EG[adjustedSquare];
                break;
            default:
                return 0;
        }
        
        return ((mgValue * (256 - gamePhase)) + (egValue * gamePhase)) / 256;
    }
    
    int calculateGamePhase(const Board& board) {
        int totalMaterial = 0;
        for (int i = 0; i < 64; i++) {
            ChessPieceType piece = board.squares[i].piece.PieceType;
            if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING && piece != ChessPieceType::PAWN) {
                switch (piece) {
                    case ChessPieceType::QUEEN: totalMaterial += 900; break;
                    case ChessPieceType::ROOK: totalMaterial += 500; break;
                    case ChessPieceType::BISHOP:
                    case ChessPieceType::KNIGHT: totalMaterial += 300; break;
                    default: break;
                }
            }
        }
        
        int phase = 256 - (totalMaterial * 256) / 3900;
        return std::max(0, std::min(256, phase));
    }
}

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {
    int value = 0;
    switch (pieceType) {
        case ChessPieceType::PAWN:
            value = PieceSquareTables::PAWN_MG[position];
            break;
        case ChessPieceType::KNIGHT:
            value = PieceSquareTables::KNIGHT_MG[position];
            break;
        case ChessPieceType::BISHOP:
            value = PieceSquareTables::BISHOP_MG[position];
            break;
        case ChessPieceType::ROOK:
            value = PieceSquareTables::ROOK_MG[position];
            break;
        case ChessPieceType::QUEEN:
            value = PieceSquareTables::QUEEN_MG[position];
            break;
        case ChessPieceType::KING:
            value = PieceSquareTables::KING_MG[position];
            break;
        default:
            value = 0;
            break;
    }
    if (color == ChessPieceColor::BLACK) {
        value = -value;
    }
    return value;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;
    for (int col = 0; col < 8; col++) {
        int whitePawns = 0, blackPawns = 0;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                } else {
                    blackPawns++;
                }
            }
        }
        if (whitePawns > 1) score -= 20 * (whitePawns - 1);
        if (blackPawns > 1) score += 20 * (blackPawns - 1);
    }
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col) continue;
                    for (int adjRow = 0; adjRow < 8; adjRow++) {
                        int adjPos = adjRow * 8 + adjCol;
                        if (board.squares[adjPos].piece.PieceType == ChessPieceType::PAWN && 
                            board.squares[adjPos].piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) break;
                }
                if (isolated) {
                    if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 30;
                    } else {
                        score += 30;
                    }
                }
            }
        }
    }
    return score;
}

int evaluateMobility(const Board& board) {
    int mobilityScore = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        int mobility = 0;
        int row = i / 8;
        int col = i % 8;
        
        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT: {
                int knightMoves[][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
                for (auto& move : knightMoves) {
                    int newRow = row + move[0];
                    int newCol = col + move[1];
                    if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE ||
                            board.squares[pos].piece.PieceColor != piece.PieceColor) {
                            mobility++;
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 4 : -mobility * 4;
                break;
            }
            
            case ChessPieceType::BISHOP: {
                int directions[][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; 
                            }
                            break; 
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 3 : -mobility * 3;
                break;
            }
            
            case ChessPieceType::ROOK: {
                int directions[][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; 
                            }
                            break; 
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 2 : -mobility * 2;
                break;
            }
            
            case ChessPieceType::QUEEN: {
                int directions[][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; 
                            }
                            break; 
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 1 : -mobility * 1;
                break;
            }
            
            default:
                break;
        }
    }
    
    return mobilityScore;
}

int evaluateCenterControl(const Board& board) {
    int score = 0;
    int centerSquares[] = {27, 28, 35, 36};
    for (int square : centerSquares) {
        if (board.squares[square].piece.PieceType != ChessPieceType::NONE) {
            if (board.squares[square].piece.PieceColor == ChessPieceColor::WHITE) {
                score += 30;
            } else {
                score -= 30;
            }
        }
    }
    return score;
}

int evaluateKingSafety(const Board& board) {
    int safetyScore = 0;
    
    int whiteKingPos = -1, blackKingPos = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
            if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                whiteKingPos = i;
            } else {
                blackKingPos = i;
            }
        }
    }
    
    if (whiteKingPos != -1) {
        safetyScore += evaluateKingSafetyForColor(board, whiteKingPos, ChessPieceColor::WHITE);
    }
    if (blackKingPos != -1) {
        safetyScore -= evaluateKingSafetyForColor(board, blackKingPos, ChessPieceColor::BLACK);
    }
    
    return safetyScore;
}

int evaluateKingSafety(const Board& board, ChessPieceColor color) {
    int kingPos = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::KING &&
            board.squares[i].piece.PieceColor == color) {
            kingPos = i;
            break;
        }
    }
    
    if (kingPos == -1) {
        return 0; 
    }
    
    return evaluateKingSafetyForColor(board, kingPos, color);
}

int evaluateKingSafetyForColor(const Board& board, int kingPos, ChessPieceColor color) {
    int safety = 0;
    int kingRow = kingPos / 8;
    int kingCol = kingPos % 8;
    
    int pawnShieldBonus = 0;
    if (color == ChessPieceColor::WHITE) {
        for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
            for (int row = kingRow + 1; row < 8; row++) {
                int pos = row * 8 + col;
                if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN &&
                    board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                    pawnShieldBonus += 30; // Bonus for pawn shield
                    break;
                }
            }
        }
    } else {
        for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
            for (int row = kingRow - 1; row >= 0; row--) {
                int pos = row * 8 + col;
                if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN &&
                    board.squares[pos].piece.PieceColor == ChessPieceColor::BLACK) {
                    pawnShieldBonus += 30; // Bonus for pawn shield
                    break;
                }
            }
        }
    }
    safety += pawnShieldBonus;
    
    for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
        bool hasOwnPawn = false;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN &&
                board.squares[pos].piece.PieceColor == color) {
                hasOwnPawn = true;
                break;
            }
        }
        if (!hasOwnPawn) {
            safety -= 20; 
        }
    }
    
    int enemyAttacks = 0;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceColor != color) {
            int distance = std::abs(i / 8 - kingRow) + std::abs(i % 8 - kingCol);
            if (distance <= 2) {
                switch (piece.PieceType) {
                    case ChessPieceType::QUEEN: enemyAttacks += 50; break;
                    case ChessPieceType::ROOK: enemyAttacks += 30; break;
                    case ChessPieceType::BISHOP: enemyAttacks += 20; break;
                    case ChessPieceType::KNIGHT: enemyAttacks += 25; break;
                    default: break;
                }
            }
        }
    }
    safety -= enemyAttacks;
    
    if ((color == ChessPieceColor::WHITE && kingPos == 4) ||
        (color == ChessPieceColor::BLACK && kingPos == 60)) {
        safety += 50; 
    }
    
    return safety;
}

int evaluatePassedPawns(const Board& board) {
    int score = 0;
    
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].piece;
            
            if (piece.PieceType == ChessPieceType::PAWN) {
                bool isPassed = true;
                
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1); checkCol++) {
                        for (int checkRow = row + 1; checkRow < 8; checkRow++) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor == ChessPieceColor::BLACK) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    if (isPassed) {
                        int passedBonus = (row - 1) * 20 + 10; 
                        score += passedBonus;
                    }
                } else {
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1); checkCol++) {
                        for (int checkRow = row - 1; checkRow >= 0; checkRow--) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor == ChessPieceColor::WHITE) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    if (isPassed) {
                        int passedBonus = (6 - row) * 20 + 10; 
                        score -= passedBonus;
                    }
                }
            }
        }
    }
    
    return score;
}

int evaluateBishopPair(const Board& board) {
    int whiteBishops = 0, blackBishops = 0;
    
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::BISHOP) {
            if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                whiteBishops++;
            } else {
                blackBishops++;
            }
        }
    }
    
    int score = 0;
    if (whiteBishops >= 2) score += 50; 
    if (blackBishops >= 2) score -= 50;
    
    return score;
}

int evaluateRooksOnOpenFiles(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::ROOK) {
            int col = i % 8;
            bool isOpenFile = true;
            bool isSemiOpen = true;
            
            for (int row = 0; row < 8; row++) {
                int pos = row * 8 + col;
                if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                    isOpenFile = false;
                    if (board.squares[pos].piece.PieceColor == board.squares[i].piece.PieceColor) {
                        isSemiOpen = false;
                    }
                }
            }
            
            if (isOpenFile) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += 20;
                } else {
                    score -= 20;
                }
            } else if (isSemiOpen) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += 10;
                } else {
                    score -= 10;
                }
            }
        }
    }
    
    return score;
}

int evaluateEndgame(const Board& board) {
    int totalMaterial = 0;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            totalMaterial += piece.PieceValue;
        }
    }
    
    int score = 0;
    if (totalMaterial < 2000) { 
        for (int i = 0; i < 64; i++) {
            if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
                int file = i % 8;
                int rank = i / 8;
                int centerDistance = std::max(abs(file - 3.5), abs(rank - 3.5));
                
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += (7 - centerDistance) * 5; 
                } else {
                    score -= (7 - centerDistance) * 5;
                }
            }
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED EVALUATION FUNCTION WITH TUNING PARAMETERS
// ===================================================================
int evaluatePosition(const Board& board) {
    int mgScore = 0, egScore = 0;
    int gamePhase = PieceSquareTables::calculateGamePhase(board);
    
    // ===================================================================
    // MATERIAL AND ENHANCED PIECE-SQUARE TABLES
    // ===================================================================
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        // Use tuned material values
        int materialValue = getMaterialValue(piece.PieceType);
        
        // Use both old and new PST (blended for gradual improvement)
        int oldPositionalValue = PieceSquareTables::getPieceSquareValue(piece.PieceType, square, piece.PieceColor, gamePhase);
        
        // New enhanced PST values
        int adjustedSquare = (piece.PieceColor == ChessPieceColor::WHITE) ? square : (63 - square);
        int newMgPST = getTunedPST(piece.PieceType, adjustedSquare, false);
        int newEgPST = getTunedPST(piece.PieceType, adjustedSquare, true);
        int newPositionalValue = interpolatePhase(newMgPST, newEgPST, gamePhase);
        
        // Blend old and new (70% new, 30% old for gradual transition)
        int blendedPositionalValue = (newPositionalValue * 7 + oldPositionalValue * 3) / 10;
        
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + newMgPST;
            egScore += materialValue + newEgPST;
        } else {
            mgScore -= materialValue + newMgPST;
            egScore -= materialValue + newEgPST;
        }
    }
    
    // ===================================================================
    // ENHANCED POSITIONAL EVALUATION WITH TUNABLE WEIGHTS
    // ===================================================================
    
    if (ENABLE_PAWN_STRUCTURE) {
        int pawnScore = evaluateEnhancedPawnStructure(board);
        mgScore += pawnScore;
        egScore += pawnScore * 1.2; // Pawn structure more important in endgame
        logEvaluationComponents("Enhanced Pawn Structure", pawnScore);
    }
    
    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateMobility(board);
        mgScore += mobilityScore;
        egScore += mobilityScore * 0.8; // Mobility less critical in endgame
        logEvaluationComponents("Piece Mobility", mobilityScore);
    }
    
    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += kingSafetyScore * 0.3; // King safety less important in endgame
        logEvaluationComponents("King Safety", kingSafetyScore);
    }
    
    // Enhanced evaluations with tuning weights
    int bishopPairScore = evaluateBishopPair(board);
    mgScore += bishopPairScore;
    egScore += bishopPairScore * 1.5; // Bishop pair more valuable in endgame
    
    int rookFileScore = evaluateRooksOnOpenFiles(board);
    mgScore += rookFileScore;
    egScore += rookFileScore;
    
    int passedPawnScore = evaluatePassedPawns(board);
    mgScore += passedPawnScore * 0.8; // Less important in middlegame
    egScore += passedPawnScore * 1.5; // Very important in endgame
    
    // Tactical safety (critical for avoiding blunders)
    int tacticalSafetyScore = evaluateTacticalSafety(board);
    mgScore += tacticalSafetyScore;
    egScore += tacticalSafetyScore * 0.7;
    
    int hangingPiecesScore = evaluateHangingPieces(board);
    mgScore += hangingPiecesScore;
    egScore += hangingPiecesScore;
    
    int queenTrapScore = evaluateQueenTrapDanger(board);
    mgScore += queenTrapScore;
    egScore += queenTrapScore * 0.5; // Less relevant in endgame
    
    // ===================================================================
    // TEMPO BONUS
    // ===================================================================
    if (board.turn == ChessPieceColor::WHITE) {
        mgScore += TEMPO_BONUS;
        egScore += TEMPO_BONUS / 2;
    } else {
        mgScore -= TEMPO_BONUS;
        egScore -= TEMPO_BONUS / 2;
    }
    
    // ===================================================================
    // PHASE INTERPOLATION
    // ===================================================================
    int finalScore = interpolatePhase(mgScore, egScore, gamePhase);
    
    logEvaluationComponents("Final Enhanced Score", finalScore);
    return finalScore;
}

int evaluateHangingPieces(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) continue;
        
        bool isUnderAttack = false;
        bool isDefended = false;
        
        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        for (int j = 0; j < 64; j++) {
            const Piece& enemyPiece = board.squares[j].piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor) continue;
            
            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }
        
        if (isUnderAttack) {
            for (int j = 0; j < 64; j++) {
                const Piece& friendlyPiece = board.squares[j].piece;
                if (friendlyPiece.PieceType == ChessPieceType::NONE || friendlyPiece.PieceColor != piece.PieceColor || j == i) continue;
                
                if (canPieceAttackSquare(board, j, i)) {
                    isDefended = true;
                    break;
                }
            }
            
            if (!isDefended) {
                int hangingPenalty = piece.PieceValue * 0.8; 
                
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= hangingPenalty;
                } else {
                    score += hangingPenalty;
                }
                
                if (piece.PieceType == ChessPieceType::QUEEN) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 500; 
                    } else {
                        score += 500;
                    }
                }
            }
        }
        
        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / 8;
            int col = i % 8;
            
            bool inEnemyTerritory = false;
            if (piece.PieceColor == ChessPieceColor::WHITE && row >= 5) {
                inEnemyTerritory = true;
            } else if (piece.PieceColor == ChessPieceColor::BLACK && row <= 2) {
                inEnemyTerritory = true;
            }
            
            if (inEnemyTerritory) {
                int supportCount = 0;
                for (int j = 0; j < 64; j++) {
                    const Piece& friendlyPiece = board.squares[j].piece;
                    if (friendlyPiece.PieceType == ChessPieceType::NONE || 
                        friendlyPiece.PieceColor != piece.PieceColor || 
                        j == i || 
                        friendlyPiece.PieceType == ChessPieceType::PAWN) continue;
                    
                    if (canPieceAttackSquare(board, j, i)) {
                        supportCount++;
                    }
                }
                
                if (supportCount == 0) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 300; 
                    } else {
                        score += 300;
                    }
                }
                
                int nearbyEnemies = 0;
                for (int dr = -2; dr <= 2; dr++) {
                    for (int dc = -2; dc <= 2; dc++) {
                        int checkRow = row + dr;
                        int checkCol = col + dc;
                        if (checkRow >= 0 && checkRow < 8 && checkCol >= 0 && checkCol < 8) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[checkPos].piece.PieceColor == enemyColor) {
                                nearbyEnemies++;
                            }
                        }
                    }
                }
                
                if (nearbyEnemies >= 2) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 200; 
                    } else {
                        score += 200;
                    }
                }
            }
        }
        
        if (piece.PieceType == ChessPieceType::QUEEN || 
            piece.PieceType == ChessPieceType::ROOK || 
            piece.PieceType == ChessPieceType::BISHOP) {
            
            int row = i / 8;
            int col = i % 8;
            
            bool isOnEdge = (row == 0 || row == 7 || col == 0 || col == 7);
            bool isInCorner = ((row == 0 || row == 7) && (col == 0 || col == 7));
            
            if (isInCorner && piece.PieceType == ChessPieceType::QUEEN) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= 400;
                } else {
                    score += 400;
                }
            } else if (isOnEdge && piece.PieceType == ChessPieceType::QUEEN) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= 150;
                } else {
                    score += 150;
                }
            }
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED PAWN STRUCTURE EVALUATION WITH TUNING PARAMETERS
// ===================================================================
int evaluateEnhancedPawnStructure(const Board& board) {
    int score = 0;
    
    // Track pawn structure for both colors
    for (int file = 0; file < 8; file++) {
        int whitePawns = 0, blackPawns = 0;
        int whiteMaxRank = -1, blackMinRank = 8;
        std::vector<int> whitePawnRanks, blackPawnRanks;
        
        // Count pawns on this file
        for (int rank = 0; rank < 8; rank++) {
            int square = rank * 8 + file;
            const Piece& piece = board.squares[square].piece;
            
            if (piece.PieceType == ChessPieceType::PAWN) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                    whiteMaxRank = std::max(whiteMaxRank, rank);
                    whitePawnRanks.push_back(rank);
                } else {
                    blackPawns++;
                    blackMinRank = std::min(blackMinRank, rank);
                    blackPawnRanks.push_back(rank);
                }
            }
        }
        
        // ===================================================================
        // DOUBLED PAWNS PENALTY (Tunable)
        // ===================================================================
        if (whitePawns > 1) {
            score -= DOUBLED_PAWN_PENALTY * (whitePawns - 1);
        }
        if (blackPawns > 1) {
            score += DOUBLED_PAWN_PENALTY * (blackPawns - 1);
        }
        
        // ===================================================================
        // ISOLATED PAWNS PENALTY (Tunable)
        // ===================================================================
        if (whitePawns > 0) {
            bool hasSupport = false;
            // Check adjacent files for friendly pawns
            for (int adjFile = std::max(0, file - 1); adjFile <= std::min(7, file + 1); adjFile++) {
                if (adjFile == file) continue;
                for (int rank = 0; rank < 8; rank++) {
                    int adjSquare = rank * 8 + adjFile;
                    if (board.squares[adjSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[adjSquare].piece.PieceColor == ChessPieceColor::WHITE) {
                        hasSupport = true;
                        break;
                    }
                }
                if (hasSupport) break;
            }
            if (!hasSupport) {
                score -= ISOLATED_PAWN_PENALTY;
            }
        }
        
        if (blackPawns > 0) {
            bool hasSupport = false;
            for (int adjFile = std::max(0, file - 1); adjFile <= std::min(7, file + 1); adjFile++) {
                if (adjFile == file) continue;
                for (int rank = 0; rank < 8; rank++) {
                    int adjSquare = rank * 8 + adjFile;
                    if (board.squares[adjSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[adjSquare].piece.PieceColor == ChessPieceColor::BLACK) {
                        hasSupport = true;
                        break;
                    }
                }
                if (hasSupport) break;
            }
            if (!hasSupport) {
                score += ISOLATED_PAWN_PENALTY;
            }
        }
        
        // ===================================================================
        // PASSED PAWNS BONUS (Tunable by rank)
        // ===================================================================
        if (whitePawns == 1 && blackPawns == 0) {
            // Check if it's truly passed (no enemy pawns blocking)
            bool isBlocked = false;
            for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                for (int rank = whiteMaxRank + 1; rank < 8; rank++) {
                    int checkSquare = rank * 8 + checkFile;
                    if (board.squares[checkSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].piece.PieceColor == ChessPieceColor::BLACK) {
                        isBlocked = true;
                        break;
                    }
                }
                if (isBlocked) break;
            }
            if (!isBlocked) {
                score += PASSED_PAWN_BONUS[whiteMaxRank];
            }
        }
        
        if (blackPawns == 1 && whitePawns == 0) {
            bool isBlocked = false;
            for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                for (int rank = 0; rank < blackMinRank; rank++) {
                    int checkSquare = rank * 8 + checkFile;
                    if (board.squares[checkSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].piece.PieceColor == ChessPieceColor::WHITE) {
                        isBlocked = true;
                        break;
                    }
                }
                if (isBlocked) break;
            }
            if (!isBlocked) {
                score -= PASSED_PAWN_BONUS[7 - blackMinRank];
            }
        }
    }
    
    // ===================================================================
    // CONNECTED PAWNS BONUS (Tunable)
    // ===================================================================
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType != ChessPieceType::PAWN) continue;
        
        int file = square % 8;
        int rank = square / 8;
        
        // Check for connected pawns (diagonal support)
        bool hasConnection = false;
        for (int df = -1; df <= 1; df += 2) { // Check left and right diagonals
            int newFile = file + df;
            if (newFile >= 0 && newFile < 8) {
                int backRank = (piece.PieceColor == ChessPieceColor::WHITE) ? rank - 1 : rank + 1;
                if (backRank >= 0 && backRank < 8) {
                    int checkSquare = backRank * 8 + newFile;
                    if (board.squares[checkSquare].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].piece.PieceColor == piece.PieceColor) {
                        hasConnection = true;
                        break;
                    }
                }
            }
        }
        
        if (hasConnection) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += CONNECTED_PAWNS_BONUS;
            } else {
                score -= CONNECTED_PAWNS_BONUS;
            }
        }
        
        // ===================================================================
        // PAWN CHAIN BONUS (Multiple connected pawns)
        // ===================================================================
        if (hasConnection) {
            // Check if this pawn is part of a longer chain
            int chainLength = 1;
            int currentFile = file;
            int currentRank = rank;
            
            // Count chain extending forward
            while (currentFile < 7) {
                currentFile++;
                currentRank = (piece.PieceColor == ChessPieceColor::WHITE) ? currentRank + 1 : currentRank - 1;
                if (currentRank < 0 || currentRank >= 8) break;
                
                int chainSquare = currentRank * 8 + currentFile;
                if (board.squares[chainSquare].piece.PieceType == ChessPieceType::PAWN &&
                    board.squares[chainSquare].piece.PieceColor == piece.PieceColor) {
                    chainLength++;
                } else {
                    break;
                }
            }
            
            if (chainLength >= 3) { // Chain of 3+ pawns gets bonus
                int chainBonus = PAWN_CHAIN_BONUS * (chainLength - 2);
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score += chainBonus;
                } else {
                    score -= chainBonus;
                }
            }
        }
    }
    
    return score;
}

bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos) {
    const Piece& piece = board.squares[piecePos].piece;
    if (piece.PieceType == ChessPieceType::NONE) return false;
    
    int fromRow = piecePos / 8;
    int fromCol = piecePos % 8;
    int toRow = targetPos / 8;
    int toCol = targetPos % 8;
    
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;
    
    switch (piece.PieceType) {
        case ChessPieceType::PAWN: {
            int direction = (piece.PieceColor == ChessPieceColor::WHITE) ? 1 : -1;
            return (rowDiff == direction && (colDiff == 1 || colDiff == -1));
        }
        case ChessPieceType::KNIGHT: {
            return (abs(rowDiff) == 2 && abs(colDiff) == 1) || 
                   (abs(rowDiff) == 1 && abs(colDiff) == 2);
        }
        case ChessPieceType::BISHOP: {
            if (abs(rowDiff) != abs(colDiff)) return false;
            int rowStep = (rowDiff > 0) ? 1 : -1;
            int colStep = (colDiff > 0) ? 1 : -1;
            for (int i = 1; i < abs(rowDiff); i++) {
                int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
            }
            return true;
        }
        case ChessPieceType::ROOK: {
            if (rowDiff != 0 && colDiff != 0) return false;
            if (rowDiff == 0) {
                int colStep = (colDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(colDiff); i++) {
                    int checkPos = fromRow * 8 + (fromCol + i * colStep);
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
                }
            } else {
                int rowStep = (rowDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(rowDiff); i++) {
                    int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
                }
            }
            return true;
        }
        case ChessPieceType::QUEEN: {
            if (rowDiff == 0 || colDiff == 0 || abs(rowDiff) == abs(colDiff)) {
                if (rowDiff == 0) {
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(colDiff); i++) {
                        int checkPos = fromRow * 8 + (fromCol + i * colStep);
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
                    }
                } else if (colDiff == 0) {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
                    }
                } else {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) return false;
                    }
                }
                return true;
            }
            return false;
        }
        case ChessPieceType::KING: {
            return abs(rowDiff) <= 1 && abs(colDiff) <= 1;
        }
        default:
            return false;
    }
}

int evaluateQueenTrapDanger(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::QUEEN) continue;
        
        int row = i / 8;
        int col = i % 8;
        
        int escapeSquares = 0;
        
        int directions[8][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
        
        for (int dir = 0; dir < 8; dir++) {
            int newRow = row + directions[dir][0];
            int newCol = col + directions[dir][1];
            
            while (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                int newPos = newRow * 8 + newCol;
                
                if (board.squares[newPos].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[newPos].piece.PieceColor == piece.PieceColor) {
                    break;
                }
                
                bool squareIsSafe = true;
                ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                            ChessPieceColor::BLACK : ChessPieceColor::WHITE;
                
                for (int j = 0; j < 64; j++) {
                    const Piece& enemyPiece = board.squares[j].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE || 
                        enemyPiece.PieceColor != enemyColor) continue;
                    
                    if (canPieceAttackSquare(board, j, newPos)) {
                        squareIsSafe = false;
                        break;
                    }
                }
                
                if (squareIsSafe) {
                    escapeSquares++;
                }
                
                if (board.squares[newPos].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[newPos].piece.PieceColor != piece.PieceColor) {
                    break;
                }
                
                newRow += directions[dir][0];
                newCol += directions[dir][1];
                
                if (abs(newRow - row) > 2 || abs(newCol - col) > 2) break;
            }
        }
        
        if (escapeSquares == 0) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= 800; 
            } else {
                score += 800;
            }
        } else if (escapeSquares <= 2) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= 400; 
            } else {
                score += 400;
            }
        } else if (escapeSquares <= 4) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= 200;
            } else {
                score += 200;
            }
        }
        
        bool isOnEdge = (row == 0 || row == 7 || col == 0 || col == 7);
        bool isInCorner = ((row == 0 || row == 7) && (col == 0 || col == 7));
        
        if (isInCorner && escapeSquares <= 3) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= 600; 
            } else {
                score += 600;
            }
        } else if (isOnEdge && escapeSquares <= 5) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= 300; 
            } else {
                score += 300;
            }
        }
    }
    
    return score;
}

int evaluateTacticalSafety(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / 8;
            int col = i % 8;
            
            int attackers = 0;
            int defenders = 0;
            
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            for (int j = 0; j < 64; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) continue;
                
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }
            
            if (attackers > defenders) {
                int dangerLevel = (attackers - defenders) * 300;
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= dangerLevel;
                } else {
                    score += dangerLevel;
                }
                
                if (attackers > 0) {
                                         int weakestAttacker = 10000;
                     for (int j = 0; j < 64; j++) {
                         const Piece& attacker = board.squares[j].piece;
                         if (attacker.PieceColor == enemyColor && 
                             canPieceAttackSquare(board, j, i)) {
                             if (attacker.PieceValue < weakestAttacker) {
                                 weakestAttacker = attacker.PieceValue;
                             }
                         }
                     }
                    
                    if (weakestAttacker < piece.PieceValue) {
                        if (piece.PieceColor == ChessPieceColor::WHITE) {
                            score -= 800; 
                        } else {
                            score += 800;
                        }
                    }
                }
            }
            
            bool nearEnemyKing = false;
            for (int j = 0; j < 64; j++) {
                if (board.squares[j].piece.PieceType == ChessPieceType::KING &&
                    board.squares[j].piece.PieceColor == enemyColor) {
                    int kingRow = j / 8;
                    int kingCol = j % 8;
                                         int distance = (abs(row - kingRow) > abs(col - kingCol)) ? 
                                   abs(row - kingRow) : abs(col - kingCol);
                    if (distance <= 2) {
                        nearEnemyKing = true;
                        break;
                    }
                }
            }
            
            if (nearEnemyKing && defenders < 2) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= 250;
                } else {
                    score += 250;
                }
            }
        }
        
        else if (piece.PieceValue >= 300) { 
            int attackers = 0;
            int defenders = 0;
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                        ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            for (int j = 0; j < 64; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) continue;
                
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }
            
            if (attackers > defenders) {
                int penalty = (attackers - defenders) * piece.PieceValue / 4;
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= penalty;
                } else {
                    score += penalty;
                }
            }
        }
    }
    
    return score;
}
