#pragma once

#include <string>
#include <vector>

enum class ChessPieceColor { WHITE, BLACK };

enum class ChessPieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE };

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

    Piece()
        : PieceColor(ChessPieceColor::WHITE), PieceType(ChessPieceType::NONE), PieceValue(0),
          AttackedValue(0), DefendedValue(0), PieceActionValue(0), selected(false), moved(false) {
        ValidMoves.clear();
    }

    Piece(ChessPieceColor color, ChessPieceType type)
        : PieceColor(color), PieceType(type), AttackedValue(0), DefendedValue(0), selected(false),
          moved(false) {
        PieceValue = getPieceValue(type);
        PieceActionValue = getPieceActionValue(type);
        ValidMoves.clear();
    }

    static constexpr short getPieceValue(ChessPieceType pieceType) {
        constexpr short values[] = {100, 320, 325, 500, 975, 32767, 0};
        return values[static_cast<int>(pieceType)];
    }

    static constexpr short getPieceActionValue(ChessPieceType pieceType) {
        constexpr short values[] = {6, 3, 3, 2, 2, 1, 0};
        return values[static_cast<int>(pieceType)];
    }
};
