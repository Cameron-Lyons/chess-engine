#include "core/ChessBoard.h"
#include "core/ChessPiece.h"
#include "search/ValidMoves.h"
#include <stack>
#include <string>
#include <iostream>

extern Board ChessBoard;
extern Board PrevBoard;
extern std::stack<int> MoveHistory;

void Engine(){
    ChessBoard = Board();
    ChessBoard.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveHistory = std::stack<int>();
}

void RegisterPiece(int col, int row, Piece piece){
    int position = col + row*8;
    ChessBoard.squares[position].piece = piece;
}

bool MovePiece(int srcCol, int srcRow,
                int destCol, int destRow){
    int src = srcCol + srcRow*8;
    int dest = destCol + destRow*8;

    if (src < 0 || src >= 64 || dest < 0 || dest >= 64) {
        return false;
    }

    Piece piece = ChessBoard.squares[src].piece;
    
    if (piece.PieceType == ChessPieceType::NONE) {
        return false;
    }
    
    if (piece.PieceColor != ChessBoard.turn) {
        return false;
    }

    if (!IsMoveLegal(ChessBoard, src, dest)) {
        return false;
    }

    PrevBoard = ChessBoard;

    bool promotePawn = (piece.PieceType == ChessPieceType::PAWN && (destRow == 0 || destRow == 7));

    ChessBoard.movePiece(src, dest);
    
    if (promotePawn) {
        ChessBoard.squares[dest].piece.PieceType = ChessPieceType::QUEEN;
        ChessBoard.updateBitboards();
    }
    
    if (piece.PieceType == ChessPieceType::KING) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            if (destCol == 6 && srcCol == 4) {
                ChessBoard.movePiece(7, 5);
                ChessBoard.whiteCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) {
                ChessBoard.movePiece(0, 3);
                ChessBoard.whiteCanCastle = false;
            }
        } else {
            if (destCol == 6 && srcCol == 4) {
                ChessBoard.movePiece(63, 61);
                ChessBoard.blackCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) {
                ChessBoard.movePiece(56, 59);
                ChessBoard.blackCanCastle = false;
            }
        }
    }
    
    if (piece.PieceType == ChessPieceType::KING) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            WhiteKingPosition = dest;
        } else {
            BlackKingPosition = dest;
        }
    }
    
    GenValidMoves(ChessBoard);

    if (IsKingInCheck(ChessBoard, piece.PieceColor)){
        ChessBoard = PrevBoard;
        GenValidMoves(ChessBoard);
        return false;
    }
    
    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    MoveHistory.push(ChessBoard.LastMove);
    return true;
}

bool MovePiece(int srcCol, int srcRow, int destCol, int destRow, ChessPieceType promotionPiece = ChessPieceType::QUEEN){
    int src = srcCol + srcRow*8;
    int dest = destCol + destRow*8;

    if (src < 0 || src >= 64 || dest < 0 || dest >= 64) {
        return false;
    }

    Piece piece = ChessBoard.squares[src].piece;
    
    if (piece.PieceType == ChessPieceType::NONE) {
        return false;
    }
    
    if (piece.PieceColor != ChessBoard.turn) {
        return false;
    }

    if (!IsMoveLegal(ChessBoard, src, dest)) {
        return false;
    }

    PrevBoard = ChessBoard;

    bool promotePawn = (piece.PieceType == ChessPieceType::PAWN && (destRow == 0 || destRow == 7));

    ChessBoard.movePiece(src, dest);
    
    if (promotePawn) {
        if (promotionPiece == ChessPieceType::QUEEN || promotionPiece == ChessPieceType::ROOK ||
            promotionPiece == ChessPieceType::BISHOP || promotionPiece == ChessPieceType::KNIGHT) {
            ChessBoard.squares[dest].piece.PieceType = promotionPiece;
        } else {
            ChessBoard.squares[dest].piece.PieceType = ChessPieceType::QUEEN;
        }
        ChessBoard.updateBitboards();
        
        std::cout << "Pawn promoted to " << 
                     ((promotionPiece == ChessPieceType::QUEEN) ? "Queen" :
                      (promotionPiece == ChessPieceType::ROOK) ? "Rook" :
                      (promotionPiece == ChessPieceType::BISHOP) ? "Bishop" :
                      (promotionPiece == ChessPieceType::KNIGHT) ? "Knight" : "Queen")
                     << "!\n";
    }
    
    if (piece.PieceType == ChessPieceType::KING) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            if (destCol == 6 && srcCol == 4) {
                ChessBoard.movePiece(7, 5);
                ChessBoard.whiteCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) {
                ChessBoard.movePiece(0, 3);
                ChessBoard.whiteCanCastle = false;
            }
        } else {
            if (destCol == 6 && srcCol == 4) {
                ChessBoard.movePiece(63, 61);
                ChessBoard.blackCanCastle = false;
            } else if (destCol == 2 && srcCol == 4) {
                ChessBoard.movePiece(56, 59);
                ChessBoard.blackCanCastle = false;
            }
        }
    }
    
    if (piece.PieceType == ChessPieceType::KING) {
        if (piece.PieceColor == ChessPieceColor::WHITE) {
            WhiteKingPosition = dest;
        } else {
            BlackKingPosition = dest;
        }
    }
    
    GenValidMoves(ChessBoard);

    if (IsKingInCheck(ChessBoard, piece.PieceColor)){
        ChessBoard = PrevBoard;
        GenValidMoves(ChessBoard);
        return false;
    }
    
    ChessBoard.turn = (ChessBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    
    MoveHistory.push(ChessBoard.LastMove);
    return true;
}
