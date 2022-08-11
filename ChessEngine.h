#include "ChessBoard.h"
#include "ChessPiece.h"
#include <stack>

Board ChessBoard;
Board PrevBoard;


void Engine(){
    Board ChessBoard = Board();
    std::stack<int> MoveHistory;
}


void RegisterPiece(int col, int row, Piece piece){
    int position = col + row*8;
    ChessBoard.squares[position].Piece = piece;
    return;
}


bool MovePiece(int srcCol, int srcRow,
                int destCol, int destRow){
    int src = srcCol + srcRow*8;
    int dest = destCol + destRow*8;

    Piece piece = ChessBoard.squares[src].Piece;
    Board PrevBoard = Board();

    Board.movePiece(ChessBoard, src, dest, PromoteToPieceType);
    PieceValidMoves.GenerateValidMoves(ChessBoard);

    if (piece.PieceColor == WHITE){
        if (ChessBoard.whiteChecked){
            ChessBoard = PrevBoard;
            PieceValidMoves.GenerateValidMoves(ChessBoard);
            return false;
        }
    }
    else {
        if (ChessBoard.blackChecked){
            ChessBoard = PrevBoard;
            PieceValidMoves.GenerateValidMoves(ChessBoard);
            return false;
        }
    }
    MoveHistory.push(ChessBoard.LastMove);
    return true;
}


