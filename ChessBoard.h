#ifndef CHESSBOARD_H
#define CHESSBOARD_H

#include "ChessPiece.h"
#include "Bitboard.h"
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <algorithm>
#include <memory>
#include <functional>

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

struct Square {
    Piece Piece;
    int loc;
    std::vector<int> ValidMoves;
    
    Square() : loc(0) {}
    Square(int location) : loc(location) {}
    
    std::string toString() const {
        if (Piece.PieceType == ChessPieceType::NONE) {
            return ".";
        }
        std::string result;
        result += (Piece.PieceColor == ChessPieceColor::WHITE ? "W" : "B");
        result += std::to_string(static_cast<int>(Piece.PieceType));
        return result;
    }
};

struct Board {
    std::array<Square, 64> squares;
    ChessPieceColor turn;
    bool whiteCanCastle;
    bool blackCanCastle;
    bool whiteChecked;
    bool blackChecked;
    int LastMove;
    
    Bitboard whitePawns, whiteKnights, whiteBishops, whiteRooks, whiteQueens, whiteKings;
    Bitboard blackPawns, blackKnights, blackBishops, blackRooks, blackQueens, blackKings;
    Bitboard whitePieces, blackPieces, allPieces;
    
    ChessTimePoint lastMoveTime;
    
    Board() : turn(ChessPieceColor::WHITE), whiteCanCastle(true), blackCanCastle(true), 
              whiteChecked(false), blackChecked(false), LastMove(0), 
              lastMoveTime(ChessClock::now()) {
        for(int i = 0; i < 64; i++){
            squares[i] = Square(i);
        }
    }
    
    template<typename T>
    bool isValidIndex(T index) const {
        return index >= 0 && index < 64;
    }
    
    template<typename T>
    ChessPieceColor getPieceColor(T pos) const {
        if (!isValidIndex(pos)) return ChessPieceColor::WHITE;
        return squares[pos].Piece.PieceColor;
    }
    
    template<typename T>
    ChessPieceType getPieceType(T pos) const {
        if (!isValidIndex(pos)) return ChessPieceType::NONE;
        return squares[pos].Piece.PieceType;
    }
    
    std::vector<int> getPiecesOfType(ChessPieceType type) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (squares[i].Piece.PieceType == type) {
                result.push_back(i);
            }
        }
        return result;
    }
    
    std::vector<int> getPiecesOfColor(ChessPieceColor color) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (squares[i].Piece.PieceColor == color) {
                result.push_back(i);
            }
        }
        return result;
    }
    
    std::string toString() const {
        std::string result = "Board:\n";
        for (int row = 7; row >= 0; --row) {
            result += std::to_string(row + 1) + " ";
            for (int col = 0; col < 8; ++col) {
                result += squares[row * 8 + col].toString() + " ";
            }
            result += std::to_string(row + 1) + "\n";
        }
        result += "  a b c d e f g h\n";
        result += "Turn: " + std::string(turn == ChessPieceColor::WHITE ? "White" : "Black") + "\n";
        return result;
    }
    
    ChessTimePoint getCurrentTime() const {
        return ChessClock::now();
    }
    
    ChessDuration getTimeSinceLastMove() const {
        return std::chrono::duration_cast<ChessDuration>(
            ChessClock::now() - lastMoveTime);
    }
    
    std::string toFEN() const;
    bool fromFEN(ChessString fen);
    
    void InitializeFromFEN(ChessString fen);
    
    bool movePiece(int from, int to);
    
    void clearBitboards();
    void updateBitboards();
    void updateOccupancy();
    Bitboard getPieceBitboard(ChessPieceType type, ChessPieceColor color) const;
    
    template<typename Func>
    std::vector<int> generateMovesForPiece(int pos, Func&& filter) const {
        std::vector<int> result;
        if (isValidIndex(pos)) {
            auto moves = squares[pos].ValidMoves;
            std::copy_if(moves.begin(), moves.end(), std::back_inserter(result), filter);
        }
        return result;
    }
    
    template<typename Func>
    void forEachPiece(Func&& func) const {
        for (int i = 0; i < 64; ++i) {
            if (squares[i].Piece.PieceType != ChessPieceType::NONE) {
                func(i, squares[i].Piece);
            }
        }
    }
    
    template<typename Func>
    std::vector<int> filterPositions(Func&& predicate) const {
        std::vector<int> result;
        for (int i = 0; i < 64; ++i) {
            if (predicate(i, squares[i].Piece)) {
                result.push_back(i);
            }
        }
        return result;
    }
    
    ChessError validateMove(int from, int to) const;
    
    void recordMoveTime() {
        lastMoveTime = ChessClock::now();
    }
};

namespace ChessUtils {
    
    inline bool isValidPosition(int row, int col) {
        return row >= 0 && row < 8 && col >= 0 && col < 8;
    }
    
    inline int positionToIndex(int row, int col) {
        return row * 8 + col;
    }
    
    inline std::pair<int, int> indexToPosition(int index) {
        return {index / 8, index % 8};
    }
    
    template<typename Container>
    inline std::vector<int> filterValidMoves(const Container& moves) {
        std::vector<int> result;
        std::copy_if(moves.begin(), moves.end(), std::back_inserter(result),
                    [](int move) { return move >= 0 && move < 64; });
        return result;
    }
    
    inline std::string formatMove(int from, int to) {
        return std::string("Move from ") + std::to_string(from) + 
               " to " + std::to_string(to);
    }
    
    inline std::string formatError(ChessError error) {
        switch (error) {
            case ChessError::InvalidMove: return "Invalid move";
            case ChessError::NoPieceAtSource: return "No piece at source position";
            case ChessError::WrongTurn: return "Wrong player's turn";
            case ChessError::MoveLeavesKingInCheck: return "Move leaves king in check";
            case ChessError::InvalidPosition: return "Invalid position";
            case ChessError::InvalidFEN: return "Invalid FEN string";
            case ChessError::Timeout: return "Operation timed out";
            case ChessError::OutOfMemory: return "Out of memory";
            default: return "Unknown error";
        }
    }
}

#endif