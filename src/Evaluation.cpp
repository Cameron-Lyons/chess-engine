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
    int whiteQueens = 0, blackQueens = 0;
    int whiteRooks = 0, blackRooks = 0;
    int whiteMinorPieces = 0, blackMinorPieces = 0;
    
    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].Piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            totalMaterial += piece.PieceValue;
            
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                switch (piece.PieceType) {
                    case ChessPieceType::QUEEN: whiteQueens++; break;
                    case ChessPieceType::ROOK: whiteRooks++; break;
                    case ChessPieceType::BISHOP:
                    case ChessPieceType::KNIGHT: whiteMinorPieces++; break;
                    default: break;
                }
            } else {
                switch (piece.PieceType) {
                    case ChessPieceType::QUEEN: blackQueens++; break;
                    case ChessPieceType::ROOK: blackRooks++; break;
                    case ChessPieceType::BISHOP:
                    case ChessPieceType::KNIGHT: blackMinorPieces++; break;
                    default: break;
                }
            }
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
    
    // Material and positional evaluation
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
    
    // Advanced positional factors
    score += evaluatePawnStructure(board);
    score += evaluateMobility(board);
    score += evaluateCenterControl(board);
    score += evaluatePassedPawns(board);
    score += evaluateBishopPair(board);
    score += evaluateRooksOnOpenFiles(board);
    score += evaluateEndgame(board);
    
    // King safety
    score += evaluateKingSafety(board, ChessPieceColor::WHITE);
    score -= evaluateKingSafety(board, ChessPieceColor::BLACK);
    
    return score;
}
