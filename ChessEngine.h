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

    // Validate input
    if (src < 0 || src >= 64 || dest < 0 || dest >= 64) {
        return false;
    }

    Piece piece = ChessBoard.squares[src].Piece;
    
    // Check if there's a piece at source
    if (piece.PieceType == NONE) {
        return false;
    }
    
    // Check if it's the correct player's turn
    if (piece.PieceColor != ChessBoard.turn) {
        return false;
    }

    PrevBoard = ChessBoard;  // Save previous board state

    // Check if the move is in the valid moves list
    bool moveIsValid = false;
    for (int validDest : piece.ValidMoves) {
        if (validDest == dest) {
            moveIsValid = true;
            break;
        }
    }
    
    if (!moveIsValid) {
        return false;
    }

    bool promotePawn = (piece.PieceType == PAWN && (destRow == 0 || destRow == 7));
    
    // Make the move
    ChessBoard.squares[dest].Piece = piece;
    ChessBoard.squares[src].Piece = Piece();
    
    // Mark piece as moved
    ChessBoard.squares[dest].Piece.moved = true;
    
    // Handle pawn promotion
    if (promotePawn) {
        ChessBoard.squares[dest].Piece.PieceType = QUEEN;
    }
    
    // Handle castling
    if (piece.PieceType == KING) {
        if (piece.PieceColor == WHITE) {
            if (destCol == 6 && srcCol == 4) { // Kingside castle
                ChessBoard.squares[5].Piece = ChessBoard.squares[7].Piece;
                ChessBoard.squares[7].Piece = Piece();
                ChessBoard.whiteCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) { // Queenside castle
                ChessBoard.squares[3].Piece = ChessBoard.squares[0].Piece;
                ChessBoard.squares[0].Piece = Piece();
                ChessBoard.whiteCanCastle = false;
            }
        } else {
            if (destCol == 6 && srcCol == 4) { // Kingside castle
                ChessBoard.squares[61].Piece = ChessBoard.squares[63].Piece;
                ChessBoard.squares[63].Piece = Piece();
                ChessBoard.blackCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) { // Queenside castle
                ChessBoard.squares[59].Piece = ChessBoard.squares[56].Piece;
                ChessBoard.squares[56].Piece = Piece();
                ChessBoard.blackCanCastle = false;
            }
        }
    }
    
    // Update king positions
    if (piece.PieceType == KING) {
        if (piece.PieceColor == WHITE) {
            WhiteKingPosition = dest;
        } else {
            BlackKingPosition = dest;
        }
    }
    
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
