#pragma once

#include "Bitboard.h"
#include "CastlingConstants.h"
#include "ChessPiece.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

class BitboardPosition {
private:
    Bitboard pieces[2][6];
    Bitboard occupancy[3];
    uint8_t sideToMove;
    uint8_t castlingRights;
    uint8_t epSquare;
    uint8_t halfmoveClock;
    uint16_t fullmoveNumber;
    ChessTimePoint lastMoveTime;
    uint64_t hash;
    static constexpr int PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5;
    static constexpr int WHITE = 0, BLACK = 1;

public:
    BitboardPosition();
    void setFromFEN(const std::string& fen);
    std::string toFEN() const;

    ChessPieceType getPieceAt(int square) const {
        Bitboard mask = 1ULL << square;

        if (occupancy[WHITE] & mask) {
            if (pieces[WHITE][PAWN] & mask) {
                return ChessPieceType::PAWN;
            }
            if (pieces[WHITE][KNIGHT] & mask) {
                return ChessPieceType::KNIGHT;
            }
            if (pieces[WHITE][BISHOP] & mask) {
                return ChessPieceType::BISHOP;
            }
            if (pieces[WHITE][ROOK] & mask) {
                return ChessPieceType::ROOK;
            }
            if (pieces[WHITE][QUEEN] & mask) {
                return ChessPieceType::QUEEN;
            }
            if (pieces[WHITE][KING] & mask) {
                return ChessPieceType::KING;
            }
        }

        if (occupancy[BLACK] & mask) {
            if (pieces[BLACK][PAWN] & mask) {
                return ChessPieceType::PAWN;
            }
            if (pieces[BLACK][KNIGHT] & mask) {
                return ChessPieceType::KNIGHT;
            }
            if (pieces[BLACK][BISHOP] & mask) {
                return ChessPieceType::BISHOP;
            }
            if (pieces[BLACK][ROOK] & mask) {
                return ChessPieceType::ROOK;
            }
            if (pieces[BLACK][QUEEN] & mask) {
                return ChessPieceType::QUEEN;
            }
            if (pieces[BLACK][KING] & mask) {
                return ChessPieceType::KING;
            }
        }

        return ChessPieceType::NONE;
    }

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

    bool isEmpty(int square) const {
        return !(occupancy[2] & (1ULL << square));
    }

    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const {
        return pieces[color == ChessPieceColor::WHITE ? WHITE : BLACK][static_cast<int>(type)];
    }

    Bitboard getColorBitboard(ChessPieceColor color) const {
        return occupancy[color == ChessPieceColor::WHITE ? WHITE : BLACK];
    }

    Bitboard getAllPieces() const {
        return occupancy[2];
    }

    void makeMove(int from, int to, ChessPieceType promotion = ChessPieceType::NONE);

    ChessPieceColor getSideToMove() const {
        return sideToMove == WHITE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    }

    void setSideToMove(ChessPieceColor color) {
        sideToMove = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    }

    bool canCastle(ChessPieceColor color, bool kingside) const {
        int shift = (color == ChessPieceColor::WHITE ? 0 : 2) + (kingside ? 0 : 1);
        return (castlingRights >> shift) & 1;
    }

    void setCastlingRights(bool whiteKingside, bool whiteQueenside, bool blackKingside,
                           bool blackQueenside) {
        castlingRights = (whiteKingside ? CastlingConstants::kWhiteKingsideCastlingRight : 0) |
                         (whiteQueenside ? CastlingConstants::kWhiteQueensideCastlingRight : 0) |
                         (blackKingside ? CastlingConstants::kBlackKingsideCastlingRight : 0) |
                         (blackQueenside ? CastlingConstants::kBlackQueensideCastlingRight : 0);
    }

    int getEnPassantSquare() const {
        return epSquare < CastlingConstants::kNoEnPassantSquareBitboard
                   ? epSquare
                   : CastlingConstants::kNoEnPassantSquareMailbox;
    }

    void setEnPassantSquare(int square) {
        epSquare = (square >= 0 && square < NUM_SQUARES)
                       ? square
                       : CastlingConstants::kNoEnPassantSquareBitboard;
    }

    uint64_t getHash() const {
        return hash;
    }

    template <typename Func>
    void forEachPiece(ChessPieceColor color, ChessPieceType type, Func&& func) const {
        Bitboard bb = getPieceBitboard(type, color);
        while (bb) {
            int square = std::countr_zero(bb);
            func(square);
            bb &= bb - 1;
        }
    }

    ChessTimePoint getCurrentTime() const {
        return ChessClock::now();
    }

    ChessDuration getTimeSinceLastMove() const {
        return std::chrono::duration_cast<ChessDuration>(ChessClock::now() - lastMoveTime);
    }

    void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }

    std::string toString() const;

private:
    void updateOccupancy();
    void removePiece(int square, ChessPieceColor color, ChessPieceType type);
    void placePiece(int square, ChessPieceColor color, ChessPieceType type);
    void updateHash(int square, ChessPieceColor color, ChessPieceType type);
};
