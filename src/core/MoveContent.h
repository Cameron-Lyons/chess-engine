#ifndef MOVE_CONTENT_H
#define MOVE_CONTENT_H

#include "ChessPiece.h"
#include <string>

enum class MoveType { NORMAL, CASTLE_KING, CASTLE_QUEEN, EN_PASSANT, PROMOTION };

struct MoveContent {
    int src;
    int dest;
    ChessPieceType piece;
    ChessPieceType capture;
    ChessPieceType promotion;
    MoveType type;
    int score;

    MoveContent()
        : src(-1), dest(-1), piece(ChessPieceType::NONE), capture(ChessPieceType::NONE),
          promotion(ChessPieceType::NONE), type(MoveType::NORMAL), score(0) {}

    bool operator==(const MoveContent& other) const {
        return src == other.src && dest == other.dest && promotion == other.promotion;
    }

    bool operator!=(const MoveContent& other) const {
        return !(*this == other);
    }
};

struct PieceMoving {
    int dest;
    int src;
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;

    PieceMoving(ChessPieceColor pieceColor, ChessPieceType pieceType, bool moved) {
        this->dest = 0;
        this->src = 0;
        this->moved = moved;
        this->pieceType = pieceType;
        this->pieceColor = pieceColor;
    }

    PieceMoving(ChessPieceType pieceType) {
        this->pieceType = pieceType;
        this->pieceColor = ChessPieceColor::WHITE;
        this->src = 0;
        this->dest = 0;
        this->moved = false;
    }
};

struct PieceTaken {
    bool moved;
    ChessPieceType pieceType;
    ChessPieceColor pieceColor;
    int position;

    PieceTaken(ChessPieceColor pieceColor, ChessPieceType pieceType, bool moved) {
        this->moved = moved;
        this->pieceType = pieceType;
        this->pieceColor = pieceColor;
        this->position = 0;
    }

    PieceTaken(ChessPieceType pieceType) {
        this->pieceType = pieceType;
        this->pieceColor = ChessPieceColor::WHITE;
        this->position = 0;
        this->moved = false;
    }
};

extern PieceMoving MovingPiece;
extern PieceMoving MovingPieceSecondary;
extern bool PawnPromoted;

std::string GetColumnFromInt(int column) {
    switch (column) {
        case 0:
            return "a";
        case 1:
            return "b";
        case 2:
            return "c";
        case 3:
            return "d";
        case 4:
            return "e";
        case 5:
            return "f";
        case 6:
            return "g";
        case 7:
            return "h";
        default:
            return "Unknown";
    }
}

std::string GetPgnMove(ChessPieceType pieceType) {
    switch (pieceType) {
        case ChessPieceType::BISHOP:
            return "B";
        case ChessPieceType::KING:
            return "K";
        case ChessPieceType::KNIGHT:
            return "N";
        case ChessPieceType::QUEEN:
            return "Q";
        case ChessPieceType::ROOK:
            return "R";
        default:
            return "";
    }
}

std::string PortableGameNotation() {
    std::string pgn;
    int srcCol = MovingPiece.src % 8;
    int srcRow = MovingPiece.src / 8;
    int destCol = MovingPiece.dest % 8;
    int destRow = MovingPiece.dest / 8;

    if (MovingPieceSecondary.pieceColor == ChessPieceColor::WHITE) {
        if (MovingPieceSecondary.src == 6) {
            pgn += "O-O";
        } else if (MovingPieceSecondary.src == 2) {
            pgn += "O-O-O";
        }
    } else {
        if (MovingPieceSecondary.src == 62) {
            pgn += "O-O";
        } else if (MovingPieceSecondary.src == 58) {
            pgn += "O-O-O";
        }
    }

    if (MovingPiece.pieceType != ChessPieceType::KING) {
        pgn += GetPgnMove(MovingPiece.pieceType);
        switch (MovingPiece.pieceType) {
            case ChessPieceType::KNIGHT:
                pgn += GetColumnFromInt(srcCol);
                pgn += std::to_string(srcRow + 1);
                break;
            case ChessPieceType::ROOK:
                pgn += GetColumnFromInt(srcCol);
                pgn += std::to_string(srcRow + 1);
                break;
            case ChessPieceType::BISHOP:
                pgn += GetColumnFromInt(srcCol);
                pgn += std::to_string(srcRow + 1);
                break;
            case ChessPieceType::QUEEN:
                pgn += GetColumnFromInt(srcCol);
                pgn += std::to_string(srcRow + 1);
                break;
            case ChessPieceType::KING:
                pgn += GetColumnFromInt(srcCol);
                pgn += std::to_string(srcRow + 1);
                break;
            case ChessPieceType::PAWN:
                if (srcCol != destCol) {
                    pgn += GetColumnFromInt(srcCol);
                }
                break;
            case ChessPieceType::NONE:
            default:
                break;
        }
        pgn += GetColumnFromInt(destCol);
        pgn += std::to_string(destRow + 1);
        if (PawnPromoted) {
            pgn += "=Q";
        }
    }
    return pgn;
}

#endif