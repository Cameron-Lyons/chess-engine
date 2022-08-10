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


void GenValidMovesKingCastle(Board board, Piece king){
    if (king.PieceType == NONE){
        return;
    }
    if (king.moved){
        return;
    }
    if (king.PieceColor == WHITE && ~board.whiteCanCastle){
        return;
    }
    if (king.PieceColor == BLACK && ~board.blackCanCastle){
        return;
    }
    if (king.PieceColor == WHITE && board.whiteChecked){
        return;
    }
    if (king.PieceColor == BLACK && board.blackChecked){
        return;
    }
    if (king.PieceColor == BLACK){
        if (board.squares[63].Piece.PieceType == ROOK){
            if (board.squares[62].Piece.PieceType == NONE
                && board.squares[61].Piece.PieceType == NONE){
                if (!BlackAttackBoard[61] && !BlackAttackBoard[62] && !BlackAttackBoard[63]){
                    king.ValidMoves.push_back(62);
                    WhiteAttackBoard[62] = true;
                }
            }
        }
        if (board.squares[56].Piece.PieceType == ROOK){
            if (board.squares[57].Piece.PieceType == NONE
                && board.squares[58].Piece.PieceType == NONE
                && board.squares[59].Piece.PieceType == NONE){
                if (!BlackAttackBoard[59] && !BlackAttackBoard[58] && !BlackAttackBoard[57]){
                    king.ValidMoves.push_back(58);
                    WhiteAttackBoard[58] = true;
                }
            }
        }
    if (king.PieceColor == WHITE){
        if (board.squares[7].Piece.PieceType == ROOK){
            if (board.squares[6].Piece.PieceType == NONE
                && board.squares[5].Piece.PieceType == NONE){
                if (!WhiteAttackBoard[5] && !WhiteAttackBoard[6] && !WhiteAttackBoard[7]){
                    king.ValidMoves.push_back(6);
                    BlackAttackBoard[6] = true;
                }
            }
        }
        if (board.squares[0].Piece.PieceType == ROOK){
            if (board.squares[1].Piece.PieceType == NONE
                && board.squares[2].Piece.PieceType == NONE
                && board.squares[3].Piece.PieceType == NONE){
                if (!WhiteAttackBoard[3] && !WhiteAttackBoard[2] && !WhiteAttackBoard[1]){
                    king.ValidMoves.push_back(2);
                    BlackAttackBoard[2] = true;
                }
            }
        }
    }
    }
}


void GenValidMoves(Board board){
    board.whiteChecked = false;
    board.blackChecked = false;

    static bool BlackAttackBoard[64];
    static bool WhiteAttackBoard[64];

    for (int x=0; x<64; x++){
        Square square = board.squares[x];
        if (square.Piece.PieceType == NONE) {
            continue;
        }
        square.Piece.ValidMoves.clear();
        switch (square.Piece.PieceType)
        {
        case PAWN:
            if (square.Piece.PieceColor == WHITE){
                CheckValidMovesPawn(MoveArrays.whitePawnMoves.moves, square.Piece, x, board, MoveArrays.whitePawnTotalMoves[x]);
            }
            else{
                CheckValidMovesPawn(MoveArrays.blackPawnMoves.moves, square.Piece, x, board, MoveArrays.blackPawnTotalMoves[x]);
            }
            break;
        case KNIGHT:
            for (int i=0; i<MoveArrays.knightTotalMoves[x]; i++){
                AnalyzeMove(board, MoveArrays.knightMoves.moves[i], square.Piece);
            }
            break;
        case BISHOP:
            for (int i=0; i<MoveArrays.bishopTotalMoves1[x]; i++){
                if(~AnalyzeMove(board, MoveArrays.bishopMoves1.moves[i], square.Piece);){
                    break;
                }
            }   
            for (int i=0; i<MoveArrays.bishopTotalMoves2[x]; i++){
                if(~AnalyzeMove(board, MoveArrays.bishopMoves2.moves[i], square.Piece);){
                    break;
                }
            }
            for (int i=0; i<MoveArrays.bishopTotalMoves3[x]; i++){
                if(~AnalyzeMove(board, MoveArrays.bishopMoves3.moves[i], square.Piece);){
                    break;
                }
            }
            for (int i=0; i<MoveArrays.bishopTotalMoves4[x]; i++){
                if(~AnalyzeMove(board, MoveArrays.bishopMoves4.moves[i], square.Piece);){
                    break;
                }
            }
        
        }
    }
}