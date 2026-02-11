#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

using ChessString = std::string_view;

using ChessClock = std::chrono::steady_clock;
using ChessDuration = std::chrono::milliseconds;
using ChessTimePoint = ChessClock::time_point;

enum class ChessError {
    InvalidMove,
    NoPieceAtSource,
    WrongTurn,
    MoveLeavesKingInCheck,
    InvalidPosition,
    InvalidFEN,
    Timeout,
    OutOfMemory
};

struct Move {
    int from;
    int to;
    std::string notation;

    Move(int f, int t, std::string_view n = "") : from(f), to(t), notation(n) {}

    bool operator==(const Move& other) const {
        return from == other.from && to == other.to;
    }

    bool operator<(const Move& other) const {
        if (from != other.from)
            return from < other.from;
        return to < other.to;
    }
};

struct Position {
    int row;
    int col;

    Position(int r, int c) : row(r), col(c) {}

    bool isValid() const {
        return row >= 0 && row < 8 && col >= 0 && col < 8;
    }

    int toIndex() const {
        return row * 8 + col;
    }

    static Position fromIndex(int index) {
        return Position(index / 8, index % 8);
    }

    bool operator==(const Position& other) const {
        return row == other.row && col == other.col;
    }
};

struct ModernPiece {
    int type;
    int color;
    Position pos;
    std::vector<Position> validMoves;

    ModernPiece(int t, int c, Position p) : type(t), color(c), pos(p) {}

    bool isValid() const {
        return pos.isValid() && type >= 0 && color >= 0;
    }
};

class ModernBoard {
private:
    std::array<ModernPiece*, 64> pieces;
    int currentTurn;
    ChessTimePoint lastMoveTime;

public:
    ModernBoard() : currentTurn(0), lastMoveTime(ChessClock::now()) {
        std::fill(pieces.begin(), pieces.end(), nullptr);
    }

    ~ModernBoard() {
        for (auto piece : pieces) {
            delete piece;
        }
    }

    template <typename T>
    void setPiece(T index, ModernPiece* piece) {
        if (index >= 0 && index < 64) {
            if (pieces[index]) {
                delete pieces[index];
            }
            pieces[index] = piece;
        }
    }

    template <typename T>
    ModernPiece* getPiece(T index) const {
        if (index >= 0 && index < 64) {
            return pieces[index];
        }
        return nullptr;
    }

    std::vector<std::pair<int, ModernPiece*>> getPieces() const {
        std::vector<std::pair<int, ModernPiece*>> result;
        for (int i = 0; i < 64; ++i) {
            if (pieces[i]) {
                result.emplace_back(i, pieces[i]);
            }
        }
        return result;
    }

    std::vector<std::pair<int, ModernPiece*>> getPiecesOfType(int type) const {
        std::vector<std::pair<int, ModernPiece*>> result;
        auto allPieces = getPieces();
        std::copy_if(allPieces.begin(), allPieces.end(), std::back_inserter(result),
                     [type](const auto& pair) { return pair.second->type == type; });
        return result;
    }

    std::vector<std::pair<int, ModernPiece*>> getPiecesOfColor(int color) const {
        std::vector<std::pair<int, ModernPiece*>> result;
        auto allPieces = getPieces();
        std::copy_if(allPieces.begin(), allPieces.end(), std::back_inserter(result),
                     [color](const auto& pair) { return pair.second->color == color; });
        return result;
    }

    bool makeMove(Move move) {
        auto fromPiece = getPiece(move.from);
        if (!fromPiece) {
            return false;
        }

        if (fromPiece->color != currentTurn) {
            return false;
        }

        if (pieces[move.to]) {
            delete pieces[move.to];
        }
        pieces[move.to] = fromPiece;
        pieces[move.to]->pos = Position::fromIndex(move.to);
        pieces[move.from] = nullptr;

        currentTurn = 1 - currentTurn;
        lastMoveTime = ChessClock::now();

        return true;
    }

    ChessDuration getTimeSinceLastMove() const {
        return std::chrono::duration_cast<ChessDuration>(ChessClock::now() - lastMoveTime);
    }

    std::string toString() const {
        std::string result = "Modern Board:\n";
        for (int row = 7; row >= 0; --row) {
            result += std::to_string(row + 1) + " ";
            for (int col = 0; col < 8; ++col) {
                int index = row * 8 + col;
                if (auto piece = getPiece(index)) {
                    result += std::to_string(piece->type) + " ";
                } else {
                    result += ". ";
                }
            }
            result += std::to_string(row + 1) + "\n";
        }
        result += "  a b c d e f g h\n";
        result += "Turn: " + std::to_string(currentTurn) + "\n";
        return result;
    }

    std::vector<Move> generateMoves() const;

    double evaluate() const;

    Move findBestMove(ChessDuration timeLimit) const;
};

namespace ModernChessUtils {

bool isValidPosition(int row, int col) {
    return row >= 0 && row < 8 && col >= 0 && col < 8;
}

int positionToIndex(int row, int col) {
    return row * 8 + col;
}

std::pair<int, int> indexToPosition(int index) {
    return {index / 8, index % 8};
}

template <typename T>
bool isValidIndex(T index) {
    return index >= 0 && index < 64;
}

template <typename Container>
std::vector<Move> filterValidMoves(const Container& moves) {
    std::vector<Move> result;
    std::copy_if(moves.begin(), moves.end(), std::back_inserter(result),
                 [](const Move& move) { return isValidIndex(move.from) && isValidIndex(move.to); });
    return result;
}

template <typename Container>
std::vector<Move> sortMoves(Container&& moves) {
    std::vector<Move> result(moves.begin(), moves.end());
    std::sort(result.begin(), result.end());
    return result;
}

std::string formatMove(const Move& move) {
    return std::string("Move from ") + std::to_string(move.from) + " to " + std::to_string(move.to);
}

std::string formatError(ChessError error) {
    switch (error) {
        case ChessError::InvalidMove:
            return "Invalid move";
        case ChessError::NoPieceAtSource:
            return "No piece at source";
        case ChessError::WrongTurn:
            return "Wrong turn";
        case ChessError::MoveLeavesKingInCheck:
            return "Move leaves king in check";
        case ChessError::InvalidPosition:
            return "Invalid position";
        case ChessError::InvalidFEN:
            return "Invalid FEN";
        case ChessError::Timeout:
            return "Timeout";
        case ChessError::OutOfMemory:
            return "Out of memory";
        default:
            return "Unknown error";
    }
}
} // namespace ModernChessUtils