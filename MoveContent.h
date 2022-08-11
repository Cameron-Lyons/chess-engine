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


std::string string GetColumnFromInt(int column)
{
    switch (column)
    {
        case 1:
            return "a";
        case 2:
            return "b";
        case 3:
            return "c";
        case 4:
            return "d";
        case 5:
            return "e";
        case 6:
            return "f";
        case 7:
            return "g";
        case 8:
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
    else {
        pgn += GetPGBMove(MovingPiece.PieceType);
        switch (MovingPiece.PieceType)
        {
        case KNIGHT:
            pgn += GetColumnFromInt(srcCol + 1);
            pgn += srcRow;
            break;
        case ROOK:
            pgn += GetColumnFromInt(srcCol + 1);
            pgn += srcRow;
            break;
        case PAWN:
            if (srcCol != destCol){
                pgn += GetColummnFromInt(srcCol + 1);
            }
            break;
        }
    pgn += GetColumnFromInt(destCol + 1);
    pgn += destRow;
    if (PawnPromoted){
        pgn += "Q";
    }
    }
    return pgn;
}


