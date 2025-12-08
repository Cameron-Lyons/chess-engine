#include "PositionAnalysis.h"
#include "../core/BitboardMoves.h"
#include "../search/ValidMoves.h"
#include "Evaluation.h"
#include <algorithm>
#include <iostream>
#include <sstream>

PositionAnalysis PositionAnalyzer::analyzePosition(const Board& board) {
    PositionAnalysis analysis;

    // Count material and pieces
    analysis.whiteMaterial = countMaterial(board, ChessPieceColor::WHITE);
    analysis.blackMaterial = countMaterial(board, ChessPieceColor::BLACK);
    analysis.materialBalance = analysis.whiteMaterial - analysis.blackMaterial;

    // Count pieces by type
    analysis.whitePieces = 0;
    analysis.blackPieces = 0;
    analysis.whitePawns = 0;
    analysis.blackPawns = 0;
    analysis.whiteKnights = 0;
    analysis.blackKnights = 0;
    analysis.whiteBishops = 0;
    analysis.blackBishops = 0;
    analysis.whiteRooks = 0;
    analysis.blackRooks = 0;
    analysis.whiteQueens = 0;
    analysis.blackQueens = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                analysis.whitePieces++;
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        analysis.whitePawns++;
                        break;
                    case ChessPieceType::KNIGHT:
                        analysis.whiteKnights++;
                        break;
                    case ChessPieceType::BISHOP:
                        analysis.whiteBishops++;
                        break;
                    case ChessPieceType::ROOK:
                        analysis.whiteRooks++;
                        break;
                    case ChessPieceType::QUEEN:
                        analysis.whiteQueens++;
                        break;
                    default:
                        break;
                }
            } else {
                analysis.blackPieces++;
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN:
                        analysis.blackPawns++;
                        break;
                    case ChessPieceType::KNIGHT:
                        analysis.blackKnights++;
                        break;
                    case ChessPieceType::BISHOP:
                        analysis.blackBishops++;
                        break;
                    case ChessPieceType::ROOK:
                        analysis.blackRooks++;
                        break;
                    case ChessPieceType::QUEEN:
                        analysis.blackQueens++;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    // Evaluate positional factors
    analysis.centerControl = evaluateCenterControl(board);
    analysis.mobility = evaluateMobility(board);
    analysis.kingSafety = evaluateKingSafety(board);
    analysis.pawnStructure = evaluatePawnStructure(board);
    analysis.pieceActivity = evaluatePieceActivity(board);

    // Evaluate tactical factors
    analysis.hangingPieces = findHangingPieces(board);
    analysis.pins = findPins(board);
    analysis.forks = findForks(board);
    analysis.discoveredAttacks = findDiscoveredAttacks(board);

    // Determine game phase
    analysis.gamePhase = determineGamePhase(board);

    // Get evaluation breakdown
    analysis.staticEvaluation = evaluatePosition(board);
    analysis.materialScore = analysis.materialBalance;
    analysis.positionalScore = analysis.centerControl + analysis.mobility + analysis.pawnStructure;
    analysis.tacticalScore = -analysis.hangingPieces * 100 - analysis.pins * 50;
    analysis.endgameScore = evaluateEndgame(board);

    // Identify threats, opportunities, and weaknesses
    analysis.threats = identifyThreats(board);
    analysis.opportunities = identifyOpportunities(board);
    analysis.weaknesses = identifyWeaknesses(board);

    // Move quality (simplified - would need search for actual best moves)
    analysis.bestMoveScore = 0;
    analysis.secondBestMoveScore = 0;
    analysis.moveQualityGap = 0;

    return analysis;
}

std::string PositionAnalyzer::formatAnalysis(const PositionAnalysis& analysis) {
    std::ostringstream oss;

    oss << "=== Position Analysis ===\n\n";

    // Material
    oss << "Material:\n";
    oss << "  White: " << analysis.whiteMaterial << " (" << analysis.whitePieces << " pieces)\n";
    oss << "  Black: " << analysis.blackMaterial << " (" << analysis.blackPieces << " pieces)\n";
    oss << "  Balance: " << (analysis.materialBalance > 0 ? "+" : "") << analysis.materialBalance
        << "\n\n";

    // Piece counts
    oss << "Piece Counts:\n";
    oss << "  White - Pawns: " << analysis.whitePawns << ", Knights: " << analysis.whiteKnights
        << ", Bishops: " << analysis.whiteBishops << ", Rooks: " << analysis.whiteRooks
        << ", Queens: " << analysis.whiteQueens << "\n";
    oss << "  Black - Pawns: " << analysis.blackPawns << ", Knights: " << analysis.blackKnights
        << ", Bishops: " << analysis.blackBishops << ", Rooks: " << analysis.blackRooks
        << ", Queens: " << analysis.blackQueens << "\n\n";

    // Positional factors
    oss << "Positional Factors:\n";
    oss << "  Center Control: " << analysis.centerControl << "\n";
    oss << "  Mobility: " << analysis.mobility << "\n";
    oss << "  King Safety: " << analysis.kingSafety << "\n";
    oss << "  Pawn Structure: " << analysis.pawnStructure << "\n";
    oss << "  Piece Activity: " << analysis.pieceActivity << "\n\n";

    // Tactical factors
    oss << "Tactical Factors:\n";
    oss << "  Hanging Pieces: " << analysis.hangingPieces << "\n";
    oss << "  Pins: " << analysis.pins << "\n";
    oss << "  Forks: " << analysis.forks << "\n";
    oss << "  Discovered Attacks: " << analysis.discoveredAttacks << "\n\n";

    // Game phase
    oss << "Game Phase: ";
    switch (analysis.gamePhase) {
        case PositionAnalysis::OPENING:
            oss << "Opening\n";
            break;
        case PositionAnalysis::MIDDLEGAME:
            oss << "Middlegame\n";
            break;
        case PositionAnalysis::ENDGAME:
            oss << "Endgame\n";
            break;
    }
    oss << "\n";

    // Evaluation breakdown
    oss << "Evaluation Breakdown:\n";
    oss << "  Static Evaluation: " << analysis.staticEvaluation << "\n";
    oss << "  Material: " << analysis.materialScore << "\n";
    oss << "  Positional: " << analysis.positionalScore << "\n";
    oss << "  Tactical: " << analysis.tacticalScore << "\n";
    oss << "  Endgame: " << analysis.endgameScore << "\n\n";

    // Threats and opportunities
    if (!analysis.threats.empty()) {
        oss << "Threats:\n";
        for (const auto& threat : analysis.threats) {
            oss << "  - " << threat << "\n";
        }
        oss << "\n";
    }

    if (!analysis.opportunities.empty()) {
        oss << "Opportunities:\n";
        for (const auto& opp : analysis.opportunities) {
            oss << "  - " << opp << "\n";
        }
        oss << "\n";
    }

    if (!analysis.weaknesses.empty()) {
        oss << "Weaknesses:\n";
        for (const auto& weakness : analysis.weaknesses) {
            oss << "  - " << weakness << "\n";
        }
        oss << "\n";
    }

    return oss.str();
}

void PositionAnalyzer::printDetailedAnalysis(const PositionAnalysis& analysis) {
    std::cout << formatAnalysis(analysis);
}

PositionAnalysis::GamePhase PositionAnalyzer::determineGamePhase(const Board& board) {
    int totalMaterial = 0;
    int pieceCount = 0;
    int queenCount = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            pieceCount++;
            switch (piece.PieceType) {
                case ChessPieceType::QUEEN:
                    totalMaterial += 900;
                    queenCount++;
                    break;
                case ChessPieceType::ROOK:
                    totalMaterial += 500;
                    break;
                case ChessPieceType::BISHOP:
                case ChessPieceType::KNIGHT:
                    totalMaterial += 300;
                    break;
                case ChessPieceType::PAWN:
                    totalMaterial += 100;
                    break;
                default:
                    break;
            }
        }
    }

    if (totalMaterial > 6000 && queenCount >= 1 && pieceCount > 20) {
        return PositionAnalysis::OPENING;
    }

    if (totalMaterial < 2000 || pieceCount <= 10) {
        return PositionAnalysis::ENDGAME;
    }

    return PositionAnalysis::MIDDLEGAME;
}

int PositionAnalyzer::countMaterial(const Board& board, ChessPieceColor color) {
    int material = 0;
    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceColor == color && piece.PieceType != ChessPieceType::NONE &&
            piece.PieceType != ChessPieceType::KING) {
            material += piece.PieceValue;
        }
    }
    return material;
}

int PositionAnalyzer::evaluateCenterControl(const Board& board) {
    return ::evaluateCenterControl(board);
}

int PositionAnalyzer::evaluateMobility(const Board& board) {
    return ::evaluateMobility(board);
}

int PositionAnalyzer::evaluateKingSafety(const Board& board) {
    int whiteSafety = ::evaluateKingSafety(board, ChessPieceColor::WHITE);
    int blackSafety = ::evaluateKingSafety(board, ChessPieceColor::BLACK);
    return whiteSafety - blackSafety;
}

int PositionAnalyzer::evaluatePawnStructure(const Board& board) {
    return ::evaluatePawnStructure(board);
}

int PositionAnalyzer::evaluatePieceActivity(const Board& board) {
    int activity = 0;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> whiteMoves = GetAllMoves(board, ChessPieceColor::WHITE);
    std::vector<std::pair<int, int>> blackMoves = GetAllMoves(board, ChessPieceColor::BLACK);

    activity = static_cast<int>(whiteMoves.size() - blackMoves.size());

    return activity * 5;
}

int PositionAnalyzer::findHangingPieces(const Board& board) {
    return ::evaluateHangingPieces(board) / 100;
}

int PositionAnalyzer::findPins(const Board& board) {
    int pins = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int attackerSq = 0; attackerSq < 64; ++attackerSq) {
                const Piece& attacker = board.squares[attackerSq].piece;
                if (attacker.PieceColor == enemyColor &&
                    canPieceAttackSquare(board, attackerSq, sq)) {
                    pins++;
                    break;
                }
            }
        }
    }

    return pins;
}

int PositionAnalyzer::findForks(const Board& board) {
    int forks = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::KNIGHT || piece.PieceType == ChessPieceType::PAWN) {
            int attackCount = 0;
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int targetSq = 0; targetSq < 64; ++targetSq) {
                const Piece& target = board.squares[targetSq].piece;
                if (target.PieceColor == enemyColor && target.PieceType != ChessPieceType::NONE &&
                    target.PieceType != ChessPieceType::KING) {
                    if (canPieceAttackSquare(board, sq, targetSq)) {
                        attackCount++;
                    }
                }
            }

            if (attackCount >= 2) {
                forks++;
            }
        }
    }

    return forks;
}

int PositionAnalyzer::findDiscoveredAttacks(const Board& board) {
    int discovered = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KNIGHT) {
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int targetSq = 0; targetSq < 64; ++targetSq) {
                const Piece& target = board.squares[targetSq].piece;
                if (target.PieceColor == enemyColor && target.PieceType != ChessPieceType::NONE) {
                    Board testBoard = board;
                    if (testBoard.movePiece(sq, targetSq)) {
                        for (int behindSq = 0; behindSq < 64; ++behindSq) {
                            const Piece& behind = testBoard.squares[behindSq].piece;
                            if (behind.PieceColor == piece.PieceColor &&
                                behind.PieceType != ChessPieceType::NONE &&
                                canPieceAttackSquare(testBoard, behindSq, targetSq) &&
                                !canPieceAttackSquare(board, behindSq, targetSq)) {
                                discovered++;
                                goto next_piece;
                            }
                        }
                    }
                }
            }
        next_piece:;
        }
    }

    return discovered;
}

std::vector<std::string> PositionAnalyzer::identifyThreats(const Board& board) {
    std::vector<std::string> threats;

    ChessPieceColor currentColor = board.turn;
    ChessPieceColor enemyColor =
        (currentColor == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> enemyMoves = GetAllMoves(board, enemyColor);

    for (const auto& move : enemyMoves) {
        Board testBoard = board;
        if (testBoard.movePiece(move.first, move.second)) {
            if (IsKingInCheck(testBoard, currentColor)) {
                threats.push_back("King in check threat");
                break;
            }

            const Piece& captured = board.squares[move.second].piece;
            if (captured.PieceType != ChessPieceType::NONE && captured.PieceColor == currentColor) {
                threats.push_back("Piece capture threat on square " + std::to_string(move.second));
            }
        }
    }

    return threats;
}

std::vector<std::string> PositionAnalyzer::identifyOpportunities(const Board& board) {
    std::vector<std::string> opportunities;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    for (const auto& move : moves) {
        const Piece& target = board.squares[move.second].piece;
        if (target.PieceType != ChessPieceType::NONE &&
            target.PieceColor != board.squares[move.first].piece.PieceColor) {
            opportunities.push_back("Capture opportunity: " + std::to_string(move.first) + " to " +
                                    std::to_string(move.second));
        }
    }

    return opportunities;
}

std::vector<std::string> PositionAnalyzer::identifyWeaknesses(const Board& board) {
    std::vector<std::string> weaknesses;

    int hanging = findHangingPieces(board);
    if (hanging > 0) {
        weaknesses.push_back(std::to_string(hanging) + " hanging piece(s)");
    }

    int pins = findPins(board);
    if (pins > 0) {
        weaknesses.push_back(std::to_string(pins) + " pinned piece(s)");
    }

    return weaknesses;
}
