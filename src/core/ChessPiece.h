#pragma once

#include <cstdint>

enum class ChessPieceColor : std::uint8_t { WHITE, BLACK };

enum class ChessPieceType : std::uint8_t { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NONE };

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

    Piece()
        : PieceColor(ChessPieceColor::WHITE), PieceType(ChessPieceType::NONE), PieceValue(0),
          AttackedValue(0), DefendedValue(0), PieceActionValue(0), selected(false), moved(false) {}

    Piece(ChessPieceColor color, ChessPieceType type)
        : PieceColor(color), PieceType(type), PieceValue(getPieceValue(type)), AttackedValue(0),
          DefendedValue(0), PieceActionValue(getPieceActionValue(type)), selected(false),
          moved(false) {}

    static constexpr short getPieceValue(ChessPieceType pieceType) {
        constexpr short values[] = {100, 320, 325, 500, 975, 32767, 0};
        return values[static_cast<int>(pieceType)];
    }

    static constexpr short getPieceActionValue(ChessPieceType pieceType) {
        constexpr short values[] = {6, 3, 3, 2, 2, 1, 0};
        return values[static_cast<int>(pieceType)];
    }

    static constexpr ChessPieceType fromFenChar(char fenChar) {
        switch (fenChar) {
            case 'p':
            case 'P':
                return ChessPieceType::PAWN;
            case 'n':
            case 'N':
                return ChessPieceType::KNIGHT;
            case 'b':
            case 'B':
                return ChessPieceType::BISHOP;
            case 'r':
            case 'R':
                return ChessPieceType::ROOK;
            case 'q':
            case 'Q':
                return ChessPieceType::QUEEN;
            case 'k':
            case 'K':
                return ChessPieceType::KING;
            default:
                return ChessPieceType::NONE;
        }
    }
};
