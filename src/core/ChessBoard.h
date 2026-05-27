#pragma once

#include "Bitboard.h"
#include "CastlingConstants.h"
#include "ChessPiece.h"
#include "Move.h"
#include "SquareSentinel.h"

#include "../utils/Concepts.h"
#include "../utils/ChessFormat.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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

    std::string toString() const {
        return chess::format::pieceLabel(piece);
    }
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

#define BOARD_REF_MEMBERS_(pos, st)                                                                  \
    squares((pos).squares), turn((st).turn), enPassantSquare((st).enPassantSquare),                  \
          halfmoveClock((st).halfmoveClock), fullmoveNumber((st).fullmoveNumber),                    \
          whiteChecked((st).whiteChecked), blackChecked((st).blackChecked),                          \
          LastMove((st).lastMove), whitePawns((pos).whitePawns),                                   \
          whiteKnights((pos).whiteKnights), whiteBishops((pos).whiteBishops),                        \
          whiteRooks((pos).whiteRooks), whiteQueens((pos).whiteQueens),                             \
          whiteKings((pos).whiteKings), blackPawns((pos).blackPawns),                               \
          blackKnights((pos).blackKnights), blackBishops((pos).blackBishops),                        \
          blackRooks((pos).blackRooks), blackQueens((pos).blackQueens),                              \
          blackKings((pos).blackKings), whitePieces((pos).whitePieces),                             \
          blackPieces((pos).blackPieces), allPieces((pos).allPieces),                                \
          lastMoveTime((st).lastMoveTime),                                                           \
          whiteCanCastle((st), CastlingConstants::kWhiteCastlingRightsMask),                        \
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

    Board()
        : BOARD_REF_MEMBERS_(position, state) {
        for (int i = 0; i < 64; ++i) {
            squares[i] = Square(i);
        }
    }

    Board(const Board& other) : position(other.position), state(other.state), BOARD_REF_MEMBERS_(position, state) {}

    Board& operator=(const Board& other) {
        if (this == &other) {
            return *this;
        }
        position = other.position;
        state = other.state;
        return *this;
    }

    Board(Board&& other) noexcept
        : position(std::move(other.position)), state(std::move(other.state)),
          BOARD_REF_MEMBERS_(position, state) {}

    Board& operator=(Board&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        position = std::move(other.position);
        state = std::move(other.state);
        return *this;
    }

    template <std::integral T>
    bool isValidIndex(T index) const {
        if constexpr (std::is_signed_v<T>) {
            return index >= 0 && index < static_cast<T>(NUM_SQUARES);
        }
        return index < static_cast<T>(NUM_SQUARES);
    }

    template <std::integral T>
    ChessPieceColor getPieceColor(T pos) const {
        if (!isValidIndex(pos)) {
            return ChessPieceColor::WHITE;
        }
        return squares[pos].piece.PieceColor;
    }

    template <std::integral T>
    ChessPieceType getPieceType(T pos) const {
        if (!isValidIndex(pos)) {
            return ChessPieceType::NONE;
        }
        return squares[pos].piece.PieceType;
    }

    std::vector<int> getPiecesOfType(ChessPieceType type) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (squares[i].piece.PieceType == type) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::vector<int> getPiecesOfColor(ChessPieceColor color) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (squares[i].piece.PieceColor == color) {
                result.push_back(i);
            }
        }
        return result;
    }

    std::string toString() const {
        std::string result = "Board:\n";
        for (int row = 7; row >= 0; --row) {
            result += std::format("{} ", row + 1);
            for (int col = 0; col < 8; ++col) {
                result += std::format("{} ", squares[(row * 8) + col].toString());
            }
            result += std::format("{}\n", row + 1);
        }
        result += std::format("  a b c d e f g h\nTurn: {}\n",
                              turn == ChessPieceColor::WHITE ? "White" : "Black");
        return result;
    }

    ChessTimePoint getCurrentTime() const {
        return ChessClock::now();
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

    bool canCastleAny(ChessPieceColor color) const {
        const std::uint8_t mask = (color == ChessPieceColor::WHITE)
                                      ? CastlingConstants::kWhiteCastlingRightsMask
                                      : CastlingConstants::kBlackCastlingRightsMask;
        return hasCastlingRight(mask);
    }

    bool canCastleKingside(ChessPieceColor color) const {
        const std::uint8_t mask = (color == ChessPieceColor::WHITE)
                                      ? CastlingConstants::kWhiteKingsideCastlingRight
                                      : CastlingConstants::kBlackKingsideCastlingRight;
        return hasCastlingRight(mask);
    }

    bool canCastleQueenside(ChessPieceColor color) const {
        const std::uint8_t mask = (color == ChessPieceColor::WHITE)
                                      ? CastlingConstants::kWhiteQueensideCastlingRight
                                      : CastlingConstants::kBlackQueensideCastlingRight;
        return hasCastlingRight(mask);
    }

    std::string toFEN() const;
    std::expected<void, ChessError> fromFEN(ChessString fen);
    void InitializeFromFEN(ChessString fen);
    bool movePiece(int from, int to);
    void clearBitboards();
    void updateBitboards();
    void updateOccupancy();
    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const;

    template <chess::SquareCallback Func>
    void forEachPiece(Func&& func) const {
        for (int i = 0; i < 64; ++i) {
            if (squares[i].piece.PieceType != ChessPieceType::NONE) {
                func(i, squares[i].piece);
            }
        }
    }

    template <chess::SquarePredicate Func>
    std::vector<int> filterPositions(Func&& predicate) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (predicate(i, squares[i].piece)) {
                result.push_back(i);
            }
        }
        return result;
    }

    void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }
};

namespace ChessUtils {

inline bool isValidPosition(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

inline int positionToIndex(int row, int col) {
    return (row * 8) + col;
}

inline std::pair<int, int> indexToPosition(int index) {
    return {index / 8, index % 8};
}

template <chess::SquareIndexContainer Container>
inline std::vector<int> filterValidMoves(const Container& moves) {
    std::vector<int> result;
    std::copy_if(moves.begin(), moves.end(), std::back_inserter(result),
                 [](int move) { return move >= 0 && move < 64; });
    return result;
}

inline std::string formatMove(int from, int to) {
    return chess::format::debugMove(from, to);
}

inline std::string formatError(ChessError error) {
    switch (error) {
        case ChessError::None:
            return "No error";
        case ChessError::InvalidMove:
            return "Invalid move";
        case ChessError::NoPieceAtSource:
            return "No piece at source position";
        case ChessError::WrongTurn:
            return "Wrong player's turn";
        case ChessError::MoveLeavesKingInCheck:
            return "Move leaves king in check";
        case ChessError::InvalidPosition:
            return "Invalid position";
        case ChessError::InvalidFEN:
            return "Invalid FEN string";
        case ChessError::Timeout:
            return "Operation timed out";
        case ChessError::OutOfMemory:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}
} // namespace ChessUtils
