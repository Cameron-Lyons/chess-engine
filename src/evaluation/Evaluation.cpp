#include "Evaluation.h"
#include "../ai/EndgameTablebase.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "EvaluationTuning.h"
#include "HybridEvaluator.h"
#include "NNUE.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

extern uint64_t ZobristTable[64][12];

using namespace EvaluationParams;

static bool useNNUE = false;
thread_local bool useFastEval = false;

namespace {
constexpr int kBoardSquareCount = 64;
constexpr int kPieceTypeCount = 6;
constexpr int kMirrorSquareOffset = kBoardSquareCount - 1;
constexpr int kGamePhaseScale = 256;
constexpr int kMaxMinorMaterial = 3900;
constexpr int kMaterialWeightQueen = 900;
constexpr int kMaterialWeightRook = 500;
constexpr int kMaterialWeightMinor = 300;

constexpr int kCenterControlBonus = 30;
constexpr int kDoublePawnPenalty = 20;
constexpr int kIsolatedPawnPenalty = 30;
constexpr int kPassedPawnRankScale = 20;
constexpr int kPassedPawnBaseBonus = 10;
constexpr int kBishopPairBonus = 50;
constexpr int kOpenFileRookBonus = 20;
constexpr int kSemiOpenFileRookBonus = 10;
constexpr int kEndgameMaterialThreshold = 2000;
constexpr int kKingCenterDistanceTarget = 7;
constexpr int kKingCentralizationScale = 5;

constexpr int kOpeningEndgameBoundaryPhase = 12;
constexpr float kPawnMgPassedScale = 0.8F;
constexpr float kPawnEgScale = 1.2F;
constexpr float kPassedPawnEgScale = 1.5F;
constexpr float kMobilityEgScale = 0.8F;
constexpr float kKingSafetyEgScale = 0.3F;
constexpr float kBishopPairEgScale = 1.5F;
constexpr float kTacticalSafetyEgScale = 0.7F;
constexpr float kQueenTrapEgScale = 0.5F;
constexpr int kQueenGamePhaseIncrement = 4;

constexpr float kHangingPenaltyRatio = 0.8F;
constexpr int kHangingQueenPenalty = 500;
constexpr int kQueenUnsupportedPenalty = 300;
constexpr int kQueenCrowdedPenalty = 200;
constexpr int kQueenCornerTrapPenalty = 400;
constexpr int kQueenEdgeTrapPenalty = 150;
constexpr int kQueenEnemyTerritoryWhiteRow = 5;
constexpr int kQueenEnemyTerritoryBlackRow = 2;
constexpr int kNearbyEnemyRadius = 2;
constexpr int kNearbyEnemyThreshold = 2;
constexpr int kDangerLevelScale = 300;
constexpr int kHighValuePieceThreshold = 300;
constexpr int kPenaltyDivisor = 4;
constexpr int kQueenTacticalPenalty = 800;
constexpr int kNearEnemyKingDistance = 2;
constexpr int kNearEnemyKingLowDefenderThreshold = 2;
constexpr int kNearEnemyKingPenalty = 250;

constexpr int kQueenTrapNoEscapePenalty = 800;
constexpr int kQueenTrapLowEscapePenalty = 400;
constexpr int kQueenTrapMediumEscapePenalty = 200;
constexpr int kQueenTrapLowEscapeThreshold = 2;
constexpr int kQueenTrapMediumEscapeThreshold = 4;
constexpr int kQueenTrapCornerEscapeThreshold = 3;
constexpr int kQueenTrapEdgeEscapeThreshold = 5;
constexpr int kQueenTrapCornerPenalty = 600;
constexpr int kQueenTrapEdgePenalty = 300;
constexpr int kQueenEscapeSearchRadius = 2;
} // namespace

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

    int adjustedSquare =
        (color == ChessPieceColor::WHITE) ? square : (kMirrorSquareOffset - square);

    if (adjustedSquare < 0 || adjustedSquare >= NUM_SQUARES) {
        return 0;
    }

    static const int* const mgTables[] = {PAWN_MG, KNIGHT_MG, BISHOP_MG,
                                          ROOK_MG, QUEEN_MG,  KING_MG};
    static const int* const egTables[] = {PAWN_EG, KNIGHT_EG, BISHOP_EG,
                                          ROOK_EG, QUEEN_EG,  KING_EG};
    int idx = static_cast<int>(piece);
    if (idx < 0 || idx >= kPieceTypeCount) {
        return 0;
    }
    int mgValue = mgTables[idx][adjustedSquare];
    int egValue = egTables[idx][adjustedSquare];
    return ((mgValue * (kGamePhaseScale - gamePhase)) + (egValue * gamePhase)) / kGamePhaseScale;
}

int calculateGamePhase(const Board& board) {
    int totalMaterial = 0;
    for (int i = 0; i < NUM_SQUARES; i++) {
        ChessPieceType piece = board.squares[i].piece.PieceType;
        if (piece != ChessPieceType::NONE && piece != ChessPieceType::KING &&
            piece != ChessPieceType::PAWN) {
            switch (piece) {
                case ChessPieceType::QUEEN:
                    totalMaterial += kMaterialWeightQueen;
                    break;
                case ChessPieceType::ROOK:
                    totalMaterial += kMaterialWeightRook;
                    break;
                case ChessPieceType::BISHOP:
                case ChessPieceType::KNIGHT:
                    totalMaterial += kMaterialWeightMinor;
                    break;
                default:
                    break;
            }
        }
    }

    int phase = kGamePhaseScale - ((totalMaterial * kGamePhaseScale) / kMaxMinorMaterial);
    return std::max(0, std::min(kGamePhaseScale, phase));
}
} // namespace PieceSquareTables

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color) {

    if (position < 0 || position >= NUM_SQUARES) {
        return 0;
    }

    static const int* const tables[] = {PieceSquareTables::PAWN_MG,   PieceSquareTables::KNIGHT_MG,
                                        PieceSquareTables::BISHOP_MG, PieceSquareTables::ROOK_MG,
                                        PieceSquareTables::QUEEN_MG,  PieceSquareTables::KING_MG};
    int idx = static_cast<int>(pieceType);
    int value = (idx >= 0 && idx < kPieceTypeCount) ? tables[idx][position] : 0;
    if (color == ChessPieceColor::BLACK) {
        value = -value;
    }
    return value;
}

int evaluatePawnStructure(const Board& board) {
    int score = 0;
    for (int col = 0; col < BOARD_SIZE; col++) {
        int whitePawns = 0;
        int blackPawns = 0;
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = (row * BOARD_SIZE) + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                    whitePawns++;
                } else {
                    blackPawns++;
                }
            }
        }
        if (whitePawns > 1) {
            score -= kDoublePawnPenalty * (whitePawns - 1);
        }
        if (blackPawns > 1) {
            score += kDoublePawnPenalty * (blackPawns - 1);
        }
    }
    for (int col = 0; col < BOARD_SIZE; col++) {
        for (int row = 0; row < BOARD_SIZE; row++) {
            int pos = (row * BOARD_SIZE) + col;
            if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                bool isolated = true;
                for (int adjCol = std::max(0, col - 1); adjCol <= std::min(BOARD_SIZE - 1, col + 1);
                     adjCol++) {
                    if (adjCol == col) {
                        continue;
                    }
                    for (int adjRow = 0; adjRow < BOARD_SIZE; adjRow++) {
                        int adjPos = (adjRow * BOARD_SIZE) + adjCol;
                        if (board.squares[adjPos].piece.PieceType == ChessPieceType::PAWN &&
                            board.squares[adjPos].piece.PieceColor == ChessPieceColor::WHITE) {
                            isolated = false;
                            break;
                        }
                    }
                    if (!isolated) {
                        break;
                    }
                }
                if (isolated) {
                    if (board.squares[pos].piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= kIsolatedPawnPenalty;
                    } else {
                        score += kIsolatedPawnPenalty;
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
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
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
                        int pos = (newRow * BOARD_SIZE) + newCol;
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
                        int newRow = row + (dir[0] * dist);
                        int newCol = col + (dir[1] * dist);
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 ||
                            newCol >= BOARD_SIZE) {
                            break;
                        }
                        int pos = (newRow * BOARD_SIZE) + newCol;
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
                        int newRow = row + (dir[0] * dist);
                        int newCol = col + (dir[1] * dist);
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 ||
                            newCol >= BOARD_SIZE) {
                            break;
                        }
                        int pos = (newRow * BOARD_SIZE) + newCol;
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
                        int newRow = row + (dir[0] * dist);
                        int newCol = col + (dir[1] * dist);
                        if (newRow < 0 || newRow >= BOARD_SIZE || newCol < 0 ||
                            newCol >= BOARD_SIZE) {
                            break;
                        }
                        int pos = (newRow * BOARD_SIZE) + newCol;
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
                score += kCenterControlBonus;
            } else {
                score -= kCenterControlBonus;
            }
        }
    }
    return score;
}

int evaluateKingSafety(const Board& board) {
    int safetyScore = 0;
    int whiteKingPos = -1;
    int blackKingPos = -1;
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
                int checkSquare = (checkRow * BOARD_SIZE) + checkCol;
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
        int square = (rank * BOARD_SIZE) + kingCol;
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
        int square = (rank * BOARD_SIZE) + kingCol;
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
            int pos = (row * BOARD_SIZE) + col;
            const Piece& piece = board.squares[pos].piece;

            if (piece.PieceType == ChessPieceType::PAWN) {
                bool isPassed = true;

                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    for (int checkCol = std::max(0, col - 1); checkCol <= std::min(7, col + 1);
                         checkCol++) {
                        for (int checkRow = row + 1; checkRow < BOARD_SIZE; checkRow++) {
                            int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor ==
                                    ChessPieceColor::BLACK) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) {
                            break;
                        }
                    }
                    if (isPassed) {
                        int passedBonus = ((row - 1) * kPassedPawnRankScale) + kPassedPawnBaseBonus;
                        score += passedBonus;
                    }
                } else {
                    for (int checkCol = std::max(0, col - 1);
                         checkCol <= std::min(BOARD_SIZE - 1, col + 1); checkCol++) {
                        for (int checkRow = row - 1; checkRow >= 0; checkRow--) {
                            int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                            if (board.squares[checkPos].piece.PieceType == ChessPieceType::PAWN &&
                                board.squares[checkPos].piece.PieceColor ==
                                    ChessPieceColor::WHITE) {
                                isPassed = false;
                                break;
                            }
                        }
                        if (!isPassed) {
                            break;
                        }
                    }
                    if (isPassed) {
                        int passedBonus = (((BOARD_SIZE - 2) - row) * kPassedPawnRankScale) +
                                          kPassedPawnBaseBonus;
                        score -= passedBonus;
                    }
                }
            }
        }
    }

    return score;
}

int evaluateBishopPair(const Board& board) {
    int whiteBishops = 0;
    int blackBishops = 0;

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
    if (whiteBishops >= 2) {
        score += kBishopPairBonus;
    }
    if (blackBishops >= 2) {
        score -= kBishopPairBonus;
    }
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
                int pos = (row * BOARD_SIZE) + col;
                if (board.squares[pos].piece.PieceType == ChessPieceType::PAWN) {
                    isOpenFile = false;
                    if (board.squares[pos].piece.PieceColor == board.squares[i].piece.PieceColor) {
                        isSemiOpen = false;
                    }
                }
            }

            if (isOpenFile) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += kOpenFileRookBonus;
                } else {
                    score -= kOpenFileRookBonus;
                }
            } else if (isSemiOpen) {
                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score += kSemiOpenFileRookBonus;
                } else {
                    score -= kSemiOpenFileRookBonus;
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
    if (totalMaterial < kEndgameMaterialThreshold) {
        for (int i = 0; i < NUM_SQUARES; i++) {
            if (board.squares[i].piece.PieceType == ChessPieceType::KING) {
                int file = i % BOARD_SIZE;
                int rank = i / BOARD_SIZE;
                const int centerDistance =
                    static_cast<int>(std::max(std::abs(file - 3.5), std::abs(rank - 3.5)));

                if (board.squares[i].piece.PieceColor == ChessPieceColor::WHITE) {
                    score +=
                        (kKingCenterDistanceTarget - centerDistance) * kKingCentralizationScale;
                } else {
                    score -=
                        (kKingCenterDistanceTarget - centerDistance) * kKingCentralizationScale;
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
            int idx = (p.PieceColor == ChessPieceColor::WHITE) ? 0 : kPieceTypeCount;
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
                gamePhase += kQueenGamePhaseIncrement;
                break;
            default:
                break;
        }
    }
    gamePhase = std::min(gamePhase, TOTAL_PHASE);

    for (int square = 0; square < NUM_SQUARES; ++square) {
        const Piece& piece = board.squares[square].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
        int materialValue = piece.PieceValue;
        int adjustedSquare =
            (piece.PieceColor == ChessPieceColor::WHITE) ? square : kMirrorSquareOffset - square;

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
        int pawnMg = 0;
        int pawnEg = 0;
        if (pawnHashTable.probe(pawnKey, pawnMg, pawnEg)) {
            mgScore += pawnMg;
            egScore += pawnEg;
        } else {
            int pawnScore = evaluatePawnStructure(board);
            int passedPawnScore = evaluatePassedPawns(board);
            pawnMg = pawnScore +
                     static_cast<int>(static_cast<float>(passedPawnScore) * kPawnMgPassedScale);
            pawnEg = static_cast<int>(static_cast<float>(pawnScore) * kPawnEgScale) +
                     static_cast<int>(static_cast<float>(passedPawnScore) * kPassedPawnEgScale);
            pawnHashTable.store(pawnKey, pawnMg, pawnEg);
            mgScore += pawnMg;
            egScore += pawnEg;
        }
    }

    if (ENABLE_PIECE_MOBILITY) {
        int mobilityScore = evaluateMobility(board);
        mgScore += mobilityScore;
        egScore += static_cast<int>(static_cast<float>(mobilityScore) * kMobilityEgScale);
    }

    if (ENABLE_KING_SAFETY) {
        int kingSafetyScore = evaluateKingSafety(board);
        mgScore += kingSafetyScore;
        egScore += static_cast<int>(static_cast<float>(kingSafetyScore) * kKingSafetyEgScale);
    }

    const bool fastEval = useFastEval;

    int bishopPairScore = evaluateBishopPair(board);
    mgScore += bishopPairScore;
    egScore += static_cast<int>(static_cast<float>(bishopPairScore) * kBishopPairEgScale);
    int rookFileScore = evaluateRooksOnOpenFiles(board);
    mgScore += rookFileScore;
    egScore += rookFileScore;
    int tacticalSafetyScore = 0;
    if (!fastEval) {
        tacticalSafetyScore = evaluateTacticalSafety(board);
        mgScore += tacticalSafetyScore;
    }
    int endgameScore = 0;
    if (gamePhase < kOpeningEndgameBoundaryPhase) {
        endgameScore = evaluateEndgame(board);
        egScore += endgameScore;
        int endgameKnowledgeScore = EndgameKnowledge::evaluateEndgame(board);
        egScore += endgameKnowledgeScore;
    }
    egScore += static_cast<int>(static_cast<float>(tacticalSafetyScore) * kTacticalSafetyEgScale);
    if (!fastEval) {
        int hangingPiecesScore = evaluateHangingPieces(board);
        mgScore += hangingPiecesScore;
        egScore += hangingPiecesScore;
        int queenTrapScore = evaluateQueenTrapDanger(board);
        mgScore += queenTrapScore;
        egScore += static_cast<int>(static_cast<float>(queenTrapScore) * kQueenTrapEgScale);
    }

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

    if (!fastEval) {
        logEvaluationComponents("Final Enhanced Score", finalScore);
    }
    return finalScore;
}

int evaluateHangingPieces(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
        if (piece.PieceType == ChessPieceType::PAWN || piece.PieceType == ChessPieceType::KING) {
            continue;
        }
        bool isUnderAttack = false;
        bool isDefended = false;

        ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                         ? ChessPieceColor::BLACK
                                         : ChessPieceColor::WHITE;

        for (int j = 0; j < NUM_SQUARES; j++) {
            const Piece& enemyPiece = board.squares[j].piece;
            if (enemyPiece.PieceType == ChessPieceType::NONE ||
                enemyPiece.PieceColor != enemyColor) {
                continue;
            }
            if (canPieceAttackSquare(board, j, i)) {
                isUnderAttack = true;
                break;
            }
        }

        if (isUnderAttack) {
            for (int j = 0; j < NUM_SQUARES; j++) {
                const Piece& friendlyPiece = board.squares[j].piece;
                if (friendlyPiece.PieceType == ChessPieceType::NONE ||
                    friendlyPiece.PieceColor != piece.PieceColor || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    isDefended = true;
                    break;
                }
            }

            if (!isDefended) {
                const int hangingPenalty =
                    static_cast<int>(static_cast<float>(piece.PieceValue) * kHangingPenaltyRatio);

                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= hangingPenalty;
                } else {
                    score += hangingPenalty;
                }

                if (piece.PieceType == ChessPieceType::QUEEN) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= kHangingQueenPenalty;
                    } else {
                        score += kHangingQueenPenalty;
                    }
                }
            }
        }

        if (piece.PieceType == ChessPieceType::QUEEN) {
            int row = i / BOARD_SIZE;
            int col = i % BOARD_SIZE;

            bool inEnemyTerritory =
                (piece.PieceColor == ChessPieceColor::WHITE &&
                 row >= kQueenEnemyTerritoryWhiteRow) ||
                (piece.PieceColor == ChessPieceColor::BLACK && row <= kQueenEnemyTerritoryBlackRow);

            if (inEnemyTerritory) {
                int supportCount = 0;
                for (int j = 0; j < NUM_SQUARES; j++) {
                    const Piece& friendlyPiece = board.squares[j].piece;
                    if (friendlyPiece.PieceType == ChessPieceType::NONE ||
                        friendlyPiece.PieceColor != piece.PieceColor || j == i ||
                        friendlyPiece.PieceType == ChessPieceType::PAWN) {
                        continue;
                    }
                    if (canPieceAttackSquare(board, j, i)) {
                        supportCount++;
                    }
                }

                if (supportCount == 0) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= kQueenUnsupportedPenalty;
                    } else {
                        score += kQueenUnsupportedPenalty;
                    }
                }

                int nearbyEnemies = 0;
                for (int dr = -kNearbyEnemyRadius; dr <= kNearbyEnemyRadius; dr++) {
                    for (int dc = -kNearbyEnemyRadius; dc <= kNearbyEnemyRadius; dc++) {
                        int checkRow = row + dr;
                        int checkCol = col + dc;
                        if (checkRow >= 0 && checkRow < BOARD_SIZE && checkCol >= 0 &&
                            checkCol < BOARD_SIZE) {
                            int checkPos = (checkRow * BOARD_SIZE) + checkCol;
                            if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE &&
                                board.squares[checkPos].piece.PieceColor == enemyColor) {
                                nearbyEnemies++;
                            }
                        }
                    }
                }

                if (nearbyEnemies >= kNearbyEnemyThreshold) {
                    if (piece.PieceColor == ChessPieceColor::WHITE) {
                        score -= kQueenCrowdedPenalty;
                    } else {
                        score += kQueenCrowdedPenalty;
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
                    score -= kQueenCornerTrapPenalty;
                } else {
                    score += kQueenCornerTrapPenalty;
                }
            } else if (isOnEdge && piece.PieceType == ChessPieceType::QUEEN) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= kQueenEdgeTrapPenalty;
                } else {
                    score += kQueenEdgeTrapPenalty;
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
    if (piece.PieceType == ChessPieceType::NONE) {
        return false;
    }
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
            if (abs(rowDiff) != abs(colDiff)) {
                return false;
            }
            int rowStep = (rowDiff > 0) ? 1 : -1;
            int colStep = (colDiff > 0) ? 1 : -1;
            for (int i = 1; i < abs(rowDiff); i++) {
                int checkPos = ((fromRow + i * rowStep) * BOARD_SIZE) + (fromCol + i * colStep);
                if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                    return false;
                }
                if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                    return false;
                }
            }
            return true;
        }
        case ChessPieceType::ROOK: {
            if (rowDiff != 0 && colDiff != 0) {
                return false;
            }
            if (rowDiff == 0) {
                int colStep = (colDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(colDiff); i++) {
                    int checkPos = (fromRow * BOARD_SIZE) + (fromCol + i * colStep);
                    if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                        return false;
                    }
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                        return false;
                    }
                }
            } else {
                int rowStep = (rowDiff > 0) ? 1 : -1;
                for (int i = 1; i < abs(rowDiff); i++) {
                    int checkPos = ((fromRow + i * rowStep) * BOARD_SIZE) + fromCol;
                    if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                        return false;
                    }
                    if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                        return false;
                    }
                }
            }
            return true;
        }
        case ChessPieceType::QUEEN: {
            if (rowDiff == 0 || colDiff == 0 || abs(rowDiff) == abs(colDiff)) {
                if (rowDiff == 0) {
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(colDiff); i++) {
                        int checkPos = (fromRow * BOARD_SIZE) + (fromCol + i * colStep);
                        if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                            return false;
                        }
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                            return false;
                        }
                    }
                } else if (colDiff == 0) {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos = ((fromRow + i * rowStep) * BOARD_SIZE) + fromCol;
                        if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                            return false;
                        }
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                            return false;
                        }
                    }
                } else {
                    int rowStep = (rowDiff > 0) ? 1 : -1;
                    int colStep = (colDiff > 0) ? 1 : -1;
                    for (int i = 1; i < abs(rowDiff); i++) {
                        int checkPos =
                            ((fromRow + i * rowStep) * BOARD_SIZE) + (fromCol + i * colStep);
                        if (checkPos < 0 || checkPos >= NUM_SQUARES) {
                            return false;
                        }
                        if (board.squares[checkPos].piece.PieceType != ChessPieceType::NONE) {
                            return false;
                        }
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
        if (piece.PieceType != ChessPieceType::QUEEN) {
            continue;
        }
        int row = i / BOARD_SIZE;
        int col = i % BOARD_SIZE;
        int escapeSquares = 0;

        int directions[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                {0, 1},   {1, -1}, {1, 0},  {1, 1}};

        for (auto& direction : directions) {
            int newRow = row + direction[0];
            int newCol = col + direction[1];

            while (newRow >= 0 && newRow < BOARD_SIZE && newCol >= 0 && newCol < BOARD_SIZE) {
                int newPos = (newRow * BOARD_SIZE) + newCol;

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
                        enemyPiece.PieceColor != enemyColor) {
                        continue;
                    }
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

                newRow += direction[0];
                newCol += direction[1];

                if (abs(newRow - row) > kQueenEscapeSearchRadius ||
                    abs(newCol - col) > kQueenEscapeSearchRadius) {
                    break;
                }
            }
        }

        if (escapeSquares == 0) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= kQueenTrapNoEscapePenalty;
            } else {
                score += kQueenTrapNoEscapePenalty;
            }
        } else if (escapeSquares <= kQueenTrapLowEscapeThreshold) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= kQueenTrapLowEscapePenalty;
            } else {
                score += kQueenTrapLowEscapePenalty;
            }
        } else if (escapeSquares <= kQueenTrapMediumEscapeThreshold) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= kQueenTrapMediumEscapePenalty;
            } else {
                score += kQueenTrapMediumEscapePenalty;
            }
        }

        bool isOnEdge = (row == 0 || row == 7 || col == 0 || col == 7);
        bool isInCorner = ((row == 0 || row == 7) && (col == 0 || col == 7));

        if (isInCorner && escapeSquares <= kQueenTrapCornerEscapeThreshold) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= kQueenTrapCornerPenalty;
            } else {
                score += kQueenTrapCornerPenalty;
            }
        } else if (isOnEdge && escapeSquares <= kQueenTrapEdgeEscapeThreshold) {
            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score -= kQueenTrapEdgePenalty;
            } else {
                score += kQueenTrapEdgePenalty;
            }
        }
    }

    return score;
}

int evaluateTacticalSafety(const Board& board) {
    int score = 0;

    for (int i = 0; i < NUM_SQUARES; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE) {
            continue;
        }
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
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }

            if (attackers > defenders) {
                int dangerLevel = (attackers - defenders) * kDangerLevelScale;
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
                            weakestAttacker = std::min<int>(attacker.PieceValue, weakestAttacker);
                        }
                    }

                    if (weakestAttacker < piece.PieceValue) {
                        if (piece.PieceColor == ChessPieceColor::WHITE) {
                            score -= kQueenTacticalPenalty;
                        } else {
                            score += kQueenTacticalPenalty;
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
                    if (distance <= kNearEnemyKingDistance) {
                        nearEnemyKing = true;
                        break;
                    }
                }
            }

            if (nearEnemyKing && defenders < kNearEnemyKingLowDefenderThreshold) {
                if (piece.PieceColor == ChessPieceColor::WHITE) {
                    score -= kNearEnemyKingPenalty;
                } else {
                    score += kNearEnemyKingPenalty;
                }
            }
        }

        else if (piece.PieceValue >= kHighValuePieceThreshold) {
            int attackers = 0;
            int defenders = 0;
            ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                             ? ChessPieceColor::BLACK
                                             : ChessPieceColor::WHITE;

            for (int j = 0; j < NUM_SQUARES; j++) {
                const Piece& otherPiece = board.squares[j].piece;
                if (otherPiece.PieceType == ChessPieceType::NONE || j == i) {
                    continue;
                }
                if (canPieceAttackSquare(board, j, i)) {
                    if (otherPiece.PieceColor == enemyColor) {
                        attackers++;
                    } else {
                        defenders++;
                    }
                }
            }

            if (attackers > defenders) {
                int penalty = (attackers - defenders) * piece.PieceValue / kPenaltyDivisor;
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

void setFastEvaluationMode(bool enabled) {
    useFastEval = enabled;
}

bool isFastEvaluationMode() {
    return useFastEval;
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
