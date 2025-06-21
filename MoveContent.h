#ifndef MOVE_CONTENT_H
#define MOVE_CONTENT_H

#include "ChessPiece.h"
#include <string>

struct PieceMoving{
    int dest;
    int src;
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;

    PieceMoving(ChessPieceColor pieceColor, ChessPieceType pieceType, bool moved){
        this->dest = 0;
        this->src = 0;
        this->moved = moved;
        this->pieceType = pieceType;
        this->pieceColor = pieceColor;
    }
    
    PieceMoving(ChessPieceType pieceType){
        this->pieceType = pieceType;
        this->pieceColor = WHITE;
        this->src = 0;
        this->dest = 0;
        this->moved = false;
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
        this->position = 0;
    }

    PieceTaken(ChessPieceType pieceType){
        this->pieceType = pieceType;
        this->pieceColor = WHITE;
        this->position = 0;
        this->moved = false;
    }
};

// Global variables for move tracking
extern PieceMoving MovingPiece;
extern PieceMoving MovingPieceSecondary;
extern bool PawnPromoted;

std::string GetColumnFromInt(int column)
{
    switch (column)
    {
        case 0:
            return "a";
        case 1:
            return "b";
        case 2:
            return "c";
        case 3:
            return "d";
        case 4:
            return "e";
        case 5:
            return "f";
        case 6:
            return "g";
        case 7:
            return "h";
        default:
            return "Unknown";
    }
}

std::string GetPgnMove(ChessPieceType pieceType)
{
    switch (pieceType)
    {
        case BISHOP:
            return "B";
        case KING:
            return "K";
        case KNIGHT:
            return "N";
        case QUEEN:
            return "Q";
        case ROOK:
            return "R";
        default:
            return "";
    }
}

std::string PortableGameNotation(){
    std::string pgn;
    int srcCol = MovingPiece.src % 8;
    int srcRow = MovingPiece.src / 8;
    int destCol = MovingPiece.dest % 8;
    int destRow = MovingPiece.dest / 8;

    if (MovingPieceSecondary.pieceColor == WHITE){
        if (MovingPieceSecondary.src == 6){
            pgn += "O-O";
        }
        else if (MovingPieceSecondary.src == 2){
            pgn += "O-O-O";
        }
    }
    else{
        if (MovingPieceSecondary.src == 62){
            pgn += "O-O";
        }
        else if (MovingPieceSecondary.src == 58){
            pgn += "O-O-O";
        }
    }
    
    if (MovingPiece.pieceType != KING) {  // Not castling
        pgn += GetPgnMove(MovingPiece.pieceType);
        switch (MovingPiece.pieceType)
        {
        case KNIGHT:
            pgn += GetColumnFromInt(srcCol);
            pgn += std::to_string(srcRow + 1);
            break;
        case ROOK:
            pgn += GetColumnFromInt(srcCol);
            pgn += std::to_string(srcRow + 1);
            break;
        case PAWN:
            if (srcCol != destCol){
                pgn += GetColumnFromInt(srcCol);
            }
            break;
        }
        pgn += GetColumnFromInt(destCol);
        pgn += std::to_string(destRow + 1);
        if (PawnPromoted){
            pgn += "=Q";
        }
    }
    return pgn;
}

#endif // MOVE_CONTENT_H


