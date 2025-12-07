#ifndef PIECE_MOVES_H
#define PIECE_MOVES_H

#include "../core/ChessPiece.h"
#include <string>
#include <vector>

struct PieceMoveSet {
    std::vector<int> moves;
    std::vector<PieceMoveSet> moveset;
};

int Position(int rank, int file) {
    return rank * 8 + file;
};

struct {
    PieceMoveSet blackPawnMoves;
    int blackPawnTotalMoves[64] = {};
    PieceMoveSet whitePawnMoves;
    int whitePawnTotalMoves[64] = {};
    PieceMoveSet knightMoves;
    int knightTotalMoves[64] = {};
    PieceMoveSet bishopMoves1;
    int bishopTotalMoves1[64] = {};
    PieceMoveSet bishopMoves2;
    int bishopTotalMoves2[64] = {};
    PieceMoveSet bishopMoves3;
    int bishopTotalMoves3[64] = {};
    PieceMoveSet bishopMoves4;
    int bishopTotalMoves4[64] = {};
    PieceMoveSet rookMoves1;
    int rookTotalMoves1[64] = {};
    PieceMoveSet rookMoves2;
    int rookTotalMoves2[64] = {};
    PieceMoveSet rookMoves3;
    int rookTotalMoves3[64] = {};
    PieceMoveSet rookMoves4;
    int rookTotalMoves4[64] = {};
    PieceMoveSet queenMoves1;
    int queenTotalMoves1[64] = {};
    PieceMoveSet queenMoves2;
    int queenTotalMoves2[64] = {};
    PieceMoveSet queenMoves3;
    int queenTotalMoves3[64] = {};
    PieceMoveSet queenMoves4;
    int queenTotalMoves4[64] = {};
    PieceMoveSet queenMoves5;
    int queenTotalMoves5[64] = {};
    PieceMoveSet queenMoves6;
    int queenTotalMoves6[64] = {};
    PieceMoveSet queenMoves7;
    int queenTotalMoves7[64] = {};
    PieceMoveSet queenMoves8;
    int queenTotalMoves8[64] = {};
    PieceMoveSet kingMoves;
} MoveArrays;

#endif
