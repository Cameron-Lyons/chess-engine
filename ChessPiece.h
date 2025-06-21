#ifndef CHESS_PIECE_H
#define CHESS_PIECE_H

#include <string>
#include <vector>


enum ChessPieceColor {
    WHITE,
    BLACK
};

enum ChessPieceType {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE  // This is used for empty squares
};

class Piece {
    public:
        ChessPieceColor PieceColor;
        ChessPieceType PieceType;
        short PieceValue;
        short AttackedValue;
        short DefendedValue;
        short PieceActionValue;
        bool selected;
        bool moved;
        std::string possibleMoves [64];
        std::vector<int> ValidMoves;
        
        // Constructor
        Piece() : PieceColor(WHITE), PieceType(NONE), PieceValue(0), 
                  AttackedValue(0), DefendedValue(0), PieceActionValue(0),
                  selected(false), moved(false) {
            ValidMoves.clear();
        }
        
        Piece(ChessPieceColor color, ChessPieceType type) : 
              PieceColor(color), PieceType(type), AttackedValue(0), 
              DefendedValue(0), selected(false), moved(false) {
            PieceValue = getPieceValue(type);
            PieceActionValue = getPieceActionValue(type);
            ValidMoves.clear();
        }
    
    static short getPieceValue(ChessPieceType pieceType){
        switch(pieceType) {
            case PAWN:
                return 100;
            case KNIGHT:
                return 320;
            case BISHOP:
                return 325;
            case ROOK:
                return 500;
            case QUEEN:
                return 975;
            case KING:
                return 32767;
            case NONE:
                return 0;
            default:
                return 0;
        }
    }

    static short getPieceActionValue(ChessPieceType pieceType){
        switch(pieceType) {
            case PAWN:
                return 6;
            case KNIGHT:
                return 3;
            case BISHOP:
                return 3;
            case ROOK:
                return 2;
            case QUEEN:
                return 2;
            case KING:
                return 1;
            case NONE:
                return 0;
            default:
                return 0;
        }
    }
};

#endif // CHESS_PIECE_H
