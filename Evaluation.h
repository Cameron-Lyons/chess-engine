#include "ChessBoard.h"


class Evaluation{
    const short PawnTable[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10,-20,-20, 10, 10,  5,
        5, -5,-10,  0,  0,-10, -5,  5,
        0,  0,  0,  0,  0,  0,  0,  0,
        5,  5, 10, 10, 10, 10,  5,  5,
        10, 10, 20, 20, 20, 20, 10, 10,
        50, 50, 50, 50, 50, 50, 50, 50,
        0,  0,  0,  0,  0,  0,  0,  0
    };


    const short KnightTable[64] = {
        -50,-40,-30,-30,-30,-30,-40,-50,
        -40,-20,  0,  0,  0,  0,-20,-40,
        -30,  0, 10, 15, 15, 10,  0,-30,
        -30,  5, 15, 20, 20, 15,  5,-30,
        -30,  0, 15, 20, 20, 15,  0,-30,
        -30,  5, 10, 15, 15, 10,  5,-30,
        -40,-20,  0,  5,  5,  0,-20,-40,
        -50,-40,-30,-30,-30,-30,-40,-50
    };


    const short BishopTable[64] = {
        -20,-10,-10,-10,-10,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5, 10, 10,  5,  0,-10,
        -10,  5,  5, 10, 10,  5,  5,-10,
        -10,  0, 10, 10, 10, 10,  0,-10,
        -10, 10, 10, 10, 10, 10, 10,-10,
        -10,  5,  0,  0,  0,  0,  5,-10,
        -20,-10,-10,-10,-10,-10,-10,-20
    };


    const short RookTable[64] = {
        0,  0,  0,  0,  0,  0,  0,  0,
        5, 10, 10, 10, 10, 10, 10,  5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        -5,  0,  0,  0,  0,  0,  0, -5,
        0,  0,  0,  5,  5,  0,  0,  0
    };


    const short KingTable[64] = {
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -30,-40,-40,-50,-50,-40,-40,-30,
        -20,-30,-30,-40,-40,-30,-30,-20,
        -10,-20,-20,-20,-20,-20,-20,-10,
        20, 20,  0,  0,  0,  0, 20, 20,
        20, 30, 10,  0,  0, 10, 30, 20
    };


    const short KingTableEndGame[64] = {
        -50,-40,-30,-20,-20,-30,-40,-50,
        -30,-20,-10,  0,  0,-10,-20,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 30, 40, 40, 30,-10,-30,
        -30,-10, 20, 30, 30, 20,-10,-30,
        -30,-30,  0,  0,  0,  0,-30,-30,
        -50,-30,-30,-30,-30,-30,-30,-50
    };


    const short QueenTable[64] = {
        -20,-10,-10, -5, -5,-10,-10,-20,
        -10,  0,  0,  0,  0,  0,  0,-10,
        -10,  0,  5,  5,  5,  5,  0,-10,
        -5,  0,  5,  5,  5,  5,  0, -5,
        0,  0,  5,  5,  5,  5,  0, -5,
        -10,  5,  5,  5,  5,  5,  0,-10,
        -10,  0,  5,  0,  0,  0,  0,-10,
        -20,-10,-10, -5, -5,-10,-10,-20
    };

    int EvaluatePieceScore(Square square, int pos, bool castled,
                          bool endGame, int bishopCount,
                          bool insufficientMaterial){
        int score = 0;
        int idx = pos;

        if (square.Piece.PieceColor == WHITE){
            idx = 63 - idx;
        }

        score += square.Piece.PieceValue;
        score += square.Piece.DefendedValue;
        score -= square.Piece.AttackedValue;

        // penalize hung pieces
        if (square.Piece.DefendedValue < square.Piece.AttackedValue){
            score -= ((square.Piece.DefendedValue - square.Piece.AttackedValue) * 10);
        }

        // promote mobility
        if (square.Piece.ValidMoves != 0){
            score += square.Piece.ValidMoves.count;
        }

        if (square.Piece.PieceType == PAWN){
            insufficientMaterial = false;
            // pawns on edges are worth less
            if (position % 8 ==0 || position % 8 == 7){
                score -= 15;
            }
            if (square.Piece.PieceColor == WHITE){
            // doubled pawns are bad
                if whitePawnCount[position % 8] > 0){
                    score -= 16;
                }
                if (position >= 8 && position <= 15){
                    if (square.Piece.AttackedValue == 0){
                        whitePawnCount[position % 8] += 200;
                    if (square.Piece.DefendedValue != 0){
                        whitePawnCount[position % 8] += 50;
                    }
                    }
                    }
                else if (position >= 16 && position <= 23){
                    if (square.Piece.AttackedValue == 0){
                        whitePawnCount[position % 8] += 100;
                    if (square.Piece.DefendedValue != 0){
                        whitePawnCount[position % 8] += 25;
                    }
                    }
                    }
                whitePawnCount[position % 8] += 10;
            }
        else if (square.Piece.PieceColor == BLACK){
            // doubled pawns are bad
            if blackPawnCount[position % 8] > 0){
                score -= 16;
            }
            // pawns are in the 6th row and not attacked are good
            if (position >= 48 && position <= 55){
                if (square.Piece.AttackedValue == 0){
                    blackPawnCount[position % 8] += 200;
                if (square.Piece.DefendedValue != 0){
                    blackPawnCount[position % 8] += 50;
                }
                }
                }
            // pawns are in the 6th row and not attacked are good
            else if (position >= 40 && position <= 47){
                if (square.Piece.AttackedValue == 0){
                    blackPawnCount[position % 8] += 100;
                if (square.Piece.DefendedValue != 0){
                    blackPawnCount[position % 8] += 25;
                }
                }
                blackPawnCount[position % 8] += 10;
                }
    }
    else if (square.Piece.PieceType = KNIGHT){

    }
};
