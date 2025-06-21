#ifndef CHESS_PIECE_H
#define CHESS_PIECE_H

#include <string>
#include <vector>

enum class ChessPieceColor {
    WHITE,
    BLACK
};

enum class ChessPieceType {
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
        std::string possibleMoves[64];
        std::vector<int> ValidMoves;
        
        // Constructor
        Piece() : PieceColor(ChessPieceColor::WHITE), PieceType(ChessPieceType::NONE), PieceValue(0), 
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
    
    static constexpr short getPieceValue(ChessPieceType pieceType){
        switch(pieceType) {
            case ChessPieceType::PAWN:
                return 100;
            case ChessPieceType::KNIGHT:
                return 320;
            case ChessPieceType::BISHOP:
                return 325;
            case ChessPieceType::ROOK:
                return 500;
            case ChessPieceType::QUEEN:
                return 975;
            case ChessPieceType::KING:
                return 32767;
            case ChessPieceType::NONE:
                return 0;
            default:
                return 0;
        }
    }

    static constexpr short getPieceActionValue(ChessPieceType pieceType){
        switch(pieceType) {
            case ChessPieceType::PAWN:
                return 6;
            case ChessPieceType::KNIGHT:
                return 3;
            case ChessPieceType::BISHOP:
                return 3;
            case ChessPieceType::ROOK:
                return 2;
            case ChessPieceType::QUEEN:
                return 2;
            case ChessPieceType::KING:
                return 1;
            case ChessPieceType::NONE:
                return 0;
            default:
                return 0;
        }
    }
};

#endif // CHESS_PIECE_H
