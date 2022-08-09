#include "ChessPiece.h"

struct Square {
    Piece Piece;
    int loc;
};

class Board{
    Square squares[64];
    bool whiteChecked;
    bool blackChecked;
    bool whiteCheckmated;
    bool blackCheckmated;
    bool stalemate;
    bool whiteCanCastle;
    bool blackCanCastle;
    bool isEndGame;
    ChessPieceColor turn;
    int moveCount;

    Board(){
        for(int i = 0; i < 64; i++){
            squares[i].Piece.PieceType = NONE;
        }
        whiteChecked = false;
        blackChecked = false;
        whiteCheckmated = false;
        blackCheckmated = false;
        stalemate = false;
        whiteCanCastle = true;
        blackCanCastle = true;
        isEndGame = false;
        turn = WHITE;
        moveCount = 0;
    };

    Board(const Board &oldBoard) {
        for(int i = 0; i < 64; i++){
            squares[i].Piece.PieceType = NONE;
        }
        whiteChecked = oldBoard.whiteChecked;
        blackChecked = oldBoard.blackChecked;
        whiteCheckmated = oldBoard.whiteCheckmated;
        blackCheckmated = oldBoard.blackCheckmated;
        stalemate = oldBoard.stalemate;
        whiteCanCastle = oldBoard.whiteCanCastle;
        blackCanCastle = oldBoard.blackCanCastle;
        isEndGame = oldBoard.isEndGame;
        turn = oldBoard.turn;
        moveCount = oldBoard.moveCount;
    }  

    bool PromotePawns(Board board,
                      Piece piece,
                      int destSquare,
                      ChessPieceType promotePiece) {
        if (piece.PieceType == PAWN) {
            {
            if (destSquare < 8)
            {
                board.squares[destSquare].Piece.PieceType = promotePiece;
                return true;
            }
            if (destSquare > 55)
            {
                board.squares[destSquare].Piece.PieceType = promotePiece;
                return true;
            }
        }
            return false;                        
        }
    }

    void Castle(Board board,
                Piece piece,
                int destSquare) {
        if (piece.PieceType == KING) {
            if (piece.PieceColor == WHITE && board.whiteCanCastle) {
                if (destSquare == 2) {
                    board.squares[destSquare].Piece.PieceType = KING;
                    board.squares[3].Piece.PieceType = ROOK;
                    board.squares[0].Piece.PieceType = NONE;
                    board.whiteCanCastle = false;

            }
                if (destSquare == 4) {
                    board.squares[destSquare].Piece.PieceType = KING;
                    board.squares[5].Piece.PieceType = ROOK;
                    board.squares[7].Piece.PieceType = NONE;
                    board.whiteCanCastle = false;
                }
            }
            if (piece.PieceColor == BLACK && board.blackCanCastle) {
                if (destSquare == 58) {
                    board.squares[destSquare].Piece.PieceType = KING;
                    board.squares[59].Piece.PieceType = ROOK;
                    board.squares[56].Piece.PieceType = NONE;
                    board.blackCanCastle = false;
                }
                if (destSquare == 60) {
                    board.squares[destSquare].Piece.PieceType = KING;
                    board.squares[61].Piece.PieceType = ROOK;
                    board.squares[63].Piece.PieceType = NONE;
                    board.blackCanCastle = false;
                }
            }
        }
    }
    void MovePiece(Board board, int srcPos, int destPos, bool promotePawn) {
        Piece piece = board.squares[srcPos].Piece;
        if (piece.PieceColor == BLACK){
            board.moveCount ++;
        }
        if (promotePawn) {
            board.PromotePawns(board, piece, destPos, QUEEN);
        }
        board.squares[destPos].Piece = piece;
        board.squares[srcPos].Piece.PieceType = NONE;
        board.Castle(board, piece, destPos);
    }
};
