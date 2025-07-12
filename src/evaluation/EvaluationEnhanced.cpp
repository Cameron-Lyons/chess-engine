#include "EvaluationEnhanced.h"
#include "Evaluation.h"
#include "EvaluationTuning.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace EvaluationParams;

// ===================================================================
// ENHANCED EVALUATION SYSTEM
// ===================================================================
// This replaces the old evaluation with modern, tunable techniques

// Forward declarations for helper functions
int evaluateEnhancedPawnStructure(const Board& board);
int evaluateEnhancedPieceMobility(const Board& board);
int evaluateEnhancedKingSafety(const Board& board);
int evaluateEnhancedPieceCoordination(const Board& board);
int evaluateEnhancedEndgameFactors(const Board& board);
int evaluateEnhancedTacticalMotifs(const Board& board);
int calculateEnhancedGamePhase(const Board& board);
bool isSquareAttacked(const Board& board, int square, ChessPieceColor byColor);
int countAttackers(const Board& board, int square, ChessPieceColor color);

// ===================================================================
// MAIN ENHANCED EVALUATION FUNCTION
// ===================================================================
int EnhancedEvaluation::evaluatePosition(const Board& board) {
    int mgScore = 0;  // Middlegame score
    int egScore = 0;  // Endgame score
    
    // Calculate game phase (0 = endgame, 24 = opening)
    int gamePhase = calculateEnhancedGamePhase(board);
    float phaseRatio = static_cast<float>(gamePhase) / 24.0f;
    phaseRatio = std::clamp(phaseRatio, 0.0f, 1.0f);
    
    // Material and piece-square table evaluation
    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        int materialValue = piece.PieceValue;
        int adjustedSquare = (piece.PieceColor == ChessPieceColor::WHITE) ? square : 63 - square;
        
        // Get piece-square table values for both phases
        int mgPST = getPieceSquareValue(piece.PieceType, adjustedSquare, piece.PieceColor);
        int egPST = getPieceSquareValue(piece.PieceType, adjustedSquare, piece.PieceColor);
        
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + mgPST;
            egScore += materialValue + egPST;
        } else {
            mgScore -= materialValue + mgPST;
            egScore -= materialValue + egPST;
        }
    }
    
    // Enhanced positional evaluations
    int pawnStructureScore = evaluateEnhancedPawnStructure(board);
    int mobilityScore = evaluateEnhancedPieceMobility(board);
    int kingSafetyScore = evaluateEnhancedKingSafety(board);
    int coordinationScore = evaluateEnhancedPieceCoordination(board);
    int endgameScore = evaluateEnhancedEndgameFactors(board);
    int tacticalScore = evaluateEnhancedTacticalMotifs(board);
    
    // Apply phase-dependent weights
    mgScore += pawnStructureScore;
    egScore += pawnStructureScore * 1.2f; // Pawn structure more important in endgame
    
    mgScore += mobilityScore;
    egScore += mobilityScore * 0.8f; // Mobility less critical in endgame
    
    mgScore += kingSafetyScore;
    egScore += kingSafetyScore * 0.3f; // King safety less important in endgame
    
    mgScore += coordinationScore;
    egScore += coordinationScore * 1.1f; // Coordination remains important
    
    mgScore += endgameScore * 0.5f; // Endgame factors less important in middlegame
    egScore += endgameScore;
    
    mgScore += tacticalScore;
    egScore += tacticalScore * 0.7f; // Tactical awareness less critical in endgame
    
    // Tempo bonus
    if (board.turn == ChessPieceColor::WHITE) {
        mgScore += EvaluationWeights::TEMPO_WEIGHT;
        egScore += EvaluationWeights::TEMPO_WEIGHT / 2;
    } else {
        mgScore -= EvaluationWeights::TEMPO_WEIGHT;
        egScore -= EvaluationWeights::TEMPO_WEIGHT / 2;
    }
    
    // Phase interpolation
    int finalScore = static_cast<int>(mgScore * (1.0f - phaseRatio) + egScore * phaseRatio);
    
    return finalScore;
}

// ===================================================================
// ENHANCED PAWN STRUCTURE EVALUATION
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
        
        // Doubled pawns penalty
        if (whitePawns > 1) {
            score -= EvaluationWeights::DOUBLED_PAWN_PENALTY * (whitePawns - 1);
        }
        if (blackPawns > 1) {
            score += EvaluationWeights::DOUBLED_PAWN_PENALTY * (blackPawns - 1);
        }
        
        // Isolated pawns penalty
        bool whiteIsolated = true, blackIsolated = true;
        for (int adjFile = std::max(0, file - 1); adjFile <= std::min(7, file + 1); adjFile++) {
            if (adjFile == file) continue;
            
            for (int rank = 0; rank < 8; rank++) {
                int adjSquare = rank * 8 + adjFile;
                const Piece& adjPiece = board.squares[adjSquare].piece;
                
                if (adjPiece.PieceType == ChessPieceType::PAWN) {
                    if (adjPiece.PieceColor == ChessPieceColor::WHITE) {
                        whiteIsolated = false;
                    } else {
                        blackIsolated = false;
                    }
                }
            }
        }
        
        if (whiteIsolated && whitePawns > 0) {
            score -= EvaluationWeights::ISOLATED_PAWN_PENALTY;
        }
        if (blackIsolated && blackPawns > 0) {
            score += EvaluationWeights::ISOLATED_PAWN_PENALTY;
        }
        
        // Passed pawns bonus
        for (int rank = 0; rank < 8; rank++) {
            int square = rank * 8 + file;
            const Piece& piece = board.squares[square].piece;
            
            if (piece.PieceType == ChessPieceType::PAWN) {
                bool isPassed = true;
                
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    // Check if there are enemy pawns ahead or on adjacent files
                    for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                        for (int checkRank = rank + 1; checkRank < 8; checkRank++) {
                            int checkSquare = checkRank * 8 + checkFile;
                            const Piece& checkPiece = board.squares[checkSquare].piece;
                            if (checkPiece.PieceType == ChessPieceType::PAWN && 
                                checkPiece.PieceColor == ChessPieceColor::BLACK) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    
                    if (isPassed) {
                        int passedBonus = (6 - rank) * EvaluationWeights::PASSED_PAWN_BONUS + 10;
                        score += passedBonus;
                    }
                } else {
                    // Check if there are enemy pawns ahead or on adjacent files
                    for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                        for (int checkRank = rank - 1; checkRank >= 0; checkRank--) {
                            int checkSquare = checkRank * 8 + checkFile;
                            const Piece& checkPiece = board.squares[checkSquare].piece;
                            if (checkPiece.PieceType == ChessPieceType::PAWN && 
                                checkPiece.PieceColor == ChessPieceColor::WHITE) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) break;
                    }
                    
                    if (isPassed) {
                        int passedBonus = rank * EvaluationWeights::PASSED_PAWN_BONUS + 10;
                        score -= passedBonus;
                    }
                }
            }
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED PIECE MOBILITY EVALUATION
// ===================================================================
int evaluateEnhancedPieceMobility(const Board& board) {
    int score = 0;
    
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        int mobility = 0;
        int row = square / 8;
        int col = square % 8;
        
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
                score += piece.PieceColor == ChessPieceColor::WHITE ? 
                        mobility * EvaluationWeights::KNIGHT_MOBILITY_WEIGHT : 
                        -mobility * EvaluationWeights::KNIGHT_MOBILITY_WEIGHT;
                break;
            }
            case ChessPieceType::BISHOP: {
                // Check all 4 diagonal directions
                int directions[][2] = {{-1,-1}, {-1,1}, {1,-1}, {1,1}};
                for (auto& dir : directions) {
                    int newRow = row + dir[0];
                    int newCol = col + dir[1];
                    while (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
                        }
                        newRow += dir[0];
                        newCol += dir[1];
                    }
                }
                score += piece.PieceColor == ChessPieceColor::WHITE ? 
                        mobility * EvaluationWeights::BISHOP_MOBILITY_WEIGHT : 
                        -mobility * EvaluationWeights::BISHOP_MOBILITY_WEIGHT;
                break;
            }
            case ChessPieceType::ROOK: {
                // Check all 4 orthogonal directions
                int directions[][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
                for (auto& dir : directions) {
                    int newRow = row + dir[0];
                    int newCol = col + dir[1];
                    while (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
                        }
                        newRow += dir[0];
                        newCol += dir[1];
                    }
                }
                score += piece.PieceColor == ChessPieceColor::WHITE ? 
                        mobility * EvaluationWeights::ROOK_MOBILITY_WEIGHT : 
                        -mobility * EvaluationWeights::ROOK_MOBILITY_WEIGHT;
                break;
            }
            case ChessPieceType::QUEEN: {
                // Check all 8 directions
                int directions[][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
                for (auto& dir : directions) {
                    int newRow = row + dir[0];
                    int newCol = col + dir[1];
                    while (newRow >= 0 && newRow < 8 && newCol >= 0 && newCol < 8) {
                        int pos = newRow * 8 + newCol;
                        if (board.squares[pos].piece.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (board.squares[pos].piece.PieceColor != piece.PieceColor) {
                                mobility++; // Can capture
                            }
                            break; // Blocked
                        }
                        newRow += dir[0];
                        newCol += dir[1];
                    }
                }
                score += piece.PieceColor == ChessPieceColor::WHITE ? 
                        mobility * EvaluationWeights::QUEEN_MOBILITY_WEIGHT : 
                        -mobility * EvaluationWeights::QUEEN_MOBILITY_WEIGHT;
                break;
            }
            default:
                break;
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED KING SAFETY EVALUATION
// ===================================================================
int evaluateEnhancedKingSafety(const Board& board) {
    int score = 0;
    
    // Find kings
    int whiteKing = -1, blackKing = -1;
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whiteKing = square;
            } else {
                blackKing = square;
            }
        }
    }
    
    if (whiteKing != -1) {
        score += evaluateKingSafetyForColor(board, whiteKing, ChessPieceColor::WHITE);
    }
    if (blackKing != -1) {
        score -= evaluateKingSafetyForColor(board, blackKing, ChessPieceColor::BLACK);
    }
    
    return score;
}

int evaluateKingSafetyForColor(const Board& board, int kingSquare, ChessPieceColor color) {
    int score = 0;
    int kingRow = kingSquare / 8;
    int kingCol = kingSquare % 8;
    
    // Pawn shield evaluation
    int pawnShield = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int checkRow = kingRow + dr;
            int checkCol = kingCol + dc;
            if (checkRow >= 0 && checkRow < 8 && checkCol >= 0 && checkCol < 8) {
                int checkSquare = checkRow * 8 + checkCol;
                const Piece& piece = board.squares[checkSquare].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
                    pawnShield++;
                }
            }
        }
    }
    score += pawnShield * EvaluationWeights::PAWN_SHIELD_BONUS;
    
    // Open file penalty
    bool isOnOpenFile = true;
    for (int rank = 0; rank < 8; rank++) {
        int square = rank * 8 + kingCol;
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            isOnOpenFile = false;
            break;
        }
    }
    if (isOnOpenFile) {
        score -= EvaluationWeights::OPEN_FILE_PENALTY;
    }
    
    // Semi-open file penalty
    bool isOnSemiOpenFile = true;
    for (int rank = 0; rank < 8; rank++) {
        int square = rank * 8 + kingCol;
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
            isOnSemiOpenFile = false;
            break;
        }
    }
    if (isOnSemiOpenFile) {
        score -= EvaluationWeights::SEMI_OPEN_FILE_PENALTY;
    }
    
    return score;
}

// ===================================================================
// ENHANCED PIECE COORDINATION EVALUATION
// ===================================================================
int evaluateEnhancedPieceCoordination(const Board& board) {
    int score = 0;
    
    // Bishop pair bonus
    int whiteBishops = 0, blackBishops = 0;
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::BISHOP) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whiteBishops++;
            } else {
                blackBishops++;
            }
        }
    }
    
    if (whiteBishops >= 2) score += 50;
    if (blackBishops >= 2) score -= 50;
    
    // Rook coordination on open files
    for (int file = 0; file < 8; file++) {
        bool isOpenFile = true;
        for (int rank = 0; rank < 8; rank++) {
            int square = rank * 8 + file;
            const Piece& piece = board.squares[square].piece;
            if (piece.PieceType == ChessPieceType::PAWN) {
                isOpenFile = false;
                break;
            }
        }
        
        if (isOpenFile) {
            int whiteRooks = 0, blackRooks = 0;
            for (int rank = 0; rank < 8; rank++) {
                int square = rank * 8 + file;
                const Piece& piece = board.squares[square].piece;
                if (piece.PieceType == ChessPieceType::ROOK) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        whiteRooks++;
                    } else {
                        blackRooks++;
                    }
                }
            }
            
            if (whiteRooks >= 2) score += EvaluationWeights::CONNECTED_ROOKS_BONUS;
            if (blackRooks >= 2) score -= EvaluationWeights::CONNECTED_ROOKS_BONUS;
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED ENDGAME FACTORS
// ===================================================================
int evaluateEnhancedEndgameFactors(const Board& board) {
    int score = 0;
    
    // King activity in endgame
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::KING) {
            int row = square / 8;
            int col = square % 8;
            
            // Encourage king to move to center in endgame
            int centerDistance = std::abs(row - 3.5) + std::abs(col - 3.5);
            int kingActivity = (7 - centerDistance) * EvaluationWeights::KING_ACTIVITY_WEIGHT;
            
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += kingActivity;
            } else {
                score -= kingActivity;
            }
        }
    }
    
    return score;
}

// ===================================================================
// ENHANCED TACTICAL MOTIFS
// ===================================================================
int evaluateEnhancedTacticalMotifs(const Board& board) {
    int score = 0;
    
    // Hanging pieces evaluation
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE || 
            piece.PieceType == ChessPieceType::PAWN || 
            piece.PieceType == ChessPieceType::KING) continue;
        
        bool isUnderAttack = false;
        bool isDefended = false;
        
        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE) ? 
                                    ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        // Check if piece is under attack
        for (int attackerSquare = 0; attackerSquare < 64; attackerSquare++) {
            const Piece& attacker = board.squares[attackerSquare].piece;
            if (attacker.PieceType == ChessPieceType::NONE || 
                attacker.PieceColor != enemyColor) continue;
            
            if (canPieceAttackSquare(board, attackerSquare, square)) {
                isUnderAttack = true;
                break;
            }
        }
        
        if (isUnderAttack) {
            // Check if piece is defended
            for (int defenderSquare = 0; defenderSquare < 64; defenderSquare++) {
                const Piece& defender = board.squares[defenderSquare].piece;
                if (defender.PieceType == ChessPieceType::NONE || 
                    defender.PieceColor != piece.PieceColor || 
                    defenderSquare == square) continue;
                
                if (canPieceAttackSquare(board, defenderSquare, square)) {
                    isDefended = true;
                    break;
                }
            }
            
            if (!isDefended) {
                int hangingPenalty = piece.PieceValue * 0.8f;
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= hangingPenalty;
                } else {
                    score += hangingPenalty;
                }
            }
        }
    }
    
    return score;
}

// ===================================================================
// HELPER FUNCTIONS
// ===================================================================
int calculateEnhancedGamePhase(const Board& board) {
    int gamePhase = 0;
    
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].piece;
        switch (piece.PieceType) {
            case ChessPieceType::PAWN:   gamePhase += 0; break;
            case ChessPieceType::KNIGHT: gamePhase += 1; break;
            case ChessPieceType::BISHOP: gamePhase += 1; break;
            case ChessPieceType::ROOK:   gamePhase += 2; break;
            case ChessPieceType::QUEEN:  gamePhase += 4; break;
            case ChessPieceType::KING:   gamePhase += 0; break;
            default: break;
        }
    }
    
    return gamePhase;
}

bool isSquareAttacked(const Board& board, int square, ChessPieceColor byColor) {
    for (int attackerSquare = 0; attackerSquare < 64; attackerSquare++) {
        const Piece& attacker = board.squares[attackerSquare].piece;
        if (attacker.PieceType == ChessPieceType::NONE || 
            attacker.PieceColor != byColor) continue;
        
        if (canPieceAttackSquare(board, attackerSquare, square)) {
            return true;
        }
    }
    return false;
}

int countAttackers(const Board& board, int square, ChessPieceColor color) {
    int count = 0;
    for (int attackerSquare = 0; attackerSquare < 64; attackerSquare++) {
        const Piece& attacker = board.squares[attackerSquare].piece;
        if (attacker.PieceType == ChessPieceType::NONE || 
            attacker.PieceColor != color) continue;
        
        if (canPieceAttackSquare(board, attackerSquare, square)) {
            count++;
        }
    }
    return count;
} 