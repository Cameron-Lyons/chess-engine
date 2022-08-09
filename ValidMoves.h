#include "PieceMoves.h"
#include "ChessBoard.h"
#include <vector>


static bool BlackAttackBoard[64];
static bool WhiteAttackBoard[64];


static int BlackKingPosition;
static int WhiteKingPosition;


static bool AnalyzeMove(Board board, int dest, Piece piece){
    // pawns cannot attack where they move
    if (piece.PieceColor == WHITE) {
        WhiteAttackBoard[dest] = true;
    } else {
        BlackAttackBoard[dest] = true;
    }

    if (board.squares[dest].Piece.PieceType == NONE) {
        piece.ValidMoves.push_back(dest);
        return true;
    }

    Piece attackedPiece = board.squares[dest].Piece;
    if (attackedPiece.PieceColor != piece.PieceColor) {
        if (attackedPiece.PieceType == KING) {
            if (piece.PieceColor == WHITE) {
                board.whiteChecked = true;
            } else {
                board.blackChecked = true;
            }
        } else {
            piece.ValidMoves.push_back(dest);
        }
    }
    else {
        return false;
    }
    attackedPiece.DefendedValue += piece.PieceValue;
}


void CheckValidMovesPawn(std::vector<int> moves, Piece piece, int start, Board board, int count){
    for (int i=0; i<count; i++){
        int dest = moves[i];
        if (dest%8 != start%8){
            if (piece.PieceColor == WHITE){
                WhiteAttackBoard[dest] = true;
            }
            else {
                BlackAttackBoard[dest] = true;
            }
        }
        else if (board.squares[dest].Piece.PieceType != NONE){
            return;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }
    }
}


void AnalyzeMovePawn(Board board, int dest, Piece piece){
    Piece attackedPiece = board.squares[dest].Piece;
    if (attackedPiece.PieceType == NONE){
        return;
    }
    if (piece.PieceColor == WHITE){
        WhiteAttackBoard[dest] = true;
        if (attackedPiece.PieceColor == piece.PieceColor){
            attackedPiece.DefendedValue += piece.PieceValue;
            return;
        }
        else {
            attackedPiece.AttackedValue += piece.PieceValue;
        }
        if (attackedPiece.PieceType == KING){
            board.blackChecked = true;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }

    }
    else {
        BlackAttackBoard[dest] = true;
        if (attackedPiece.PieceColor == piece.PieceColor){
            return;
        }
        if (attackedPiece.PieceType == KING){
            board.whiteChecked = true;
        }
        else{
            piece.ValidMoves.push_back(dest);
        }
    }
    return;
}

