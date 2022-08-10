#include "ChessPiece.h"


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

