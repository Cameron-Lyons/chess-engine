#include "ChessPiece.h"


struct PieceMoving{
    int dest;
    int src;
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;
}