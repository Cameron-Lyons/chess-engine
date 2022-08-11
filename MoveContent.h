#include "ChessPiece.h"
#include <string>


struct PieceMoving{
    int dest;
    int src;
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;

    PieceMoving(ChessPieceColor pieceColor, ChessPieceType pieceType, bool moved){
        this->dest = dest;
        this->src = src;
        this->moved = moved;
        this->pieceType = pieceType;
        this->pieceColor = pieceColor;
    }
    PieceMoving(ChessPieceType pieceType){
        pieceType = pieceType;
        pieceColor = WHITE;
        src = 0;
        dest = 0;
        moved = false;
    }
};


struct PieceTaken{
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;
    int position;

    PieceTaken(ChessPieceColor pieceColor, ChessPieceType pieceType, bool moved){
        this->moved = moved;
        this->pieceType = pieceType;
        this->pieceColor = pieceColor;
        this->position = position;
    }

    PieceTaken(ChessPieceType pieceType){
        pieceType = pieceType;
        pieceColor = WHITE;
        position = 0;
        moved = false;
    }
};


std::string PortableGameNotation(){
    std::string pgn;
    int srcCol = MovingPiece.src%8;
    int srcRow = MovingPiece.src/8;
    int destCol = MovingPiece.dest%8;
    int destRow = MovingPiece.dest/8;

    if (MovingPieceSecondary.PieceColor == WHITE){
        if (MovingPieceSecondary.src == 7){
            pgn += "O-O";
        }
        else if (MovingPieceSecondary.src == 0){
            pgn += "O-O-O";
        }
    }
    else{
        if (MovingPieceSecondary.src == 56){
            pgn += "O-O";
        }
        else if (MovingPieceSecondary.src == 63){
            pgn += "O-O-O";
        }
    }
    if (TakenPiece.PieceType != NONE){
        pgn += "x";
    }
    pgn += GetColumnFromInt(destCol + 1);
    pgn += destRow;
    if (PawnPromoted){
        pgn += "Q";
    }
    return pgn;
}


