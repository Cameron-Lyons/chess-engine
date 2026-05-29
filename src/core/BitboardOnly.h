#pragma once

// Lightweight bitboard view used by NNUEBitboard feature extraction.
// Piece placement is synced from the canonical Board via fromBoard(); this type
// does not parse FEN or apply moves on its own.

#include "Bitboard.h"
#include "ChessPiece.h"

#include <cstdint>
#include <string>

struct Board;

class BitboardPosition {
private:
    Bitboard pieces[2][6];
    Bitboard occupancy[3];
    uint8_t sideToMove;
    static constexpr int PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5;
    static constexpr int WHITE = 0, BLACK = 1;

public:
    BitboardPosition();

    static BitboardPosition fromBoard(const Board& board);

    ChessPieceColor getColorAt(int square) const {
        Bitboard mask = 1ULL << square;
        if (occupancy[WHITE] & mask) {
            return ChessPieceColor::WHITE;
        }
        if (occupancy[BLACK] & mask) {
            return ChessPieceColor::BLACK;
        }
        return ChessPieceColor::WHITE;
    }

    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const {
        return pieces[color == ChessPieceColor::WHITE ? WHITE : BLACK][static_cast<int>(type)];
    }

    Bitboard getAllPieces() const {
        return occupancy[2];
    }

    ChessPieceColor getSideToMove() const {
        return sideToMove == WHITE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    }
};
