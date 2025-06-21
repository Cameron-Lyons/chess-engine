#include "ChessBoard.h"
#include "ChessPiece.h"
#include "ValidMoves.h"
#include <stack>
#include <string>

extern Board ChessBoard;  // Declare as extern
extern Board PrevBoard;   // Declare as extern
extern std::stack<int> MoveHistory;  // Declare as extern

void Engine(){
    ChessBoard = Board();
    ChessBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveHistory = std::stack<int>();
}

void RegisterPiece(int col, int row, Piece piece){
    int position = col + row*8;
    ChessBoard.squares[position].Piece = piece;
}

bool MovePiece(int srcCol, int srcRow,
                int destCol, int destRow){
    int src = srcCol + srcRow*8;
    int dest = destCol + destRow*8;

    Piece piece = ChessBoard.squares[src].Piece;
    PrevBoard = ChessBoard;  // Save previous board state

    bool promotePawn = (piece.PieceType == PAWN && (destRow == 0 || destRow == 7));
    ChessBoard.MovePiece(ChessBoard, src, dest, promotePawn);
    
    // Generate valid moves for the new position
    GenValidMoves(ChessBoard);

    // Check if move leaves own king in check
    if (piece.PieceColor == WHITE){
        if (ChessBoard.whiteChecked){
            ChessBoard = PrevBoard;
            GenValidMoves(ChessBoard);
            return false;
        }
    }
    else {
        if (ChessBoard.blackChecked){
            ChessBoard = PrevBoard;
            GenValidMoves(ChessBoard);
            return false;
        }
    }
    
    MoveHistory.push(ChessBoard.LastMove);
    return true;
}
