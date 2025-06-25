#include "Evaluation.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include <algorithm>


int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {
    int value = 0;
    switch (pieceType) {
        case ChessPieceType::PAWN:
            value = PAWN_TABLE[position];
            break;
        case ChessPieceType::KNIGHT:
            value = KNIGHT_TABLE[position];
            break;
        case ChessPieceType::BISHOP:
            value = BISHOP_TABLE[position];
            break;
        case ChessPieceType::ROOK:
            value = ROOK_TABLE[position];
            break;
        case ChessPieceType::QUEEN:
            value = QUEEN_TABLE[position];
            break;
        case ChessPieceType::KING:
            value = KING_TABLE[position];
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

int evaluateMobility(const Board& board) {
    int whiteMobility = 0, blackMobility = 0;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType != ChessPieceType::NONE) {
            int moves = board.squares[i].Piece.ValidMoves.size();
            if (board.squares[i].Piece.PieceColor == ChessPieceColor::WHITE) {
                whiteMobility += moves;
            } else {
                blackMobility += moves;
            }
        }
    }
    return (whiteMobility - blackMobility) * 10;
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

int evaluateKingSafety(const Board& board, ChessPieceColor color) {
    int safety = 0;
    
    int kingSquare = -1;
    for (int i = 0; i < 64; i++) {
        if (board.squares[i].Piece.PieceType == ChessPieceType::KING && 
            board.squares[i].Piece.PieceColor == color) {
            kingSquare = i;
            break;
        }
    }
    
    if (kingSquare == -1) return 0; // Should never happen
    
    int kingFile = kingSquare % 8;
    int kingRank = kingSquare / 8;
    
    if (color == ChessPieceColor::WHITE) {
        for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1); file++) {
            int sq2 = 8 + file;
            if (board.squares[sq2].Piece.PieceType == ChessPieceType::PAWN && 
                board.squares[sq2].Piece.PieceColor == ChessPieceColor::WHITE) {
                safety += KING_SAFETY_PAWN_SHIELD_BONUS;
            }
            
            int sq3 = 16 + file;
            if (board.squares[sq3].Piece.PieceType == ChessPieceType::PAWN && 
                board.squares[sq3].Piece.PieceColor == ChessPieceColor::WHITE) {
                safety += KING_SAFETY_PAWN_SHIELD_BONUS / 2;
            }
        }
    } else {
        for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1); file++) {
            int sq7 = 48 + file;
            if (board.squares[sq7].Piece.PieceType == ChessPieceType::PAWN && 
                board.squares[sq7].Piece.PieceColor == ChessPieceColor::BLACK) {
                safety += KING_SAFETY_PAWN_SHIELD_BONUS;
            }
            
            int sq6 = 40 + file;
            if (board.squares[sq6].Piece.PieceType == ChessPieceType::PAWN && 
                board.squares[sq6].Piece.PieceColor == ChessPieceColor::BLACK) {
                safety += KING_SAFETY_PAWN_SHIELD_BONUS / 2;
            }
        }
    }
    
    for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1); file++) {
        bool hasOwnPawn = false;
        bool hasEnemyPawn = false;
        
        for (int rank = 0; rank < 8; rank++) {
            int sq = rank * 8 + file;
            if (board.squares[sq].Piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[sq].Piece.PieceColor == color) {
                    hasOwnPawn = true;
                } else {
                    hasEnemyPawn = true;
                }
            }
        }
        
        if (!hasOwnPawn && !hasEnemyPawn) {
            safety -= KING_SAFETY_OPEN_FILE_PENALTY;
        } else if (!hasOwnPawn && hasEnemyPawn) {
            safety -= KING_SAFETY_SEMI_OPEN_FILE_PENALTY;
        }
    }
    
    ChessPieceColor enemyColor = (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    int attackUnits = 0;
    int attackingPieces = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != enemyColor) {
            continue;
        }
        
        bool attacksKingZone = false;
        int pieceFile = i % 8;
        int pieceRank = i / 8;
        
        for (int dr = -1; dr <= 1; dr++) {
            for (int df = -1; df <= 1; df++) {
                int targetRank = kingRank + dr;
                int targetFile = kingFile + df;
                if (targetRank >= 0 && targetRank < 8 && targetFile >= 0 && targetFile < 8) {
                    int targetSquare = targetRank * 8 + targetFile;
                    
                    switch (piece.PieceType) {
                        case ChessPieceType::PAWN: {
                            if (enemyColor == ChessPieceColor::WHITE) {
                                if (pieceRank + 1 == targetRank && abs(pieceFile - targetFile) == 1) {
                                    attacksKingZone = true;
                                }
                            } else {
                                if (pieceRank - 1 == targetRank && abs(pieceFile - targetFile) == 1) {
                                    attacksKingZone = true;
                                }
                            }
                            break;
                        }
                        case ChessPieceType::KNIGHT: {
                            int dr = abs(pieceRank - targetRank);
                            int df = abs(pieceFile - targetFile);
                            if ((dr == 2 && df == 1) || (dr == 1 && df == 2)) {
                                attacksKingZone = true;
                            }
                            break;
                        }
                        case ChessPieceType::BISHOP:
                        case ChessPieceType::QUEEN: {
                            if (abs(pieceRank - targetRank) == abs(pieceFile - targetFile)) {
                                attacksKingZone = true;
                            }
                            if (piece.PieceType == ChessPieceType::QUEEN) {
                                if (pieceRank == targetRank || pieceFile == targetFile) {
                                    attacksKingZone = true;
                                }
                            }
                            break;
                        }
                        case ChessPieceType::ROOK: {
                            if (pieceRank == targetRank || pieceFile == targetFile) {
                                attacksKingZone = true;
                            }
                            break;
                        }
                        case ChessPieceType::KING: {
                            if (abs(pieceRank - targetRank) <= 1 && abs(pieceFile - targetFile) <= 1) {
                                attacksKingZone = true;
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
                if (attacksKingZone) break;
            }
            if (attacksKingZone) break;
        }
        
        if (attacksKingZone) {
            attackingPieces++;
            int pieceTypeIndex = static_cast<int>(piece.PieceType);
            if (pieceTypeIndex >= 0 && pieceTypeIndex < 7) {
                attackUnits += KING_SAFETY_ATTACK_WEIGHT[pieceTypeIndex];
            }
        }
    }
    
    if (attackingPieces >= 2) {
        attackUnits = std::min(attackUnits, KING_SAFETY_ATTACK_UNITS_MAX);
        safety -= (attackUnits * attackUnits) / 256;
    }
    
    return safety;
}

int evaluatePosition(const Board& board) {
    int score = 0;
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE) {
            int materialValue = piece.PieceValue;
            int positionalValue = getPieceSquareValue(piece.PieceType, i, piece.PieceColor);
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += materialValue + positionalValue;
            } else {
                score -= materialValue + positionalValue;
            }
        }
    }
    score += evaluatePawnStructure(board);
    score += evaluateMobility(board);
    score += evaluateCenterControl(board);
    
    score += evaluateKingSafety(board, ChessPieceColor::WHITE);
    score -= evaluateKingSafety(board, ChessPieceColor::BLACK);
    
    return score;
}
