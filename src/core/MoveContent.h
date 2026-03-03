#pragma once

#include "ChessPiece.h"

#include <cstdint>

enum class MoveType : std::uint8_t { NORMAL, CASTLE_KING, CASTLE_QUEEN, EN_PASSANT, PROMOTION };

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
