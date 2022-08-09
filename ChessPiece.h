#include <string>

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
        static ChessPieceType PieceType;
        short PieceValue;
        short AttackedValue;
        short DefendedValue;
        short PieceActionValue;
        bool selected;
        bool moved;
        std::string possibleMoves [64];

    static short getPieceValue(ChessPieceType pieceType){
        switch(PieceType) {
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
        }
    }

    short getPieceActionValue(ChessPieceType pieceType){
        switch(PieceType) {
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
        }
    };
};
