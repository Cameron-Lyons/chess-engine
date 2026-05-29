#pragma once

#include "Bitboard.h"
#include "CastlingConstants.h"
#include "ChessPiece.h"
#include "Move.h"
#include "SquareSentinel.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

using ChessString = std::string_view;

using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

enum class ChessError : std::uint8_t {
    None,
    InvalidMove,
    NoPieceAtSource,
    WrongTurn,
    MoveLeavesKingInCheck,
    InvalidPosition,
    InvalidFEN,
    Timeout,
    OutOfMemory
};

struct Square {
    Piece piece;
    int loc;

    Square() : loc(0) {}
    Square(int location) : loc(location) {}
};

struct Position {
    std::array<Square, 64> squares;
    Bitboard whitePawns = EMPTY;
    Bitboard whiteKnights = EMPTY;
    Bitboard whiteBishops = EMPTY;
    Bitboard whiteRooks = EMPTY;
    Bitboard whiteQueens = EMPTY;
    Bitboard whiteKings = EMPTY;
    Bitboard blackPawns = EMPTY;
    Bitboard blackKnights = EMPTY;
    Bitboard blackBishops = EMPTY;
    Bitboard blackRooks = EMPTY;
    Bitboard blackQueens = EMPTY;
    Bitboard blackKings = EMPTY;
    Bitboard whitePieces = EMPTY;
    Bitboard blackPieces = EMPTY;
    Bitboard allPieces = EMPTY;
};

struct StateInfo {
    ChessPieceColor turn = ChessPieceColor::WHITE;
    std::uint8_t castlingRights = CastlingConstants::kAllCastlingRightsMask;
    chess::EnPassantSquare enPassantSquare = chess::noEnPassantSquare();
    int halfmoveClock = 0;
    int fullmoveNumber = 1;
    bool whiteChecked = false;
    bool blackChecked = false;
    int lastMove = 0;
    ChessTimePoint lastMoveTime = ChessClock::now();
};

#define BOARD_REF_MEMBERS_(pos, st)                                                                \
    squares((pos).squares), turn((st).turn), enPassantSquare((st).enPassantSquare),                \
        halfmoveClock((st).halfmoveClock), fullmoveNumber((st).fullmoveNumber),                    \
        whiteChecked((st).whiteChecked), blackChecked((st).blackChecked), LastMove((st).lastMove), \
        whitePawns((pos).whitePawns), whiteKnights((pos).whiteKnights),                            \
        whiteBishops((pos).whiteBishops), whiteRooks((pos).whiteRooks),                            \
        whiteQueens((pos).whiteQueens), whiteKings((pos).whiteKings),                              \
        blackPawns((pos).blackPawns), blackKnights((pos).blackKnights),                            \
        blackBishops((pos).blackBishops), blackRooks((pos).blackRooks),                            \
        blackQueens((pos).blackQueens), blackKings((pos).blackKings),                              \
        whitePieces((pos).whitePieces), blackPieces((pos).blackPieces),                            \
        allPieces((pos).allPieces), lastMoveTime((st).lastMoveTime),                               \
        whiteCanCastle((st), CastlingConstants::kWhiteCastlingRightsMask),                         \
        blackCanCastle((st), CastlingConstants::kBlackCastlingRightsMask)

struct Board {
    class CastlingSideProxy {
    public:
        CastlingSideProxy(StateInfo& stateRef, std::uint8_t maskRef)
            : state(&stateRef), mask(maskRef) {}

        CastlingSideProxy(const CastlingSideProxy&) = default;
        CastlingSideProxy& operator=(const CastlingSideProxy&) = default;

        CastlingSideProxy& operator=(bool enabled) {
            if (enabled) {
                state->castlingRights |= mask;
            } else {
                state->castlingRights &= static_cast<std::uint8_t>(~mask);
            }
            return *this;
        }

        operator bool() const {
            return (state->castlingRights & mask) != 0;
        }

    private:
        StateInfo* state;
        std::uint8_t mask;
    };

    Position position;
    StateInfo state;

    // Transitional flattened members backed by Position/StateInfo.
    std::array<Square, 64>& squares;
    ChessPieceColor& turn;
    chess::EnPassantSquare& enPassantSquare;
    int& halfmoveClock;
    int& fullmoveNumber;
    bool& whiteChecked;
    bool& blackChecked;
    int& LastMove;
    Bitboard& whitePawns;
    Bitboard& whiteKnights;
    Bitboard& whiteBishops;
    Bitboard& whiteRooks;
    Bitboard& whiteQueens;
    Bitboard& whiteKings;
    Bitboard& blackPawns;
    Bitboard& blackKnights;
    Bitboard& blackBishops;
    Bitboard& blackRooks;
    Bitboard& blackQueens;
    Bitboard& blackKings;
    Bitboard& whitePieces;
    Bitboard& blackPieces;
    Bitboard& allPieces;
    ChessTimePoint& lastMoveTime;
    CastlingSideProxy whiteCanCastle;
    CastlingSideProxy blackCanCastle;

    Board() : BOARD_REF_MEMBERS_(position, state) {
        for (int i = 0; i < 64; ++i) {
            squares[i] = Square(i);
        }
    }

    Board(const Board& other)
        : position(other.position), state(other.state), BOARD_REF_MEMBERS_(position, state) {}

    Board& operator=(const Board& other) {
        if (this == &other) {
            return *this;
        }
        position = other.position;
        state = other.state;
        return *this;
    }

    Board(Board&& other) noexcept
        : position(other.position), state(other.state), BOARD_REF_MEMBERS_(position, state) {}

    Board& operator=(Board&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        position = other.position;
        state = other.state;
        return *this;
    }

    ChessDuration getTimeSinceLastMove() const {
        return std::chrono::duration_cast<ChessDuration>(ChessClock::now() - lastMoveTime);
    }

    std::uint8_t castlingRights() const {
        return state.castlingRights;
    }

    bool hasCastlingRight(std::uint8_t mask) const {
        return (state.castlingRights & mask) != 0;
    }

    void setCastlingRight(std::uint8_t mask, bool enabled = true) {
        const std::uint8_t updatedRights =
            enabled ? static_cast<std::uint8_t>(state.castlingRights | mask)
                    : static_cast<std::uint8_t>(state.castlingRights &
                                                static_cast<std::uint8_t>(~mask));
        state.castlingRights = updatedRights;
    }

    void clearCastlingRights(std::uint8_t mask) {
        state.castlingRights &= static_cast<std::uint8_t>(~mask);
    }

    std::string toFEN() const;
    std::expected<void, ChessError> fromFEN(ChessString fen);
    void InitializeFromFEN(ChessString fen);
    bool movePiece(int from, int to);
    void clearBitboards();
    void updateBitboards();
    void updateOccupancy();
    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const;

    void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }
};
