#include "Evaluation.h"
#include "../ai/EndgameTablebase.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "EvaluationEnhanced.h"
#include "EvaluationTuning.h"
#include "NNUE.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

extern uint64_t ZobristTable[64][12];

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

    if (square < 0 || square >= NUM_SQUARES) {
        return 0;
    }

    int adjustedSquare = (color == ChessPieceColor::WHITE) ? square : (63 - square);

    if (adjustedSquare < 0 || adjustedSquare >= NUM_SQUARES) {
        return 0;
    }

    static const int* const mgTables[] = {PAWN_MG,   KNIGHT_MG, BISHOP_MG,
                                          ROOK_MG,   QUEEN_MG,  KING_MG};
    static const int* const egTables[] = {PAWN_EG,   KNIGHT_EG, BISHOP_EG,
                                          ROOK_EG,   QUEEN_EG,  KING_EG};
    int idx = static_cast<int>(piece);
    if (idx < 0 || idx >= 6) return 0;
    int mgValue = mgTables[idx][adjustedSquare];
    int egValue = egTables[idx][adjustedSquare];

    return ((mgValue * (256 - gamePhase)) + (egValue * gamePhase)) / 256;
}

int calculateGamePhase(const Board& board) {
    int totalMaterial = 0;
    for (int i = 0; i < NUM_SQUARES; i++) {
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

    if (position < 0 || position >= NUM_SQUARES) {
        return 0;
    }

    static const int* const tables[] = {
        PieceSquareTables::PAWN_MG,   PieceSquareTables::KNIGHT_MG, PieceSquareTables::BISHOP_MG,
        PieceSquareTables::ROOK_MG,   PieceSquareTables::QUEEN_MG,  PieceSquareTables::KING_MG};
    int idx = static_cast<int>(pieceType);
    int value = (idx >= 0 && idx < 6) ? tables[idx][position] : 0;
    if (color == ChessPieceColor::BLACK) {
        value = -value;
    }
    return value;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;
    for (int col = 0; col < BOARD_SIZE; col++) {
        int whitePawns = 0, blackPawns = 0;
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = row * BOARD_SIZE + col;
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
    for (int col = 0; col < BOARD_SIZE; col++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = row * BOARD_SIZE + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(7, col + 1); adjCol++) {
                    if (adjCol == col)
                        continue;
                    for (int adjRow = 0; adjRow < BOARD_SIZE; adjRow++) {
                        int adjPos = adjRow * BOARD_SIZE + adjCol;
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

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        int mobility = 0;
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;

        switch (piece.PieceType) {
            case ChessPieceType::KNIGHT: {
                int knightMoves[][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                        {1, -2},  {1, 2},  {2, -1},  {2, 1}};
                for (auto& move : knightMoves) {
                    int newRow = row + move[0];
                    int newCol = col + move[1];
                    if (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
                        int pos = newRow * BOARD_SIZE + newCol;
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
                    for (int dist = 1; dist < BOARD_SIZE; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 || newCol >= BOARD_SIZE)
                            break;

                        int pos = newRow * BOARD_SIZE + newCol;
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
                    for (int dist = 1; dist < BOARD_SIZE; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 || newCol >= BOARD_SIZE)
                            break;

                        int pos = newRow * BOARD_SIZE + newCol;
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
                    for (int dist = 1; dist < BOARD_SIZE; dist++) {
                        int newRow = row + dir[0] * dist;
                        int newCol = col + dir[1] * dist;
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 || newCol >= BOARD_SIZE)
                            break;

                        int pos = newRow * BOARD_SIZE + newCol;
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
    for (int i = 0; i < NUM_SQUARES; i++) {
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
    for (int i = 0; i < NUM_SQUARES; i++) {
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
    int kingRow = kingPos / BOARD_SIZE;
    int kingCol = kingPos % BOARD_SIZE;

    int pawnShield = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            int checkRow = kingRow + dr;
            int checkCol = kingCol + dc;
            if (checkRow >= 0 && checkRow < BOARD_SIZE && checkCol >= 0 && checkCol < BOARD_SIZE) {
                int checkSquare = checkRow * BOARD_SIZE + checkCol;
                const Piece& piece = board.squares[checkSquare].piece;
                if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == color) {
                    pawnShield++;
                }
            }
        }
    }
    score += pawnShield * KING_SAFETY_PAWN_SHIELD_BONUS;

    bool isOnOpenFile = true;
    for (int rank = 0; rank < BOARD_SIZE; rank++) {
        int square = rank * BOARD_SIZE + kingCol;
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
    for (int rank = 0; rank < BOARD_SIZE; rank++) {
        int square = rank * BOARD_SIZE + kingCol;
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

    for (int col = 0; col < BOARD_SIZE; col++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = row * BOARD_SIZE + col;
            const Piece& piece = board.squares[pos].piece;

            if (piece.PieceType == ChessPieceType::PAWN) {
                bool isPassed = true;

                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1);
                         checkCol++) {
                        for (int checkRow = row + 1; checkRow < BOARD_SIZE; checkRow++) {
                            int checkPos = checkRow * BOARD_SIZE + checkCol;
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
                            int checkPos = checkRow * BOARD_SIZE + checkCol;
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

    for (int i = 0; i < NUM_SQUARES; i++) {
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

    for (int i = 0; i < NUM_SQUARES; i++) {
        if (board.squares[i].piece.PieceType == ChessPieceType::ROOK) {
            int col = i % BOARD_SIZE;
            bool isOpenFile = true;
            bool isSemiOpen = true;

            for (int row = 0; row < BOARD_SIZE; row++) {
                int pos = row * BOARD_SIZE + col;
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
    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            totalMaterial += piece.PieceValue;
        }
    }

    int score = 0;
    if (totalMaterial < 2000) {
        for (int i = 0; i < NUM_SQUARES; i++) {
            if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
                int file = i % BOARD_SIZE;
                int rank = i / BOARD_SIZE;
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

static PawnHashTable pawnHashTable;

uint64_t computePawnHash(const Board& board) {
    uint64_t h = 0;
    for (int sq = 0; sq < NUM_SQUARES; ++sq) {
        const Piece& p = board.squares[sq].piece;
        if (p.PieceType == ChessPieceType::PAWN) {
            int idx = (p.PieceColor == ChessPieceColor::WHITE) ? 0 : 6;
            h ^= ZobristTable[sq][idx];
        }
    }
    return h;
}

int evaluatePosition(const Board& board, int contempt) {

    if (useNNUE && NNUE::globalEvaluator) {
        return NNUE::evaluate(board);
    }

    int mgScore = 0;
    int egScore = 0;

    int gamePhase = 0;
    for (int square = 0; square < NUM_SQUARES; ++square) {
        switch (board.squares[square].piece.PieceType) {
            case ChessPieceType::KNIGHT:
            case ChessPieceType::BISHOP:
                gamePhase += 1;
                break;
            case ChessPieceType::ROOK:
                gamePhase += 2;
                break;
            case ChessPieceType::QUEEN:
                gamePhase += 4;
                break;
            default:
                break;
        }
    }
    if (gamePhase > TOTAL_PHASE)
        gamePhase = TOTAL_PHASE;

    for (int square = 0; square < NUM_SQUARES; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        int materialValue = piece.PieceValue;
        int adjustedSquare = (piece.PieceColor == ChessPieceColor::WHITE) ? square : 63 - square;

        int newMgPST = getTunedPST(piece.PieceType, adjustedSquare, false);
        int newEgPST = getTunedPST(piece.PieceType, adjustedSquare, true);

        if (piece.PieceColor == ChessPieceColor::WHITE) {
            mgScore += materialValue + newMgPST;
            egScore += materialValue + newEgPST;
        } else {
            mgScore -= materialValue + newMgPST;
            egScore -= materialValue + newEgPST;
        }
    }

    if (ENABLE_PAWN_STRUCTURE) {
        uint64_t pawnKey = computePawnHash(board);
        int pawnMg, pawnEg;
        if (pawnHashTable.probe(pawnKey, pawnMg, pawnEg)) {
            mgScore += pawnMg;
            egScore += pawnEg;
        } else {
            int pawnScore = evaluatePawnStructure(board);
            int passedPawnScore = evaluatePassedPawns(board);
            pawnMg = pawnScore + static_cast<int>(passedPawnScore * 0.8);
            pawnEg = static_cast<int>(pawnScore * 1.2) + static_cast<int>(passedPawnScore * 1.5);
            pawnHashTable.store(pawnKey, pawnMg, pawnEg);
            mgScore += pawnMg;
            egScore += pawnEg;
        }
    }

    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateMobility(board);
        mgScore += mobilityScore;
        egScore += static_cast<int>(mobilityScore * 0.8);
    }

    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += static_cast<int>(kingSafetyScore * 0.3);
    }

    int bishopPairScore = evaluateBishopPair(board);
    mgScore += bishopPairScore;
    egScore += static_cast<int>(bishopPairScore * 1.5);

    int rookFileScore = evaluateRooksOnOpenFiles(board);
    mgScore += rookFileScore;
    egScore += rookFileScore;

    int tacticalSafetyScore = evaluateTacticalSafety(board);
    mgScore += tacticalSafetyScore;

    int endgameScore = 0;
    if (gamePhase < 12) {
        endgameScore = evaluateEndgame(board);
        egScore += endgameScore;

        int endgameKnowledgeScore = EndgameKnowledge::evaluateEndgame(board);
        egScore += endgameKnowledgeScore;
    }
    egScore += static_cast<int>(tacticalSafetyScore * 0.7);

    int hangingPiecesScore = evaluateHangingPieces(board);
    mgScore += hangingPiecesScore;
    egScore += hangingPiecesScore;

    int queenTrapScore = evaluateQueenTrapDanger(board);
    mgScore += queenTrapScore;
    egScore += static_cast<int>(queenTrapScore * 0.5);

    if (board.turn == ChessPieceColor::WHITE) {
        mgScore += TEMPO_BONUS;
        egScore += TEMPO_BONUS / 2;
    } else {
        mgScore -= TEMPO_BONUS;
        egScore -= TEMPO_BONUS / 2;
    }

    int finalScore = interpolatePhase(mgScore, egScore, gamePhase);

    if (contempt != 0) {
        finalScore += (board.turn == ChessPieceColor::WHITE) ? contempt : -contempt;
    }

    logEvaluationComponents("Final Enhanced Score", finalScore);
    return finalScore;
}

int evaluateHangingPieces(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
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

        for (int j = 0; j < NUM_SQUARES; j++) {
            const Piece& enemyPiece = board.squares[j].piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE || enemyPiece.PieceColor != enemyColor)
                continue;

            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }

        if (isUnderAttack) {
            for (int j = 0; j < NUM_SQUARES; j++) {
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
            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;

            bool inEnemyTerritory = false;
            if (piece.PieceColor == ChessPieceColor::WHITE && row >= 5) {
                inEnemyTerritory = true;
            } else if (piece.PieceColor == ChessPieceColor::BLACK && row <= 2) {
                inEnemyTerritory = true;
            }

            if (inEnemyTerritory) {
                int supportCount = 0;
                for (int j = 0; j < NUM_SQUARES; j++) {
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
                        if (checkRow >= 0 && checkRow < BOARD_SIZE && checkCol >= 0 && checkCol < BOARD_SIZE) {
                            int checkPos = checkRow * BOARD_SIZE + checkCol;
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

            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;

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

    if (piecePos < 0 || piecePos >= NUM_SQUARES || targetPos < 0 || targetPos >= NUM_SQUARES) {
        return false;
    }

    const Piece& piece = board.squares[piecePos].piece;
    if (piece.PieceType == ChessPieceType::NONE)
        return false;

    int fromRow = piecePos / BOARD_SIZE;
    int fromCol = piecePos % BOARD_SIZE;
    int toRow = targetPos / BOARD_SIZE;
    int toCol = targetPos % BOARD_SIZE;

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
                int checkPos = (fromRow + i * rowStep) * BOARD_SIZE + (fromCol + i * colStep);
                if (checkPos < 0 || checkPos >= NUM_SQUARES)
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
                    int checkPos = fromRow * BOARD_SIZE + (fromCol + i * colStep);
                    if (checkPos < 0 || checkPos >= NUM_SQUARES)
                        return false;
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                        return false;
                }
            } else {
                int rowStep = (rowDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(rowDiff); i++) {
                    int checkPos = (fromRow + i * rowStep) * BOARD_SIZE + fromCol;
                    if (checkPos < 0 || checkPos >= NUM_SQUARES)
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
                        int checkPos = fromRow * BOARD_SIZE + (fromCol + i * colStep);
                        if (checkPos < 0 || checkPos >= NUM_SQUARES)
                            return false;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                            return false;
                    }
                } else if (colDiff == 0) {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * BOARD_SIZE + fromCol;
                        if (checkPos < 0 || checkPos >= NUM_SQUARES)
                            return false;
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE)
                            return false;
                    }
                } else {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = (fromRow + i * rowStep) * BOARD_SIZE + (fromCol + i * colStep);
                        if (checkPos < 0 || checkPos >= NUM_SQUARES)
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

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::QUEEN)
            continue;

        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;

        int escapeSquares = 0;

        int directions[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                {0, 1},   {1, -1}, {1, 0},  {1, 1}};

        for (int dir = 0; dir < 8; dir++) {
            int newRow = row + directions[dir][0];
            int newCol = col + directions[dir][1];

            while (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
                int newPos = newRow * BOARD_SIZE + newCol;

                if (board.squares[newPos].piece.PieceType != ChessPieceType::NONE &&
                    board.squares[newPos].piece.PieceColor == piece.PieceColor) {
                    break;
                }

                bool squareIsSafe = true;
                ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                                 ? ChessPieceColor::BLACK
                                                 : ChessPieceColor::WHITE;

                for (int j = 0; j < NUM_SQUARES; j++) {
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

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE)
            continue;

        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;

            int attackers = 0;
            int defenders = 0;

            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < NUM_SQUARES; j++) {
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
                    for (int j = 0; j < NUM_SQUARES; j++) {
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
            for (int j = 0; j < NUM_SQUARES; j++) {
                if (board.squares[j].piece.PieceType == ChessPieceType::KING &&
                    board.squares[j].piece.PieceColor == enemyColor) {
                    int kingRow = j / BOARD_SIZE;
                    int kingCol = j % BOARD_SIZE;
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

            for (int j = 0; j < NUM_SQUARES; j++) {
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
