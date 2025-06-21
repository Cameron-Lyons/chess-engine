#ifndef CHESS_BOARD_H
#define CHESS_BOARD_H

#include "ChessPiece.h"
#include <array>

struct Square {
    Piece Piece;
    int loc;
    
    Square() : loc(0) {}
    Square(int location) : loc(location) {}
};

class Board{
    public:
        std::array<Square, 64> squares;
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
        int LastMove;

    Board() : 
        squares{}, 
        whiteChecked{false}, 
        blackChecked{false}, 
        whiteCheckmated{false}, 
        blackCheckmated{false}, 
        stalemate{false}, 
        whiteCanCastle{true}, 
        blackCanCastle{true}, 
        isEndGame{false}, 
        turn{ChessPieceColor::WHITE}, 
        moveCount{0}, 
        LastMove{-1} {
        for(int i = 0; i < 64; i++){
            squares[i] = Square(i);
            squares[i].Piece = Piece();
        }
    }

    Board(const Board &oldBoard) = default;
    Board& operator=(const Board &oldBoard) = default;

    bool PromotePawns(Board& board,
                      Piece piece,
                      int destSquare,
                      ChessPieceType promotePiece) {
        if (piece.PieceType == ChessPieceType::PAWN) {
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

    void Castle(Board& board,
                Piece piece,
                int destSquare) {
        if (piece.PieceType == ChessPieceType::KING) {
            if (piece.PieceColor == ChessPieceColor::WHITE && board.whiteCanCastle) {
                if (destSquare == 2) {
                    board.squares[destSquare].Piece.PieceType = ChessPieceType::KING;
                    board.squares[3].Piece.PieceType = ChessPieceType::ROOK;
                    board.squares[0].Piece.PieceType = ChessPieceType::NONE;
                    board.whiteCanCastle = false;
                }
                if (destSquare == 6) {
                    board.squares[destSquare].Piece.PieceType = ChessPieceType::KING;
                    board.squares[5].Piece.PieceType = ChessPieceType::ROOK;
                    board.squares[7].Piece.PieceType = ChessPieceType::NONE;
                    board.whiteCanCastle = false;
                }
            }
            if (piece.PieceColor == ChessPieceColor::BLACK && board.blackCanCastle) {
                if (destSquare == 58) {
                    board.squares[destSquare].Piece.PieceType = ChessPieceType::KING;
                    board.squares[59].Piece.PieceType = ChessPieceType::ROOK;
                    board.squares[56].Piece.PieceType = ChessPieceType::NONE;
                    board.blackCanCastle = false;
                }
                if (destSquare == 62) {
                    board.squares[destSquare].Piece.PieceType = ChessPieceType::KING;
                    board.squares[61].Piece.PieceType = ChessPieceType::ROOK;
                    board.squares[63].Piece.PieceType = ChessPieceType::NONE;
                    board.blackCanCastle = false;
                }
            }
        }
    }
    void MovePiece(Board& board, int srcPos, int destPos, bool promotePawn) {
        Piece piece = board.squares[srcPos].Piece;
        if (piece.PieceColor == ChessPieceColor::BLACK){
            board.moveCount++;
        }
        if (promotePawn) {
            board.PromotePawns(board, piece, destPos, ChessPieceType::QUEEN);
        }
        board.squares[destPos].Piece = piece;
        board.squares[srcPos].Piece.PieceType = ChessPieceType::NONE;
        board.Castle(board, piece, destPos);
        board.LastMove = destPos;
    }

    void InitializeFromFEN(const std::string& fen) {
        for(int i = 0; i < 64; i++){
            squares[i] = Square(i);
            squares[i].Piece = Piece();
        }
        
        size_t pos = 0;
        int fenRank = 7;  // FEN starts with rank 8, which is row 7
        int file = 0;
        
        while (pos < fen.length() && fen[pos] != ' ') {
            char c = fen[pos];
            if (c == '/') {
                fenRank--;
                file = 0;
            } else if (c >= '1' && c <= '8') {
                file += (c - '0');
            } else {
                ChessPieceType type = ChessPieceType::NONE;
                ChessPieceColor color = ChessPieceColor::WHITE;
                
                switch (tolower(c)) {
                    case 'p': type = ChessPieceType::PAWN; break;
                    case 'n': type = ChessPieceType::KNIGHT; break;
                    case 'b': type = ChessPieceType::BISHOP; break;
                    case 'r': type = ChessPieceType::ROOK; break;
                    case 'q': type = ChessPieceType::QUEEN; break;
                    case 'k': type = ChessPieceType::KING; break;
                }
                
                if (isupper(c)) color = ChessPieceColor::WHITE;
                else color = ChessPieceColor::BLACK;
                
                if (type != ChessPieceType::NONE && fenRank >= 0 && fenRank < 8 && file >= 0 && file < 8) {
                    int boardRow = fenRank;  // Now fenRank is the correct row
                    int idx = boardRow * 8 + file;
                    if (idx >= 0 && idx < 64) {
                        squares[idx].Piece = Piece(color, type);
                    }
                }
                file++;
            }
            pos++;
        }
    }
};

#endif // CHESS_BOARD_H
