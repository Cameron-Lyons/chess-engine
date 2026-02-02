#ifndef BITBOARD_ONLY_H
#define BITBOARD_ONLY_H

#include "Bitboard.h"
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

    inline ChessPieceType getPieceAt(int square) const {
        Bitboard mask = 1ULL << square;

        if (occupancy[WHITE] & mask) {
            if (pieces[WHITE][PAWN] & mask)
                return ChessPieceType::PAWN;
            if (pieces[WHITE][KNIGHT] & mask)
                return ChessPieceType::KNIGHT;
            if (pieces[WHITE][BISHOP] & mask)
                return ChessPieceType::BISHOP;
            if (pieces[WHITE][ROOK] & mask)
                return ChessPieceType::ROOK;
            if (pieces[WHITE][QUEEN] & mask)
                return ChessPieceType::QUEEN;
            if (pieces[WHITE][KING] & mask)
                return ChessPieceType::KING;
        }

        if (occupancy[BLACK] & mask) {
            if (pieces[BLACK][PAWN] & mask)
                return ChessPieceType::PAWN;
            if (pieces[BLACK][KNIGHT] & mask)
                return ChessPieceType::KNIGHT;
            if (pieces[BLACK][BISHOP] & mask)
                return ChessPieceType::BISHOP;
            if (pieces[BLACK][ROOK] & mask)
                return ChessPieceType::ROOK;
            if (pieces[BLACK][QUEEN] & mask)
                return ChessPieceType::QUEEN;
            if (pieces[BLACK][KING] & mask)
                return ChessPieceType::KING;
        }

        return ChessPieceType::NONE;
    }

    inline ChessPieceColor getColorAt(int square) const {
        Bitboard mask = 1ULL << square;
        if (occupancy[WHITE] & mask)
            return ChessPieceColor::WHITE;
        if (occupancy[BLACK] & mask)
            return ChessPieceColor::BLACK;
        return ChessPieceColor::WHITE;
    }

    inline bool isEmpty(int square) const {
        return !(occupancy[2] & (1ULL << square));
    }

    inline Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const {
        return pieces[color == ChessPieceColor::WHITE ? WHITE : BLACK][static_cast<int>(type)];
    }

    inline Bitboard getColorBitboard(ChessPieceColor color) const {
        return occupancy[color == ChessPieceColor::WHITE ? WHITE : BLACK];
    }

    inline Bitboard getAllPieces() const {
        return occupancy[2];
    }

    void makeMove(int from, int to, ChessPieceType promotion = ChessPieceType::NONE);
    void unmakeMove(int from, int to, ChessPieceType captured,
                    ChessPieceType promotion = ChessPieceType::NONE);

    inline ChessPieceColor getSideToMove() const {
        return sideToMove == WHITE ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;
    }

    inline void setSideToMove(ChessPieceColor color) {
        sideToMove = (color == ChessPieceColor::WHITE) ? WHITE : BLACK;
    }

    inline bool canCastle(ChessPieceColor color, bool kingside) const {
        int shift = (color == ChessPieceColor::WHITE ? 0 : 2) + (kingside ? 0 : 1);
        return (castlingRights >> shift) & 1;
    }

    inline void setCastlingRights(bool whiteKingside, bool whiteQueenside, bool blackKingside,
                                  bool blackQueenside) {
        castlingRights = (whiteKingside ? 1 : 0) | (whiteQueenside ? 2 : 0) |
                         (blackKingside ? 4 : 0) | (blackQueenside ? 8 : 0);
    }

    inline int getEnPassantSquare() const {
        return epSquare < 64 ? epSquare : -1;
    }

    inline void setEnPassantSquare(int square) {
        epSquare = (square >= 0 && square < 64) ? square : 64;
    }

    inline uint64_t getHash() const {
        return hash;
    }

    template <typename Func>
    inline void forEachPiece(ChessPieceColor color, ChessPieceType type, Func&& func) const {
        Bitboard bb = getPieceBitboard(type, color);
        while (bb) {
            int square = __builtin_ctzll(bb);
            func(square);
            bb &= bb - 1;
        }
    }

    inline ChessTimePoint getCurrentTime() const {
        return ChessClock::now();
    }

    inline ChessDuration getTimeSinceLastMove() const {
        return std::chrono::duration_cast<ChessDuration>(ChessClock::now() - lastMoveTime);
    }

    inline void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }

    std::string toString() const;

private:
    void updateOccupancy();
    void removePiece(int square, ChessPieceColor color, ChessPieceType type);
    void placePiece(int square, ChessPieceColor color, ChessPieceType type);
    void updateHash(int square, ChessPieceColor color, ChessPieceType type);
};

class BitboardAdapter {
private:
    BitboardPosition position;

public:
    inline ChessPieceType getPieceType(int pos) const {
        return position.getPieceAt(pos);
    }

    inline ChessPieceColor getPieceColor(int pos) const {
        return position.getColorAt(pos);
    }

    inline Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const {
        return position.getPieceBitboard(type, color);
    }

    inline ChessPieceColor getTurn() const {
        return position.getSideToMove();
    }

    inline void setTurn(ChessPieceColor color) {
        position.setSideToMove(color);
    }

    inline bool movePiece(int from, int to) {
        ChessPieceType movingPiece = position.getPieceAt(from);
        if (movingPiece == ChessPieceType::NONE)
            return false;

        position.makeMove(from, to);
        return true;
    }

    inline void updateBitboards() {}

    inline std::string toFEN() const {
        return position.toFEN();
    }

    inline void InitializeFromFEN(const std::string& fen) {
        position.setFromFEN(fen);
    }

    inline BitboardPosition& getPosition() {
        return position;
    }
    inline const BitboardPosition& getPosition() const {
        return position;
    }
};

#endif