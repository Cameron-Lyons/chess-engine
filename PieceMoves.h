#include "ChessPiece.h"
#include <string>
#include <vector>


struct PieceMoveSet {
    std::vector<int> moves;
    std::vector<PieceMoveSet> moveset;
};


int Position(int rank, int file){
    return rank * 8 + file;
};


struct {
    PieceMoveSet blackPawnMoves;
    int blackPawnTotalMoves[64] = {};
    PieceMoveSet whitePawnMoves;
    int whitePawnTotalMove[64] = {};
    PieceMoveSet knightMoves;
    int knightTotalMove[64] = {};
    PieceMoveSet bishopMoves1;
    int bishopTotalMove1[64] = {};
    PieceMoveSet bishopMoves2;
    int bishopTotalMove2[64] = {};
    PieceMoveSet bishopMoves3;
    int bishopTotalMove3[64] = {};
    PieceMoveSet bishopMoves4;
    int bishopTotalMove4[64] = {};
    PieceMoveSet rookMoves1;
    int rookTotalMove1[64] = {};
    PieceMoveSet rookMoves2;
    int rookTotalMove2[64] = {};
    PieceMoveSet rookMoves3;
    int rookTotalMove3[64] = {};
    PieceMoveSet rookMoves4;
    int rookTotalMove4[64] = {};
    PieceMoveSet queenMoves1;
    int queenTotalMove1[64] = {};
    PieceMoveSet queenMoves2;
    int queenTotalMove2[64] = {};
    PieceMoveSet queenMoves3;
    int queenTotalMove3[64] = {};
    PieceMoveSet queenMoves4;
    int queenTotalMove4[64] = {};
    PieceMoveSet queenMoves5;
    int queenTotalMove5[64] = {};
    PieceMoveSet queenMoves6;
    int queenTotalMove6[64] = {};
    PieceMoveSet queenMoves7;
    int queenTotalMove7[64] = {};
    PieceMoveSet queenMoves8;
    int queenTotalMove8[64] = {};
    PieceMoveSet kingMoves;
} MoveArrays;


static void SetMovesBlackPawn(){
    for (int i = 8; i < 55; i++) {
        PieceMoveSet moveset;
        int x = i & 8;
        int y = i / 8;

        //attacking moves
        if ((y < 7) && (x < 7)) {  
            moveset.moves.push_back(i+8+1);
            MoveArrays.blackPawnTotalMoves[i]++;
        }
        if ((y < 7) && (x > 0)) {
            moveset.moves.push_back(i+8-1);
            MoveArrays.blackPawnTotalMoves[i]++;
        }

        //normal moves
        moveset.moves.push_back(i+8);
        MoveArrays.blackPawnMoves.moves[i]++;

        // start can move 2 forward
        if (y == 6) {
            moveset.moves.push_back(i+16);
            MoveArrays.blackPawnMoves.moves[i]++;
        }
        MoveArrays.blackPawnMoves.moveset[i] = moveset;
    }
}


static void SetMovesWhitePawn(){
    for (int i = 8; i <= 55; i++) {
        PieceMoveSet moveset;
        int x = i & 8;
        int y = i / 8;

        //attacking moves
        if ((y > 0) && (x < 7)) {  
            moveset.moves.push_back(i-8+1);
            MoveArrays.whitePawnMoves.moves[i]++;
        }
        if ((y > 0) && (x > 0)) {
            moveset.moves.push_back(i-8-1);
            MoveArrays.whitePawnMoves.moves[i]++;
        }

        //normal moves
        moveset.moves.push_back(i-8);
        MoveArrays.whitePawnMoves.moves[i]++;

        // start move 2
        if (y == 1) {
            moveset.moves.push_back(i-16);
            MoveArrays.whitePawnMoves.moves[i]++;
        }
        MoveArrays.whitePawnMoves.moveset[i] = moveset;
    }
}


static void SetMovesKnight(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            if (y < 6 && x < 0){
                moveset.moves.push_back(i+17);
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 1 && x < 7){
                moveset.moves.push_back(i+15);
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 1 && x > 0){
                moveset.moves.push_back(i-15); 
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 6 && x < 7){
                moveset.moves.push_back(i+6);
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 0 && x < 6){
                moveset.moves.push_back(i+10);
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y < 7 && x > 1){
                moveset.moves.push_back(i-6);
                MoveArrays.knightMoves.moves[i]++;
            }
            if (y > 0 && x > 1){
                moveset.moves.push_back(i-10);
                MoveArrays.knightMoves.moves[i]++;
            }
            if ( y < 7 && x < 6){
                moveset.moves.push_back(i+17);
                MoveArrays.knightMoves.moves[i]++;
            }
            MoveArrays.knightMoves.moveset[i] = moveset;
        }
    }
}


static void SetMovesBishop(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank < 7 && file < 7){
                rank++;
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.bishopMoves1.moves[i]++;
            }
            MoveArrays.bishopMoves1.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank < 7 && file > 0){
                rank++;
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.bishopMoves2.moves[i]++;
            }
            MoveArrays.bishopMoves2.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank > 0 && file > 7){
                rank--;
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.bishopMoves3.moves[i]++;
            }
            MoveArrays.bishopMoves3.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank > 0 && file > 0){
                rank--;
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.bishopMoves4.moves[i]++;
            }
            MoveArrays.bishopMoves4.moveset[i] = moveset;
        };
    }
}


static void SetMovesRook(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank < 7){
                rank++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.rookMoves1.moves[i]++;
            }
            MoveArrays.rookMoves1.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (rank > 0){
                rank--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.rookMoves2.moves[i]++;
            }
            MoveArrays.rookMoves2.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (file < 7){
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.rookMoves3.moves[i]++;
            }
            MoveArrays.rookMoves3.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while (file > 0){
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.rookMoves4.moves[i]++;
            }
            MoveArrays.rookMoves4.moveset[i] = moveset;
        }
    }
}


static void SetMovesQueen(){
    for(int y=0; y<8; y++){
        for(int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank < 7){
                rank++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves1.moves[i]++;
            }
            MoveArrays.queenMoves1.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank > 0){
                rank--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves2.moves[i]++;
            }
            MoveArrays.queenMoves2.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(file < 7){
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves3.moves[i]++;
            }
            MoveArrays.queenMoves3.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(file > 0){
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves4.moves[i]++;
            }
            MoveArrays.queenMoves4.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank < 7 && file < 7){
                rank++;
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves5.moves[i]++;
            }
            MoveArrays.queenMoves5.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank < 7 && file > 0){
                rank++;
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves6.moves[i]++;
            }
            MoveArrays.queenMoves6.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank > 0 && file > 7){
                rank--;
                file++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves7.moves[i]++;
            }
            MoveArrays.queenMoves7.moveset[i] = moveset;

            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            while(rank > 0 && file > 0){
                rank--;
                file--;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.queenMoves8.moves[i]++;
            }
            MoveArrays.queenMoves8.moveset[i] = moveset;
        }
    }
}


static void SetMovesKing(){
    for(int y=0; y<8; y++){
        for(int x=0; x<8; x++){
            int i = y+(8*x);
            PieceMoveSet moveset;
            int rank = y;
            int file = x;
            if (rank < 7){
                rank++;
                int move = Position(rank, file);
                moveset.moves.push_back(move);
                MoveArrays.kingMoves.moves[i]++;
            }
            if (rank > 0){
                moveset.moves.push_back(Position(rank-1, file));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (file < 7){
                moveset.moves.push_back(Position(rank, file+1));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (file > 0){
                moveset.moves.push_back(Position(rank, file-1));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (rank < 7 && file < 7){
                moveset.moves.push_back(Position(rank+1, file+1));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (rank < 7 && file > 0){
                moveset.moves.push_back(Position(rank+1, file-1));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (rank > 0 && file < 7){
                moveset.moves.push_back(Position(rank-1, file+1));
                MoveArrays.kingMoves.moves[i]++;
            }
            if (rank > 0 && file > 0){
                moveset.moves.push_back(Position(rank-1, file-1));
                MoveArrays.kingMoves.moves[i]++;
            }
            MoveArrays.kingMoves.moveset[i] = moveset;
        }
    }
}
