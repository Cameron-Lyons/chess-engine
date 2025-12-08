#ifndef EVALUATION_H
#define EVALUATION_H

#include "NNUE.h"
#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"

const int KING_SAFETY_PAWN_SHIELD_BONUS = 10;
const int KING_SAFETY_OPEN_FILE_PENALTY = 20;
const int KING_SAFETY_SEMI_OPEN_FILE_PENALTY = 10;
const int KING_SAFETY_ATTACK_WEIGHT[7] = {0, 1, 2, 3, 5, 8, 12};
const int KING_SAFETY_ATTACK_UNITS_MAX = 100;

const int PAWN_TABLE[64] = {0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50, 50, 50,  50, 50,
                            10, 10, 20, 30,  30,  20, 10, 10, 5,  5,  10,  25, 25, 10,  5,  5,
                            0,  0,  0,  20,  20,  0,  0,  0,  5,  -5, -10, 0,  0,  -10, -5, 5,
                            5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,   0,  0,  0,   0,  0};

const int KNIGHT_TABLE[64] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
                              0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
                              15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
                              -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
                              5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

const int BISHOP_TABLE[64] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
                              0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
                              5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
                              -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
                              0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

const int ROOK_TABLE[64] = {0,  0, 0, 0, 0, 0, 0, 0,  5,  10, 10, 10, 10, 10, 10, 5,
                            -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                            -5, 0, 0, 0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0,  0,  -5,
                            -5, 0, 0, 0, 0, 0, 0, -5, 0,  0,  0,  5,  5,  0,  0,  0};

const int QUEEN_TABLE[64] = {-20, -10, -10, -5,  -5,  -10, -10, -20, -10, 0,   0,   0,  0,
                             0,   0,   -10, -10, 0,   5,   5,   5,   5,   0,   -10, -5, 0,
                             5,   5,   5,   5,   0,   -5,  0,   0,   5,   5,   5,   5,  0,
                             -5,  -10, 5,   5,   5,   5,   5,   0,   -10, -10, 0,   5,  0,
                             0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20};

const int KING_TABLE[64] = {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50,
                            -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40,
                            -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30,
                            -20, -10, -20, -20, -20, -20, -20, -20, -10, 20,  20,  0,   0,
                            0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color);
int evaluatePawnStructure(const Board& board);
int evaluateMobility(const Board& board);
int evaluateCenterControl(const Board& board);
int evaluateKingSafety(const Board& board, ChessPieceColor color);
int evaluateHangingPieces(const Board& board);
int evaluateQueenTrapDanger(const Board& board);
int evaluateTacticalSafety(const Board& board);
bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos);
int evaluatePosition(const Board& board);
int evaluateKingSafetyForColor(const Board& board, int kingPos, ChessPieceColor color);
int evaluatePassedPawns(const Board& board);
int evaluateBishopPair(const Board& board);
int evaluateRooksOnOpenFiles(const Board& board);
int evaluateEndgame(const Board& board);

bool isNNUEEnabled();
void setNNUEEnabled(bool enabled);
int evaluatePositionNNUE(const Board& board);

#endif
