#ifndef PIECE_MOVES_H
#define PIECE_MOVES_H

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


static void SetMovesBlackPawn(){
    for (int i = 8; i < 56; i++) {
        PieceMoveSet moveset;
        int x = i % 8;
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
        MoveArrays.blackPawnTotalMoves[i]++;

        // start can move 2 forward
        if (y == 6) {
            moveset.moves.push_back(i+16);
            MoveArrays.blackPawnTotalMoves[i]++;
        }
        MoveArrays.blackPawnMoves.moveset.push_back(moveset);
    }
}


static void SetMovesWhitePawn(){
    for (int i = 8; i < 56; i++) {
        PieceMoveSet moveset;
        int x = i % 8;
        int y = i / 8;

        //attacking moves
        if ((y > 0) && (x < 7)) {  
            moveset.moves.push_back(i-8+1);
            MoveArrays.whitePawnTotalMoves[i]++;
        }
        if ((y > 0) && (x > 0)) {
            moveset.moves.push_back(i-8-1);
            MoveArrays.whitePawnTotalMoves[i]++;
        }

        //normal moves
        moveset.moves.push_back(i-8);
        MoveArrays.whitePawnTotalMoves[i]++;

        // start move 2
        if (y == 1) {
            moveset.moves.push_back(i-16);
            MoveArrays.whitePawnTotalMoves[i]++;
        }
        MoveArrays.whitePawnMoves.moveset.push_back(moveset);
    }
}


static void SetMovesKnight(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y*8 + x;
            PieceMoveSet moveset;
            
            // All 8 possible knight moves
            int knightMoves[8][2] = {{-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}};
            
            for (int k = 0; k < 8; k++) {
                int newY = y + knightMoves[k][0];
                int newX = x + knightMoves[k][1];
                
                if (newY >= 0 && newY < 8 && newX >= 0 && newX < 8) {
                    int move = Position(newY, newX);
                    moveset.moves.push_back(move);
                    MoveArrays.knightTotalMoves[i]++;
                }
            }
            MoveArrays.knightMoves.moveset.push_back(moveset);
        }
    }
}


static void SetMovesBishop(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y*8 + x;
            
            // Direction 1: diagonal up-right
            {
                PieceMoveSet moveset1;
                int rank = y;
                int file = x;
                while (rank < 7 && file < 7){
                    rank++;
                    file++;
                    int move = Position(rank, file);
                    moveset1.moves.push_back(move);
                    MoveArrays.bishopTotalMoves1[i]++;
                }
                MoveArrays.bishopMoves1.moveset.push_back(moveset1);
            }

            // Direction 2: diagonal up-left
            {
                PieceMoveSet moveset2;
                int rank = y;
                int file = x;
                while (rank < 7 && file > 0){
                    rank++;
                    file--;
                    int move = Position(rank, file);
                    moveset2.moves.push_back(move);
                    MoveArrays.bishopTotalMoves2[i]++;
                }
                MoveArrays.bishopMoves2.moveset.push_back(moveset2);
            }

            // Direction 3: diagonal down-right
            {
                PieceMoveSet moveset3;
                int rank = y;
                int file = x;
                while (rank > 0 && file < 7){
                    rank--;
                    file++;
                    int move = Position(rank, file);
                    moveset3.moves.push_back(move);
                    MoveArrays.bishopTotalMoves3[i]++;
                }
                MoveArrays.bishopMoves3.moveset.push_back(moveset3);
            }

            // Direction 4: diagonal down-left
            {
                PieceMoveSet moveset4;
                int rank = y;
                int file = x;
                while (rank > 0 && file > 0){
                    rank--;
                    file--;
                    int move = Position(rank, file);
                    moveset4.moves.push_back(move);
                    MoveArrays.bishopTotalMoves4[i]++;
                }
                MoveArrays.bishopMoves4.moveset.push_back(moveset4);
            }
        }
    }
}


static void SetMovesRook(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y*8 + x;
            
            // Direction 1: up
            {
                PieceMoveSet moveset1;
                int rank = y;
                int file = x;
                while (rank < 7){
                    rank++;
                    int move = Position(rank, file);
                    moveset1.moves.push_back(move);
                    MoveArrays.rookTotalMoves1[i]++;
                }
                MoveArrays.rookMoves1.moveset.push_back(moveset1);
            }

            // Direction 2: down
            {
                PieceMoveSet moveset2;
                int rank = y;
                int file = x;
                while (rank > 0){
                    rank--;
                    int move = Position(rank, file);
                    moveset2.moves.push_back(move);
                    MoveArrays.rookTotalMoves2[i]++;
                }
                MoveArrays.rookMoves2.moveset.push_back(moveset2);
            }

            // Direction 3: right
            {
                PieceMoveSet moveset3;
                int rank = y;
                int file = x;
                while (file < 7){
                    file++;
                    int move = Position(rank, file);
                    moveset3.moves.push_back(move);
                    MoveArrays.rookTotalMoves3[i]++;
                }
                MoveArrays.rookMoves3.moveset.push_back(moveset3);
            }

            // Direction 4: left
            {
                PieceMoveSet moveset4;
                int rank = y;
                int file = x;
                while (file > 0){
                    file--;
                    int move = Position(rank, file);
                    moveset4.moves.push_back(move);
                    MoveArrays.rookTotalMoves4[i]++;
                }
                MoveArrays.rookMoves4.moveset.push_back(moveset4);
            }
        }
    }
}


static void SetMovesQueen(){
    for(int y=0; y<8; y++){
        for(int x=0; x<8; x++){
            int i = y*8 + x;
            
            // Rook-like moves (4 directions)
            // Direction 1: up
            {
                PieceMoveSet moveset1;
                int rank = y;
                int file = x;
                while(rank < 7){
                    rank++;
                    int move = Position(rank, file);
                    moveset1.moves.push_back(move);
                    MoveArrays.queenTotalMoves1[i]++;
                }
                MoveArrays.queenMoves1.moveset.push_back(moveset1);
            }

            // Direction 2: down
            {
                PieceMoveSet moveset2;
                int rank = y;
                int file = x;
                while(rank > 0){
                    rank--;
                    int move = Position(rank, file);
                    moveset2.moves.push_back(move);
                    MoveArrays.queenTotalMoves2[i]++;
                }
                MoveArrays.queenMoves2.moveset.push_back(moveset2);
            }

            // Direction 3: right
            {
                PieceMoveSet moveset3;
                int rank = y;
                int file = x;
                while(file < 7){
                    file++;
                    int move = Position(rank, file);
                    moveset3.moves.push_back(move);
                    MoveArrays.queenTotalMoves3[i]++;
                }
                MoveArrays.queenMoves3.moveset.push_back(moveset3);
            }

            // Direction 4: left
            {
                PieceMoveSet moveset4;
                int rank = y;
                int file = x;
                while(file > 0){
                    file--;
                    int move = Position(rank, file);
                    moveset4.moves.push_back(move);
                    MoveArrays.queenTotalMoves4[i]++;
                }
                MoveArrays.queenMoves4.moveset.push_back(moveset4);
            }

            // Bishop-like moves (4 directions)
            // Direction 5: diagonal up-right
            {
                PieceMoveSet moveset5;
                int rank = y;
                int file = x;
                while(rank < 7 && file < 7){
                    rank++;
                    file++;
                    int move = Position(rank, file);
                    moveset5.moves.push_back(move);
                    MoveArrays.queenTotalMoves5[i]++;
                }
                MoveArrays.queenMoves5.moveset.push_back(moveset5);
            }

            // Direction 6: diagonal up-left
            {
                PieceMoveSet moveset6;
                int rank = y;
                int file = x;
                while(rank < 7 && file > 0){
                    rank++;
                    file--;
                    int move = Position(rank, file);
                    moveset6.moves.push_back(move);
                    MoveArrays.queenTotalMoves6[i]++;
                }
                MoveArrays.queenMoves6.moveset.push_back(moveset6);
            }

            // Direction 7: diagonal down-right
            {
                PieceMoveSet moveset7;
                int rank = y;
                int file = x;
                while(rank > 0 && file < 7){
                    rank--;
                    file++;
                    int move = Position(rank, file);
                    moveset7.moves.push_back(move);
                    MoveArrays.queenTotalMoves7[i]++;
                }
                MoveArrays.queenMoves7.moveset.push_back(moveset7);
            }

            // Direction 8: diagonal down-left
            {
                PieceMoveSet moveset8;
                int rank = y;
                int file = x;
                while(rank > 0 && file > 0){
                    rank--;
                    file--;
                    int move = Position(rank, file);
                    moveset8.moves.push_back(move);
                    MoveArrays.queenTotalMoves8[i]++;
                }
                MoveArrays.queenMoves8.moveset.push_back(moveset8);
            }
        }
    }
}


static void SetMovesKing(){
    for (int y=0; y<8; y++){
        for (int x=0; x<8; x++){
            int i = y*8 + x;
            PieceMoveSet moveset;
            
            // All 8 possible king moves
            int kingMoves[8][2] = {{-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}};
            
            for (int k = 0; k < 8; k++) {
                int newY = y + kingMoves[k][0];
                int newX = x + kingMoves[k][1];
                
                if (newY >= 0 && newY < 8 && newX >= 0 && newX < 8) {
                    int move = Position(newY, newX);
                    moveset.moves.push_back(move);
                }
            }
            MoveArrays.kingMoves.moveset.push_back(moveset);
        }
    }
}

// Initialize all move tables
static void InitializeMoveTables() {
    SetMovesBlackPawn();
    SetMovesWhitePawn();
    SetMovesKnight();
    SetMovesBishop();
    SetMovesRook();
    SetMovesQueen();
    SetMovesKing();
}

#endif // PIECE_MOVES_H
