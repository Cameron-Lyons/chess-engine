#include "ChessPiece.h"
#include <string>

struct PieceMoveSet {
    const std::string moves[64];
};

struct MoveArrays {
    PieceMoveSet blackPawnMoves;
    PieceMoveSet whitePawnMoves;
    PieceMoveSet knightMoves;
    PieceMoveSet bishopMoves1;
    PieceMoveSet bishopMoves2;
    PieceMoveSet bishopMoves3;
    PieceMoveSet bishopMoves4;
    PieceMoveSet rookMoves1;
    PieceMoveSet rookMoves2;
    PieceMoveSet rookMoves3;
    PieceMoveSet rookMoves4;
    PieceMoveSet queenMoves1;
    PieceMoveSet queenMoves2;
    PieceMoveSet queenMoves3;
    PieceMoveSet queenMoves4;
    PieceMoveSet queenMoves5;
    PieceMoveSet queenMoves6;
    PieceMoveSet queenMoves7;
    PieceMoveSet queenMoves8;
    PieceMoveSet kingMoves;
};
