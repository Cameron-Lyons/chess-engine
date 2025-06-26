#include "Evaluation.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include <algorithm>
#include <cmath>

// Advanced Piece-Square Tables with game phase adaptation
namespace PieceSquareTables {
    
    // Pawn tables - encourage center control and advancement
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
    
    // Knight tables - centralization is key
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
    
    // Bishop tables - long diagonals and center control
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
    
    // Rook tables - 7th rank and open files
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
    
    // Queen tables - avoid early development
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
    
    // King tables - safety in middlegame, activity in endgame
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
    
    // Get piece-square table value based on game phase
    int getPieceSquareValue(ChessPieceType piece, int square, ChessPieceColor color, int gamePhase) {
        // Flip square for black pieces (they see the board from opposite perspective)
        int adjustedSquare = (color == ChessPieceColor::WHITE) ? square : (63 - square);
        
        // gamePhase: 0 = opening/middlegame, 256 = pure endgame
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
        
        // Interpolate between middlegame and endgame values
        return ((mgValue * (256 - gamePhase)) + (egValue * gamePhase)) / 256;
    }
    
    // Calculate game phase (0 = opening, 256 = endgame)
    int calculateGamePhase(const Board& board) {
        int totalMaterial = 0;
        for (int i = 0; i < 64; i++) {
            ChessPieceType piece = board.squares[i].Piece.PieceType;
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
        
        // Opening material: ~3900 (2Q + 4R + 4B + 4N)
        // Scale from 0 (opening) to 256 (endgame)
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
            if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[pos].Piece.PieceColor == ChessPieceColor::WHITE) {
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
            if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col) continue;
                    for (int adjRow = 0; adjRow < 8; adjRow++) {
                        int adjPos = adjRow * 8 + adjCol;
                        if (board.squares[adjPos].Piece.PieceType == ChessPieceType::PAWN && 
                            board.squares[adjPos].Piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) break;
                }
                if (isolated) {
                    if (board.squares[pos].Piece.PieceColor == ChessPieceColor::WHITE) {
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

// High Complexity Win #3: Mobility Evaluation (+20-40 Elo)
int evaluateMobility(const Board& board) {
    int mobilityScore = 0;
    
    // Count mobility for each piece type
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        int mobility = 0;
        int row = i / 8;
        int col = i % 8;
        
        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT: {
                // Knight mobility - L-shaped moves
                int knightMoves[][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
                for (auto& move : knightMoves) {
                    int newRow = row + move[0];
                    int newCol = col + move[1];
                    if (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].Piece.PieceType == ChessPieceType::NONE ||
                            board.squares[pos].Piece.PieceColor != piece.PieceColor) {
                            mobility++;
                        }
                    }
                }
                // Scale knight mobility
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 4 : -mobility * 4;
                break;
            }
            
            case ChessPieceType::BISHOP: {
                // Bishop mobility - diagonal moves
                int directions[][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].Piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].Piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 3 : -mobility * 3;
                break;
            }
            
            case ChessPieceType::ROOK: {
                // Rook mobility - rank and file moves
                int directions[][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].Piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].Piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
                        }
                    }
                }
                mobilityScore += piece.PieceColor == ChessPieceColor::WHITE ? mobility * 2 : -mobility * 2;
                break;
            }
            
            case ChessPieceType::QUEEN: {
                // Queen mobility - combination of rook and bishop
                int directions[][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8) break;
                        
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].Piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].Piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
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
        if (board.squares[square].Piece.PieceType != ChessPieceType::NONE) {
            if (board.squares[square].Piece.PieceColor == ChessPieceColor::WHITE) {
                score += 30;
            } else {
                score -= 30;
            }
        }
    }
    return score;
}

// High Complexity Win #4: Enhanced King Safety (+30-50 Elo)
int evaluateKingSafety(const Board& board) {
    int safetyScore = 0;
    
    // Find kings
    int whiteKingPos = -1, blackKingPos = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType == ChessPieceType::KING) {
            if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
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

// Overloaded function for single color evaluation (used by tests)
int evaluateKingSafety(const Board& board, ChessPieceColor color) {
    // Find the king of the specified color
    int kingPos = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType == ChessPieceType::KING &&
            board.squares[i].Piece.PieceColor == color) {
            kingPos = i;
            break;
        }
    }
    
    if (kingPos == -1) {
        return 0; // King not found
    }
    
    return evaluateKingSafetyForColor(board, kingPos, color);
}

int evaluateKingSafetyForColor(const Board& board, int kingPos, ChessPieceColor color) {
    int safety = 0;
    int kingRow = kingPos / 8;
    int kingCol = kingPos % 8;
    
    // Pawn shield evaluation
    int pawnShieldBonus = 0;
    if (color == ChessPieceColor::WHITE) {
        // Check pawns in front of white king
        for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
            for (int row = kingRow + 1; row < 8; row++) {
                int pos = row * 8 + col;
                if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN &&
                    board.squares[pos].Piece.PieceColor == ChessPieceColor::WHITE) {
                    pawnShieldBonus += 30; // Bonus for pawn shield
                    break;
                }
            }
        }
    } else {
        // Check pawns in front of black king
        for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
            for (int row = kingRow - 1; row >= 0; row--) {
                int pos = row * 8 + col;
                if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN &&
                    board.squares[pos].Piece.PieceColor == ChessPieceColor::BLACK) {
                    pawnShieldBonus += 30; // Bonus for pawn shield
                    break;
                }
            }
        }
    }
    safety += pawnShieldBonus;
    
    // Open file penalty near king
    for (int col = std::max(0, kingCol - 1); col <= std::min(7, kingCol + 1); col++) {
        bool hasOwnPawn = false;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN &&
                board.squares[pos].Piece.PieceColor == color) {
                hasOwnPawn = true;
                break;
            }
        }
        if (!hasOwnPawn) {
            safety -= 20; // Penalty for open files near king
        }
    }
    
    // Enemy piece proximity penalty
    int enemyAttacks = 0;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
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
    
    // Castling rights bonus (if king hasn't moved)
    if ((color == ChessPieceColor::WHITE && kingPos == 4) ||
        (color == ChessPieceColor::BLACK && kingPos == 60)) {
        safety += 50; // Bonus for keeping castling rights
    }
    
    return safety;
}

int evaluatePassedPawns(const Board& board) {
    int score = 0;
    
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
            
            if (piece.PieceType == ChessPieceType::PAWN) {
                bool isPassed = true;
                
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    // Check if any black pawn can stop this white pawn
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1); checkCol++) {
                        for (int checkRow = row + 1; checkRow < 8; checkRow++) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].Piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].Piece.PieceColor == ChessPieceColor::BLACK) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    if (isPassed) {
                        int passedBonus = (row - 1) * 20 + 10; // More valuable closer to promotion
                        score += passedBonus;
                    }
                } else {
                    // Check if any white pawn can stop this black pawn
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1); checkCol++) {
                        for (int checkRow = row - 1; checkRow >= 0; checkRow--) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].Piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].Piece.PieceColor == ChessPieceColor::WHITE) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    if (isPassed) {
                        int passedBonus = (6 - row) * 20 + 10; // More valuable closer to promotion
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
        if (board.squares[i].Piece.PieceType == ChessPieceType::BISHOP) {
            if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
                whiteBishops++;
            } else {
                blackBishops++;
            }
        }
    }
    
    int score = 0;
    if (whiteBishops >= 2) score += 50; // Bishop pair bonus
    if (blackBishops >= 2) score -= 50;
    
    return score;
}

int evaluateRooksOnOpenFiles(const Board& board) {
    int score = 0;
    
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType == ChessPieceType::ROOK) {
            int col = i % 8;
            bool isOpenFile = true;
            bool isSemiOpen = true;
            
            // Check if file has pawns
            for (int row = 0; row < 8; row++) {
                int pos = row * 8 + col;
                if (board.squares[pos].Piece.PieceType == ChessPieceType::PAWN) {
                    isOpenFile = false;
                    if (board.squares[pos].Piece.PieceColor == board.squares[i].Piece.PieceColor) {
                        isSemiOpen = false;
                    }
                }
            }
            
            if (isOpenFile) {
                if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
                    score += 20;
                } else {
                    score -= 20;
                }
            } else if (isSemiOpen) {
                if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
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
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            totalMaterial += piece.PieceValue;
        }
    }
    
    // Endgame bonus for active king
    int score = 0;
    if (totalMaterial < 2000) { // Endgame threshold
        // Find kings and evaluate their activity
        for (int i = 0; i < 64; i++) {
            if (board.squares[i].Piece.PieceType == ChessPieceType::KING) {
                int file = i % 8;
                int rank = i / 8;
                int centerDistance = std::max(abs(file - 3.5), abs(rank - 3.5));
                
                if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
                    score += (7 - centerDistance) * 5; // Reward centralized king
                } else {
                    score -= (7 - centerDistance) * 5;
                }
            }
        }
    }
    
    return score;
}

int evaluatePosition(const Board& board) {
    int score = 0;
    
    // Calculate game phase for piece-square table interpolation
    int gamePhase = PieceSquareTables::calculateGamePhase(board);
    
    // Material evaluation with piece-square tables
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int pieceValue = piece.PieceValue;
            int positionalValue = PieceSquareTables::getPieceSquareValue(piece.PieceType, i, piece.PieceColor, gamePhase);
            
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += pieceValue + positionalValue;
            } else {
                score -= pieceValue + positionalValue;
            }
        }
    }
    
    // Enhanced evaluation components
    score += evaluatePassedPawns(board);
    score += evaluateBishopPair(board);
    score += evaluateRooksOnOpenFiles(board);
    score += evaluateEndgame(board);
    score += evaluateMobility(board); // New mobility evaluation
    score += evaluateKingSafety(board); // Enhanced king safety
    score += evaluateHangingPieces(board); // NEW: Detect hanging pieces
    
    return score;
}

// NEW: Evaluate hanging pieces (pieces under attack and undefended)
int evaluateHangingPieces(const Board& board) {
    int score = 0;
    
    // Check each square for hanging pieces
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        // Don't evaluate pawns or kings as "hanging" in the same way
        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) continue;
        
        bool isUnderAttack = false;
        bool isDefended = false;
        
        // Check if piece is under attack by enemy pieces
        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        // Simple attack detection - check if any enemy piece can move to this square
        for (int j = 0; j < 64; j++) {
            const Piece& enemyPiece = board.squares[j].Piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor) continue;
            
            // Check if enemy piece can attack this square
            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }
        
        if (isUnderAttack) {
            // Check if piece is defended by friendly pieces
            for (int j = 0; j < 64; j++) {
                const Piece& friendlyPiece = board.squares[j].Piece;
                if (friendlyPiece.PieceType == ChessPieceType::NONE || friendlyPiece.PieceColor != piece.PieceColor || j == i) continue;
                
                // Check if friendly piece can defend this square
                if (canPieceAttackSquare(board, j, i)) {
                    isDefended = true;
                    break;
                }
            }
            
            // If piece is under attack and not defended, it's hanging
            if (!isDefended) {
                int hangingPenalty = piece.PieceValue * 0.8; // 80% of piece value as penalty
                
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= hangingPenalty;
                } else {
                    score += hangingPenalty;
                }
                
                // Extra penalty for hanging queen
                if (piece.PieceType == ChessPieceType::QUEEN) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= 200; // Extra penalty for hanging queen
                    } else {
                        score += 200;
                    }
                }
            }
        }
    }
    
    return score;
}

// Helper function to check if a piece can attack a square
bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos) {
    const Piece& piece = board.squares[piecePos].Piece;
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
            // Pawn attacks diagonally
            return (rowDiff == direction && (colDiff == 1 || colDiff == -1));
        }
        case ChessPieceType::KNIGHT: {
            return (abs(rowDiff) == 2 && abs(colDiff) == 1) || 
                   (abs(rowDiff) == 1 && abs(colDiff) == 2);
        }
        case ChessPieceType::BISHOP: {
            if (abs(rowDiff) != abs(colDiff)) return false;
            // Check if path is clear
            int rowStep = (rowDiff > 0) ? 1 : -1;
            int colStep = (colDiff > 0) ? 1 : -1;
            for (int i = 1; i < abs(rowDiff); i++) {
                int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
            }
            return true;
        }
        case ChessPieceType::ROOK: {
            if (rowDiff != 0 && colDiff != 0) return false;
            // Check if path is clear
            if (rowDiff == 0) {
                int colStep = (colDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(colDiff); i++) {
                    int checkPos = fromRow * 8 + (fromCol + i * colStep);
                    if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
                }
            } else {
                int rowStep = (rowDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(rowDiff); i++) {
                    int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                    if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
                }
            }
            return true;
        }
        case ChessPieceType::QUEEN: {
            // Queen combines rook and bishop moves
            if (rowDiff == 0 || colDiff == 0 || abs(rowDiff) == abs(colDiff)) {
                // Check if path is clear (similar to rook/bishop logic)
                if (rowDiff == 0) {
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(colDiff); i++) {
                        int checkPos = fromRow * 8 + (fromCol + i * colStep);
                        if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
                    }
                } else if (colDiff == 0) {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                        if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
                    }
                } else {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                        if (board.squares[checkPos].Piece.PieceType != ChessPieceType::NONE) return false;
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
