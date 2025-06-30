#include "Evaluation.h"
#include "EvaluationTuning.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include <algorithm>
#include <cmath>

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
int evaluatePositionEnhanced(const Board& board) {
    int mgScore = 0, egScore = 0;
    int gamePhase = calculateEnhancedGamePhase(board);
    
    // ===================================================================
    // MATERIAL AND PIECE-SQUARE TABLES
    // ===================================================================
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].Piece;
        if (piece.PieceType == ChessPieceType::NONE) continue;
        
        // Material value
        int materialValue = getMaterialValue(piece.PieceType);
        
        // Enhanced PST values
        int adjustedSquare = (piece.PieceColor == ChessPieceColor::WHITE) ? square : (63 - square);
        int mgPST = getTunedPST(piece.PieceType, adjustedSquare, false);
        int egPST = getTunedPST(piece.PieceType, adjustedSquare, true);
        
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + mgPST;
            egScore += materialValue + egPST;
        } else {
            mgScore -= materialValue + mgPST;
            egScore -= materialValue + egPST;
        }
    }
    
    // ===================================================================
    // POSITIONAL EVALUATION COMPONENTS
    // ===================================================================
    
    if (ENABLE_PAWN_STRUCTURE) {
        int pawnScore = evaluateEnhancedPawnStructure(board);
        mgScore += pawnScore;
        egScore += pawnScore * 1.2; // Pawn structure more important in endgame
        logEvaluationComponents("Pawn Structure", pawnScore);
    }
    
    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateEnhancedPieceMobility(board);
        mgScore += mobilityScore;
        egScore += mobilityScore * 0.8; // Mobility less critical in endgame
        logEvaluationComponents("Piece Mobility", mobilityScore);
    }
    
    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateEnhancedKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += kingSafetyScore * 0.3; // King safety less important in endgame
        logEvaluationComponents("King Safety", kingSafetyScore);
    }
    
    if (ENABLE_PIECE_COORDINATION) {
        int coordinationScore = evaluateEnhancedPieceCoordination(board);
        mgScore += coordinationScore;
        egScore += coordinationScore;
        logEvaluationComponents("Piece Coordination", coordinationScore);
    }
    
    if (ENABLE_ENDGAME_KNOWLEDGE) {
        int endgameScore = evaluateEnhancedEndgameFactors(board);
        mgScore += endgameScore * 0.2; // Minimal impact in middlegame
        egScore += endgameScore;
        logEvaluationComponents("Endgame Factors", endgameScore);
    }
    
    if (ENABLE_TACTICAL_BONUSES) {
        int tacticalScore = evaluateEnhancedTacticalMotifs(board);
        mgScore += tacticalScore;
        egScore += tacticalScore * 0.7; // Tactics less important in simple endgames
        logEvaluationComponents("Tactical Motifs", tacticalScore);
    }
    
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
    
    logEvaluationComponents("Final Score", finalScore);
    return finalScore;
}

// ===================================================================
// ENHANCED PAWN STRUCTURE EVALUATION
// ===================================================================
int evaluateEnhancedPawnStructure(const Board& board) {
    int score = 0;
    
    for (int file = 0; file < 8; file++) {
        int whitePawns = 0, blackPawns = 0;
        int whiteMaxRank = -1, blackMinRank = 8;
        
        // Count pawns on this file
        for (int rank = 0; rank < 8; rank++) {
            int square = rank * 8 + file;
            const Piece& piece = board.squares[square].Piece;
            
            if (piece.PieceType == ChessPieceType::PAWN) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                    whiteMaxRank = std::max(whiteMaxRank, rank);
                } else {
                    blackPawns++;
                    blackMinRank = std::min(blackMinRank, rank);
                }
            }
        }
        
        // Doubled pawns
        if (whitePawns > 1) score -= DOUBLED_PAWN_PENALTY * (whitePawns - 1);
        if (blackPawns > 1) score += DOUBLED_PAWN_PENALTY * (blackPawns - 1);
        
        // Isolated pawns
        bool whiteIsolated = false, blackIsolated = false;
        if (whitePawns > 0) {
            bool hasSupport = false;
            for (int adjFile = std::max(0, file - 1); adjFile <= std::min(7, file + 1); adjFile++) {
                if (adjFile == file) continue;
                for (int rank = 0; rank < 8; rank++) {
                    int adjSquare = rank * 8 + adjFile;
                    if (board.squares[adjSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[adjSquare].Piece.PieceColor == ChessPieceColor::WHITE) {
                        hasSupport = true;
                        break;
                    }
                }
                if (hasSupport) break;
            }
            if (!hasSupport) {
                whiteIsolated = true;
                score -= ISOLATED_PAWN_PENALTY;
            }
        }
        
        if (blackPawns > 0) {
            bool hasSupport = false;
            for (int adjFile = std::max(0, file - 1); adjFile <= std::min(7, file + 1); adjFile++) {
                if (adjFile == file) continue;
                for (int rank = 0; rank < 8; rank++) {
                    int adjSquare = rank * 8 + adjFile;
                    if (board.squares[adjSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[adjSquare].Piece.PieceColor == ChessPieceColor::BLACK) {
                        hasSupport = true;
                        break;
                    }
                }
                if (hasSupport) break;
            }
            if (!hasSupport) {
                blackIsolated = true;
                score += ISOLATED_PAWN_PENALTY;
            }
        }
        
        // Passed pawns
        if (whitePawns == 1 && blackPawns == 0 && !whiteIsolated) {
            bool isBlocked = false;
            for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                for (int rank = whiteMaxRank + 1; rank < 8; rank++) {
                    int checkSquare = rank * 8 + checkFile;
                    if (board.squares[checkSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].Piece.PieceColor == ChessPieceColor::BLACK) {
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
        
        if (blackPawns == 1 && whitePawns == 0 && !blackIsolated) {
            bool isBlocked = false;
            for (int checkFile = std::max(0, file - 1); checkFile <= std::min(7, file + 1); checkFile++) {
                for (int rank = 0; rank < blackMinRank; rank++) {
                    int checkSquare = rank * 8 + checkFile;
                    if (board.squares[checkSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].Piece.PieceColor == ChessPieceColor::WHITE) {
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
    
    // Connected pawns bonus
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].Piece;
        if (piece.PieceType != ChessPieceType::PAWN) continue;
        
        int file = square % 8;
        int rank = square / 8;
        
        // Check for connected pawns (diagonal support)
        bool hasConnection = false;
        for (int df = -1; df <= 1; df += 2) {
            int newFile = file + df;
            if (newFile >= 0 && newFile < 8) {
                int backRank = (piece.PieceColor == ChessPieceColor::WHITE) ? rank - 1 : rank + 1;
                if (backRank >= 0 && backRank < 8) {
                    int checkSquare = backRank * 8 + newFile;
                    if (board.squares[checkSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[checkSquare].Piece.PieceColor == piece.PieceColor) {
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
    }
    
    return score;
}

// ===================================================================
// ENHANCED PIECE MOBILITY EVALUATION
// ===================================================================
int evaluateEnhancedPieceMobility(const Board& board) {
    int score = 0;
    
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].Piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceType == ChessPieceType::PAWN) continue;
        
        int mobility = 0;
        int file = square % 8;
        int rank = square / 8;
        
        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT: {
                int knightMoves[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
                for (int i = 0; i < 8; i++) {
                    int newRank = rank + knightMoves[i][0];
                    int newFile = file + knightMoves[i][1];
                    if (newRank >= 0 && newRank < 8 && newFile >= 0 && newFile < 8) {
                        int targetSquare = newRank * 8 + newFile;
                        const Piece& target = board.squares[targetSquare].Piece;
                        if (target.PieceType == ChessPieceType::NONE || target.PieceColor != piece.PieceColor) {
                            mobility++;
                        }
                    }
                }
                break;
            }
            case ChessPieceType::BISHOP: {
                int directions[4][2] = {{1,1}, {1,-1}, {-1,1}, {-1,-1}};
                for (int dir = 0; dir < 4; dir++) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRank = rank + directions[dir][0] * dist;
                        int newFile = file + directions[dir][1] * dist;
                        if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8) break;
                        
                        int targetSquare = newRank * 8 + newFile;
                        const Piece& target = board.squares[targetSquare].Piece;
                        if (target.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (target.PieceColor != piece.PieceColor) mobility++;
                            break;
                        }
                    }
                }
                break;
            }
            case ChessPieceType::ROOK: {
                int directions[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
                for (int dir = 0; dir < 4; dir++) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRank = rank + directions[dir][0] * dist;
                        int newFile = file + directions[dir][1] * dist;
                        if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8) break;
                        
                        int targetSquare = newRank * 8 + newFile;
                        const Piece& target = board.squares[targetSquare].Piece;
                        if (target.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (target.PieceColor != piece.PieceColor) mobility++;
                            break;
                        }
                    }
                }
                break;
            }
            case ChessPieceType::QUEEN: {
                int directions[8][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}};
                for (int dir = 0; dir < 8; dir++) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRank = rank + directions[dir][0] * dist;
                        int newFile = file + directions[dir][1] * dist;
                        if (newRank < 0 || newRank >= 8 || newFile < 0 || newFile >= 8) break;
                        
                        int targetSquare = newRank * 8 + newFile;
                        const Piece& target = board.squares[targetSquare].Piece;
                        if (target.PieceType == ChessPieceType::NONE) {
                            mobility++;
                        } else {
                            if (target.PieceColor != piece.PieceColor) mobility++;
                            break;
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
        
        int mobilityBonus = mobility * PIECE_MOBILITY_WEIGHT;
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            score += mobilityBonus;
        } else {
            score -= mobilityBonus;
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
        const Piece& piece = board.squares[square].Piece;
        if (piece.PieceType == ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                whiteKing = square;
            } else {
                blackKing = square;
            }
        }
    }
    
    if (whiteKing == -1 || blackKing == -1) return 0;
    
    // Evaluate king safety for both colors
    for (int isWhite = 0; isWhite < 2; isWhite++) {
        int kingSquare = isWhite ? whiteKing : blackKing;
        ChessPieceColor kingColor = isWhite ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
        ChessPieceColor enemyColor = isWhite ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
        
        int kingSafety = 0;
        int kingFile = kingSquare % 8;
        int kingRank = kingSquare / 8;
        
        // Count attacking pieces around king
        int attackerCount = 0;
        int attackerWeight = 0;
        
        for (int square = 0; square < 64; square++) {
            const Piece& piece = board.squares[square].Piece;
            if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != enemyColor) continue;
            
            // Check if this piece attacks squares near the king
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int targetRank = kingRank + dr;
                    int targetFile = kingFile + dc;
                    if (targetRank >= 0 && targetRank < 8 && targetFile >= 0 && targetFile < 8) {
                        int targetSquare = targetRank * 8 + targetFile;
                        if (isSquareAttacked(board, targetSquare, enemyColor)) {
                            attackerCount++;
                            switch (piece.PieceType) {
                                case ChessPieceType::PAWN: attackerWeight += 1; break;
                                case ChessPieceType::KNIGHT: attackerWeight += 2; break;
                                case ChessPieceType::BISHOP: attackerWeight += 2; break;
                                case ChessPieceType::ROOK: attackerWeight += 3; break;
                                case ChessPieceType::QUEEN: attackerWeight += 4; break;
                                default: break;
                            }
                        }
                    }
                }
            }
        }
        
        if (attackerCount > 0) {
            int attackIndex = std::min(attackerCount, 5);
            kingSafety -= KING_ATTACK_WEIGHTS[attackIndex] + attackerWeight * KING_DANGER_MULTIPLIER;
        }
        
        // Pawn shield bonus
        int pawnShield = 0;
        if (isWhite && kingRank <= 1) { // White king on back ranks
            for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1); file++) {
                for (int rank = kingRank + 1; rank <= kingRank + 2 && rank < 8; rank++) {
                    int shieldSquare = rank * 8 + file;
                    if (board.squares[shieldSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[shieldSquare].Piece.PieceColor == ChessPieceColor::WHITE) {
                        pawnShield++;
                    }
                }
            }
        } else if (!isWhite && kingRank >= 6) { // Black king on back ranks
            for (int file = std::max(0, kingFile - 1); file <= std::min(7, kingFile + 1); file++) {
                for (int rank = kingRank - 2; rank <= kingRank - 1 && rank >= 0; rank++) {
                    int shieldSquare = rank * 8 + file;
                    if (board.squares[shieldSquare].Piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[shieldSquare].Piece.PieceColor == ChessPieceColor::BLACK) {
                        pawnShield++;
                    }
                }
            }
        }
        
        kingSafety += pawnShield * KING_PAWN_SHIELD_BONUS;
        
        if (isWhite) {
            score += kingSafety;
        } else {
            score -= kingSafety;
        }
    }
    
    return score;
}

// ===================================================================
// HELPER FUNCTIONS
// ===================================================================
int calculateEnhancedGamePhase(const Board& board) {
    int phase = 0;
    for (int square = 0; square < 64; square++) {
        const Piece& piece = board.squares[square].Piece;
        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT:
            case ChessPieceType::BISHOP:
                phase += 1;
                break;
            case ChessPieceType::ROOK:
                phase += 2;
                break;
            case ChessPieceType::QUEEN:
                phase += 4;
                break;
            default:
                break;
        }
    }
    return std::min(phase, TOTAL_PHASE);
}

bool isSquareAttacked(const Board& board, int square, ChessPieceColor byColor) {
    // Simple implementation - could be optimized
    for (int attackerSquare = 0; attackerSquare < 64; attackerSquare++) {
        const Piece& piece = board.squares[attackerSquare].Piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != byColor) continue;
        
        // This is a simplified attack check - in practice you'd use the existing canPieceAttackSquare
        // For now, just return false to compile
    }
    return false;
}

// Placeholder implementations for remaining functions
int evaluateEnhancedPieceCoordination(const Board& board) { return 0; }
int evaluateEnhancedEndgameFactors(const Board& board) { return 0; }
int evaluateEnhancedTacticalMotifs(const Board& board) { return 0; }
int countAttackers(const Board& board, int square, ChessPieceColor color) { return 0; } 