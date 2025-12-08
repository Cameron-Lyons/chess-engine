#include "Evaluation.h"
#include "../ai/EndgameTablebase.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "EvaluationEnhanced.h"
#include "EvaluationTuning.h"
#include "NNUE.h"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace EvaluationParams;

static bool useNNUE = false;

namespace PieceSquareTables {

const int PAWN_MG[64] = {0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50, 50, 50,  50, 50,
                         10, 10, 20, 30,  30,  20, 10, 10, 5,  5,  10,  25, 25, 10,  5,  5,
                         0,  0,  0,  20,  20,  0,  0,  0,  5,  -5, -10, 0,  0,  -10, -5, 5,
                         5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,   0,  0,  0,   0,  0};

const int PAWN_EG[64] = {0,  0,  0,  0,  0,  0,  0,  0,  80, 80, 80, 80, 80, 80, 80, 80,
                         50, 50, 50, 50, 50, 50, 50, 50, 30, 30, 30, 30, 30, 30, 30, 30,
                         20, 20, 20, 20, 20, 20, 20, 20, 10, 10, 10, 10, 10, 10, 10, 10,
                         10, 10, 10, 10, 10, 10, 10, 10, 0,  0,  0,  0,  0,  0,  0,  0};

const int KNIGHT_MG[64] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
                           0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
                           15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
                           -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
                           5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

const int KNIGHT_EG[64] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
                           0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
                           15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
                           -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
                           5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

const int BISHOP_MG[64] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
                           0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
                           5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
                           -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
                           0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

const int BISHOP_EG[64] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
                           0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
                           5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
                           -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
                           0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

const int ROOK_MG[64] = {0,  0, 0, 0, 0, 0, 0, 0,  5,  10, 10, 10, 10, 10, 10, 5,
                         -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                         -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                         -5, 0, 0, 0, 0, 0, 0, -5, 0,  0,  0,  5,  5,  0,  0,  0};

const int ROOK_EG[64] = {0,  0, 0, 0, 0, 0, 0, 0,  5,  10, 10, 10, 10, 10, 10, 5,
                         -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                         -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                         -5, 0, 0, 0, 0, 0, 0, -5, 0,  0,  0,  5,  5,  0,  0,  0};

const int QUEEN_MG[64] = {-20, -10, -10, -5,  -5,  -10, -10, -20, -10, 0,   0,   0,  0,
                          0,   0,   -10, -10, 0,   5,   5,   5,   5,   0,   -10, -5, 0,
                          5,   5,   5,   5,   0,   -5,  0,   0,   5,   5,   5,   5,  0,
                          -5,  -10, 5,   5,   5,   5,   5,   0,   -10, -10, 0,   5,  0,
                          0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20};

const int QUEEN_EG[64] = {-20, -10, -10, -5,  -5,  -10, -10, -20, -10, 0,   0,   0,  0,
                          0,   0,   -10, -10, 0,   5,   5,   5,   5,   0,   -10, -5, 0,
                          5,   5,   5,   5,   0,   -5,  0,   0,   5,   5,   5,   5,  0,
                          -5,  -10, 5,   5,   5,   5,   5,   0,   -10, -10, 0,   5,  0,
                          0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20};

const int KING_MG[64] = {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50,
                         -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40,
                         -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30,
                         -20, -10, -20, -20, -20, -20, -20, -20, -10, 20,  20,  0,   0,
                         0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

const int KING_EG[64] = {-50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,
                         -10, -20, -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -10,
                         30,  40,  40,  30,  -10, -30, -30, -10, 30,  40,  40,  30,  -10,
                         -30, -30, -10, 20,  30,  30,  20,  -10, -30, -30, -30, 0,   0,
                         0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

int getPieceSquareValue(ChessPieceType piece, int square, ChessPieceColor color, int gamePhase) {

    if (square < 0 || square >= 64) {
        return 0;
    }

    int adjustedSquare = (color == ChessPieceColor::WHITE) ? square : (63 - square);

    if (adjustedSquare < 0 || adjustedSquare >= 64) {
        return 0;
    }

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
        if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING &&
            piece != ChessPieceType::PAWN) {
            switch (piece) {
                case ChessPieceType::QUEEN:
                    totalMaterial += 900;
                    break;
                case ChessPieceType::ROOK:
                    totalMaterial += 500;
                    break;
                case ChessPieceType::BISHOP:
                case ChessPieceType::KNIGHT:
                    totalMaterial += 300;
                    break;
                default:
                    break;
            }
        }
    }

    int phase = 256 - (totalMaterial * 256) / 3900;
    return std::max(0, std::min(256, phase));
}
} // namespace PieceSquareTables

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {

    if (position < 0 || position >= 64) {
        return 0;
    }

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
        if (whitePawns > 1)
            score -= 20 * (whitePawns - 1);
        if (blackPawns > 1)
            score += 20 * (blackPawns - 1);
    }
    for (int col = 0; col < 8; col++) {
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col)
                        continue;
                    for (int adjRow = 0; adjRow < 8; adjRow++) {
                        int adjPos = adjRow * 8 + adjCol;
                        if (board.squares[adjPos].piece.PieceType == ChessPieceType::PAWN &&
                            board.squares[adjPos].piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated)
                        break;
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
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        int mobility = 0;
        int row = i / 8;
        int col = i % 8;

        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT: {
                int knightMoves[][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                        {1, -2},  {1, 2},  {2, -1},  {2, 1}};
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
                mobilityScore +=
                    piece.PieceColor == ChessPieceColor::WHITE ? mobility * 4 : -mobility * 4;
                break;
            }

            case ChessPieceType::BISHOP: {
                int directions[][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8)
                            break;

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
                mobilityScore +=
                    piece.PieceColor == ChessPieceColor::WHITE ? mobility * 3 : -mobility * 3;
                break;
            }

            case ChessPieceType::ROOK: {
                int directions[][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8)
                            break;

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
                mobilityScore +=
                    piece.PieceColor == ChessPieceColor::WHITE ? mobility * 2 : -mobility * 2;
                break;
            }

            case ChessPieceType::QUEEN: {
                int directions[][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                       {0, 1},   {1, -1}, {1, 0},  {1, 1}};
                for (auto& dir : directions) {
                    for (int dist = 1; dist < 8; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= 8 || newCol < 0 || newCol >= 8)
                            break;

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
                mobilityScore +=
                    piece.PieceColor == ChessPieceColor::WHITE ? mobility * 1 : -mobility * 1;
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
    int score = 0;
    int kingRow = kingPos / 8;
    int kingCol = kingPos % 8;

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
    score += pawnShield * KING_SAFETY_PAWN_SHIELD_BONUS;

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
        score -= KING_SAFETY_OPEN_FILE_PENALTY;
    }

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
        score -= KING_SAFETY_SEMI_OPEN_FILE_PENALTY;
    }

    return score;
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
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1);
                         checkCol++) {
                        for (int checkRow = row + 1; checkRow < 8; checkRow++) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor ==
                                    ChessPieceColor::BLACK) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed)
                            break;
                    }
                    if (isPassed) {
                        int passedBonus = (row - 1) * 20 + 10;
                        score += passedBonus;
                    }
                } else {
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1);
                         checkCol++) {
                        for (int checkRow = row - 1; checkRow >= 0; checkRow--) {
                            int checkPos = checkRow * 8 + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor ==
                                    ChessPieceColor::WHITE) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed)
                            break;
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
    if (whiteBishops >= 2)
        score += 50;
    if (blackBishops >= 2)
        score -= 50;

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

int evaluatePosition(const Board& board) {

    if (useNNUE && NNUE::globalEvaluator) {
        return NNUE::evaluate(board);
    }

    int mgScore = 0;
    int egScore = 0;

    int gamePhase = 0;

    for (int square = 0; square < 64; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        int materialValue = piece.PieceValue;
        int adjustedSquare = (piece.PieceColor == ChessPieceColor::WHITE) ? square : 63 - square;

        int oldMgPST = getPieceSquareValue(piece.PieceType, adjustedSquare, piece.PieceColor);
        int oldEgPST = getPieceSquareValue(piece.PieceType, adjustedSquare, piece.PieceColor);

        int newMgPST = getTunedPST(piece.PieceType, adjustedSquare, false);
        int newEgPST = getTunedPST(piece.PieceType, adjustedSquare, true);
        int newPositionalValue = interpolatePhase(newMgPST, newEgPST, gamePhase);

        int oldPositionalValue = interpolatePhase(oldMgPST, oldEgPST, gamePhase);
        (void)((newPositionalValue * 7 + oldPositionalValue * 3) / 10);

        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + newMgPST;
            egScore += materialValue + newEgPST;
        } else {
            mgScore -= materialValue + newMgPST;
            egScore -= materialValue + newEgPST;
        }

        switch (piece.PieceType) {
            case ChessPieceType::PAWN:
                gamePhase += 0;
                break;
            case ChessPieceType::KNIGHT:
                gamePhase += 1;
                break;
            case ChessPieceType::BISHOP:
                gamePhase += 1;
                break;
            case ChessPieceType::ROOK:
                gamePhase += 2;
                break;
            case ChessPieceType::QUEEN:
                gamePhase += 4;
                break;
            case ChessPieceType::KING:
                gamePhase += 0;
                break;
            default:
                break;
        }
    }

    if (ENABLE_PAWN_STRUCTURE) {
        int pawnScore = evaluatePawnStructure(board);
        mgScore += pawnScore;
        egScore += pawnScore * 1.2;
        logEvaluationComponents("Pawn Structure", pawnScore);
    }

    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateMobility(board);
        mgScore += mobilityScore;
        egScore += mobilityScore * 0.8;
        logEvaluationComponents("Piece Mobility", mobilityScore);
    }

    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += kingSafetyScore * 0.3;
        logEvaluationComponents("King Safety", kingSafetyScore);
    }

    int bishopPairScore = evaluateBishopPair(board);
    mgScore += bishopPairScore;
    egScore += bishopPairScore * 1.5;

    int rookFileScore = evaluateRooksOnOpenFiles(board);
    mgScore += rookFileScore;
    egScore += rookFileScore;

    int passedPawnScore = evaluatePassedPawns(board);
    mgScore += passedPawnScore * 0.8;
    egScore += passedPawnScore * 1.5;

    int tacticalSafetyScore = evaluateTacticalSafety(board);
    mgScore += tacticalSafetyScore;

    int endgameScore = 0;
    if (gamePhase < 12) {
        endgameScore = evaluateEndgame(board);
        egScore += endgameScore;

        int endgameKnowledgeScore = EndgameKnowledge::evaluateEndgame(board);
        egScore += endgameKnowledgeScore;
    }
    egScore += tacticalSafetyScore * 0.7;

    int hangingPiecesScore = evaluateHangingPieces(board);
    mgScore += hangingPiecesScore;
    egScore += hangingPiecesScore;

    int queenTrapScore = evaluateQueenTrapDanger(board);
    mgScore += queenTrapScore;
    egScore += queenTrapScore * 0.5;

    if (board.turn == ChessPieceColor::WHITE) {
        mgScore += TEMPO_BONUS;
        egScore += TEMPO_BONUS / 2;
    } else {
        mgScore -= TEMPO_BONUS;
        egScore -= TEMPO_BONUS / 2;
    }

    int finalScore = interpolatePhase(mgScore, egScore, gamePhase);

#ifdef USE_ENHANCED_EVALUATION

#endif

    logEvaluationComponents("Final Enhanced Score", finalScore);
    return finalScore;
}

int evaluateHangingPieces(const Board& board) {
    int score = 0;

    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING)
            continue;

        bool isUnderAttack = false;
        bool isDefended = false;

        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                         ? ChessPieceColor::BLACK
                                         : ChessPieceColor::WHITE;

        for (int j = 0; j < 64; j++) {
            const Piece& enemyPiece = board.squares[j].piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor)
                continue;

            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }

        if (isUnderAttack) {
            for (int j = 0; j < 64; j++) {
                const Piece& friendlyPiece = board.squares[j].piece;
                if (friendlyPiece.PieceType == ChessPieceType::NONE ||
                    friendlyPiece.PieceColor != piece.PieceColor || j == i)
                    continue;

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
                        friendlyPiece.PieceColor != piece.PieceColor || j == i ||
                        friendlyPiece.PieceType == ChessPieceType::PAWN)
                        continue;

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

        if (piece.PieceType == ChessPieceType::QUEEN || piece.PieceType == ChessPieceType::ROOK ||
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

bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos) {

    if (piecePos < 0 || piecePos >= 64 || targetPos < 0 || targetPos >= 64) {
        return false;
    }

    const Piece& piece = board.squares[piecePos].piece;
    if (piece.PieceType == ChessPieceType::NONE)
        return false;

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
            if (abs(rowDiff) != abs(colDiff))
                return false;
            int rowStep = (rowDiff > 0) ? 1 : -1;
            int colStep = (colDiff > 0) ? 1 : -1;
            for (int i = 1; i < abs(rowDiff); i++) {
                int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                if (checkPos < 0 || checkPos >= 64)
                    return false;
                if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                    return false;
            }
            return true;
        }
        case ChessPieceType::ROOK: {
            if (rowDiff != 0 && colDiff != 0)
                return false;
            if (rowDiff == 0) {
                int colStep = (colDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(colDiff); i++) {
                    int checkPos = fromRow * 8 + (fromCol + i * colStep);
                    if (checkPos < 0 || checkPos >= 64)
                        return false;
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                        return false;
                }
            } else {
                int rowStep = (rowDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(rowDiff); i++) {
                    int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                    if (checkPos < 0 || checkPos >= 64)
                        return false;
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                        return false;
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
                        if (checkPos < 0 || checkPos >= 64)
                            return false;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                            return false;
                    }
                } else if (colDiff == 0) {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + fromCol;
                        if (checkPos < 0 || checkPos >= 64)
                            return false;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                            return false;
                    }
                } else {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * 8 + (fromCol + i * colStep);
                        if (checkPos < 0 || checkPos >= 64)
                            return false;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                            return false;
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
        if (piece.PieceType != ChessPieceType::QUEEN)
            continue;

        int row = i / 8;
        int col = i % 8;

        int escapeSquares = 0;

        int directions[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                {0, 1},   {1, -1}, {1, 0},  {1, 1}};

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
                ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                                 ? ChessPieceColor::BLACK
                                                 : ChessPieceColor::WHITE;

                for (int j = 0; j < 64; j++) {
                    const Piece& enemyPiece = board.squares[j].piece;
                    if (enemyPiece.PieceType == ChessPieceType::NONE ||
                        enemyPiece.PieceColor != enemyColor)
                        continue;

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

                if (abs(newRow - row) > 2 || abs(newCol - col) > 2)
                    break;
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
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / 8;
            int col = i % 8;

            int attackers = 0;
            int defenders = 0;

            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < 64; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i)
                    continue;

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
                    int distance = (abs(row - kingRow) > abs(col - kingCol)) ? abs(row - kingRow)
                                                                             : abs(col - kingCol);
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
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < 64; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i)
                    continue;

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

bool isNNUEEnabled() {
    return useNNUE;
}

void setNNUEEnabled(bool enabled) {
    useNNUE = enabled;
}

int evaluatePositionNNUE(const Board& board) {
    if (!NNUE::globalEvaluator) {

        return evaluatePosition(board);
    }

    bool oldUseNNUE = useNNUE;
    useNNUE = true;
    int eval = evaluatePosition(board);
    useNNUE = oldUseNNUE;
    return eval;
}
