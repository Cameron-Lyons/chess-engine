#include "LMREnhanced.h"
#include "ValidMoves.h"
#include "search.h"
#include "../evaluation/Evaluation.h"

namespace LMREnhanced {

// Helper function to get piece value
static int getPieceValue(ChessPieceType type) {
    switch (type) {
        case ChessPieceType::PAWN: return 100;
        case ChessPieceType::KNIGHT: return 320;
        case ChessPieceType::BISHOP: return 330;
        case ChessPieceType::ROOK: return 500;
        case ChessPieceType::QUEEN: return 900;
        case ChessPieceType::KING: return 20000;
        default: return 0;
    }
}

// Initialize global reduction table
ReductionTable reductionTable;

MoveClassification classifyMove(const Board& board, const std::pair<int, int>& move,
                               const KillerMoves& killers, int ply,
                               const ThreadSafeHistory& history,
                               const std::pair<int, int>& hashMove,
                               int moveNumber) {
    MoveClassification classification;
    
    classification.moveNumber = moveNumber;
    
    // Check if it's the hash move
    classification.isHashMove = (move == hashMove);
    
    // Check if it's a capture
    classification.isCapture = (board.squares[move.second].piece.PieceType != ChessPieceType::NONE);
    
    // Check if it's a killer move
    classification.isKiller = killers.isKiller(ply, move);
    
    // Get history score
    classification.historyScore = history.get(move.first, move.second);
    
    // Check for promotion
    ChessPieceType movingPiece = board.squares[move.first].piece.PieceType;
    if (movingPiece == ChessPieceType::PAWN) {
        int toRank = move.second / 8;
        if ((board.turn == ChessPieceColor::WHITE && toRank == 7) ||
            (board.turn == ChessPieceColor::BLACK && toRank == 0)) {
            classification.isPromotion = true;
        }
        
        // Check for passed pawn push
        int toFile = move.second % 8;
        bool isPassed = true;
        
        // Simple passed pawn detection
        if (board.turn == ChessPieceColor::WHITE) {
            for (int r = toRank + 1; r < 8; ++r) {
                for (int f = std::max(0, toFile - 1); f <= std::min(7, toFile + 1); ++f) {
                    int sq = r * 8 + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::BLACK) {
                        isPassed = false;
                        break;
                    }
                }
            }
        } else {
            for (int r = toRank - 1; r >= 0; --r) {
                for (int f = std::max(0, toFile - 1); f <= std::min(7, toFile + 1); ++f) {
                    int sq = r * 8 + f;
                    if (board.squares[sq].piece.PieceType == ChessPieceType::PAWN &&
                        board.squares[sq].piece.PieceColor == ChessPieceColor::WHITE) {
                        isPassed = false;
                        break;
                    }
                }
            }
        }
        
        classification.isPassed = isPassed && (board.turn == ChessPieceColor::WHITE ? toRank >= 4 : toRank <= 3);
    }
    
    // Check for castling
    if (movingPiece == ChessPieceType::KING && std::abs(move.second - move.first) == 2) {
        classification.isCastling = true;
    }
    
    // Check if move gives check (simplified)
    Board tempBoard = board;
    tempBoard.movePiece(move.first, move.second);
    tempBoard.turn = (board.turn == ChessPieceColor::WHITE) ? 
                     ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    classification.givesCheck = isInCheck(tempBoard, tempBoard.turn);
    
    // Check for tactical moves (simplified - would need more sophisticated detection)
    // Check if the move attacks a higher value piece
    if (!classification.isCapture && movingPiece != ChessPieceType::NONE) {
        // Generate moves from the destination square to see what it attacks
        std::vector<std::pair<int, int>> attacks;
        if (movingPiece == ChessPieceType::PAWN) {
            attacks = generatePawnMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::KNIGHT) {
            attacks = generateKnightMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::BISHOP) {
            attacks = generateBishopMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::ROOK) {
            attacks = generateRookMoves(tempBoard, board.turn);
        } else if (movingPiece == ChessPieceType::QUEEN) {
            attacks = generateQueenMoves(tempBoard, board.turn);
        }
        
        int movingValue = getPieceValue(movingPiece);
        for (auto& attack : attacks) {
            if (attack.first == move.second) {
                int attackSq = attack.second;
                ChessPieceType attackedPiece = tempBoard.squares[attackSq].piece.PieceType;
                if (attackedPiece != ChessPieceType::NONE &&
                    tempBoard.squares[attackSq].piece.PieceColor != board.turn) {
                    int attackedValue = getPieceValue(attackedPiece);
                    if (attackedValue > movingValue) {
                        classification.isTactical = true;
                        break;
                    }
                }
            }
        }
    }
    
    return classification;
}

PositionContext evaluatePosition(const Board& board, int staticEval, 
                               int previousEval, bool isPVNode) {
    PositionContext context;
    
    context.isPVNode = isPVNode;
    context.staticEval = staticEval;
    
    // Check if in check
    context.inCheck = isInCheck(board, board.turn);
    
    // Evaluate if position is improving
    context.evalTrend = staticEval - previousEval;
    context.isImproving = context.evalTrend > 20;
    
    // Calculate game phase (0 = opening, 256 = endgame)
    int totalMaterial = 0;
    int pieceCount = 0;
    for (int sq = 0; sq < 64; ++sq) {
        ChessPieceType piece = board.squares[sq].piece.PieceType;
        if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING) {
            totalMaterial += getPieceValue(piece);
            if (piece != ChessPieceType::PAWN) {
                pieceCount++;
            }
        }
    }
    
    // Simple phase calculation
    const int startingMaterial = 7800; // Approximate starting material
    context.gamePhase = std::min(256, (256 * (startingMaterial - totalMaterial)) / startingMaterial);
    context.isEndgame = (context.gamePhase > 200 || pieceCount <= 6);
    
    // Check if position is tactical
    // Simplified check - look for hanging pieces or attacks on valuable pieces
    context.isTactical = false;
    
    // Check if any of our pieces are hanging
    for (int sq = 0; sq < 64; ++sq) {
        if (board.squares[sq].piece.PieceType != ChessPieceType::NONE &&
            board.squares[sq].piece.PieceColor == board.turn) {
            
            // Check if this piece is attacked
            ChessPieceColor enemyColor = (board.turn == ChessPieceColor::WHITE) ?
                                       ChessPieceColor::BLACK : ChessPieceColor::WHITE;
            
            // Simple attack detection - check if any enemy piece can capture our piece
            auto enemyMoves = generateBitboardMoves(const_cast<Board&>(board), enemyColor);
            for (auto& move : enemyMoves) {
                if (move.second == sq) {
                    // Check if this is an undefended piece or bad trade
                    ChessPieceType attackedPiece = board.squares[sq].piece.PieceType;
                    ChessPieceType attackingPiece = board.squares[move.first].piece.PieceType;
                    if (getPieceValue(attackedPiece) >= getPieceValue(attackingPiece)) {
                        context.isTactical = true;
                        break;
                    }
                }
            }
            if (context.isTactical) break;
        }
    }
    
    // Singular move detection would require analyzing all moves
    // This is simplified - in practice would need deeper analysis
    context.isSingular = false;
    
    return context;
}

} // namespace LMREnhanced