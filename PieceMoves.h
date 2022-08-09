#include "ChessPiece.h"
#include <string>

struct PieceMoveSet {
    int moves[64];
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

static void SetMovesBlackPawn(){
    for (int i = 8; i < 55; i++) {
        PieceMoveSet moveset;
        int x = i & 8;
        int y = i / 8;

        //attacking moves
        if ((y < 7) && (x < 7)) {  
            moveset.moves[i] = i+8+1;
            MoveArrays.blackPawnMoves.moves[i]++;
        }
        if ((y < 7) && (x > 0)) {
            moveset.moves[i] = i+8-1;
            MoveArrays.blackPawnMoves.moves[i]++;
        }

        //normal moves
        moveset.moves[i] = i+8;
        MoveArrays.blackPawnMoves.moves[i]++;

        // start move 2
        if (y == 6) {
            moveset.moves[i] = i+16;
            MoveArrays.blackPawnMoves.moves[i]++;
        }
        MoveArrays.blackPawnMoves.moves[i] = moveset;
    }
}

static void SetMovesWhitePawn(){
    for (int i = 8; i <= 55; i++) {
        PieceMoveSet moveset;
        int x = i & 8;
        int y = i / 8;

        //attacking moves
        if ((y > 0) && (x < 7)) {  
            moveset.moves[i] = i-8+1;
            MoveArrays.whitePawnMoves.moves[i]++;
        }
        if ((y > 0) && (x > 0)) {
            moveset.moves[i] = i-8-1;
            MoveArrays.whitePawnMoves.moves[i]++;
        }

        //normal moves
        moveset.moves[i] = i-8;
        MoveArrays.whitePawnMoves.moves[i]++;

        // start move 2
        if (y == 1) {
            moveset.moves[i] = i-16;
            MoveArrays.whitePawnMoves.moves[i]++;
        }
        MoveArrays.whitePawnMoves[i] = moveset;
    }
}

static void SetMovesKnight(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            if (y < 6 && x < 0){
                moveset.moves[i] = i+17;
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 1 && x < 7){
                moveset.moves[i] = i+15;
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 1 && x > 0){
                moveset.moves[i] = i-15; 
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 6 && x < 7){
                moveset.moves[i] = i+6;
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 0 && x < 6){
                moveset.moves[i] = i+10;
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 7 && x > 1){
                moveset.moves[i] = i-6;
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 0 && x > 1){
                moveset.moves[i] = i-10;
                MoveArrays.knightMoves.moves[i]++;
            }
            if ( y < 7 && x < 6){
                moveset.moves[i] = i+17;
                MoveArrays.knightMoves.moves[i]++;
            }
            MoveArrays.knightMoves[i] = moveset;
        }
    }
}
