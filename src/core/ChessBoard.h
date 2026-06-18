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
    SquareIndex loc;

    Square() : loc(0) {}
    explicit Square(SquareIndex location) : loc(location) {}
    explicit Square(int location) : loc(location) {}
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
    SquareIndex lastMove;
    ChessTimePoint lastMoveTime = ChessClock::now();
};

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

        explicit operator bool() const {
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
    SquareIndex& LastMove;
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

private:
    Board(Position pos, StateInfo st)
        : position(pos), state(st), squares(position.squares), turn(state.turn),
          enPassantSquare(state.enPassantSquare), halfmoveClock(state.halfmoveClock),
          fullmoveNumber(state.fullmoveNumber), whiteChecked(state.whiteChecked),
          blackChecked(state.blackChecked), LastMove(state.lastMove),
          whitePawns(position.whitePawns), whiteKnights(position.whiteKnights),
          whiteBishops(position.whiteBishops), whiteRooks(position.whiteRooks),
          whiteQueens(position.whiteQueens), whiteKings(position.whiteKings),
          blackPawns(position.blackPawns), blackKnights(position.blackKnights),
          blackBishops(position.blackBishops), blackRooks(position.blackRooks),
          blackQueens(position.blackQueens), blackKings(position.blackKings),
          whitePieces(position.whitePieces), blackPieces(position.blackPieces),
          allPieces(position.allPieces), lastMoveTime(state.lastMoveTime),
          whiteCanCastle(state, CastlingConstants::kWhiteCastlingRightsMask),
          blackCanCastle(state, CastlingConstants::kBlackCastlingRightsMask) {}

public:
    Board() : Board(Position{}, StateInfo{}) {
        for (int i = 0; i < 64; ++i) {
            squares[i] = Square(i);
        }
    }

    Board(const Board& other) : Board(other.position, other.state) {}

    Board& operator=(const Board& other) {
        if (this == &other) {
            return *this;
        }
        position = other.position;
        state = other.state;
        return *this;
    }

    Board(Board&& other) noexcept : Board(other.position, other.state) {}

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
    bool movePiece(SquareIndex from, SquareIndex to);
    bool movePiece(int from, int to) {
        return movePiece(SquareIndex(from), SquareIndex(to));
    }
    void clearBitboards();
    void updateBitboards();
    void updateOccupancy();
    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const;

    void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }
};
