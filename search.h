#include "ChessBoard.h"

bool SearchForMate(ChessPieceColor movingSide, Board board, bool BlackMate, bool WhiteMate, bool StaleMate){
    bool foundNonCheckBlack = false;
    bool foundNonCheckWhite = false;

    for(int i = 0; i < 64; i++){
        Square square = board.squares[i];
        if (square.Piece.PieceType == NONE){
            continue;
        }
        if (square.Piece.PieceColor != movingSide){
            continue;
        } 
        for (std::vector<int>::iterator dest, square.Piece.ValidMoves){
            
        }
    }
}