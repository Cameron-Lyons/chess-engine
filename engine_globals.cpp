#include "engine_globals.h"
#include <sstream>

std::map<std::string, std::string> OpeningBook = {
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e2e4"},
    {"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1", "e7e5"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2", "g1f3"},
    {"rnbqkbnr/pppp1ppp/8/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R b KQkq - 1 2", "b8c6"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", "f1c4"},
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", "f8c5"},
};

uint64_t ZobristTable[64][12];
uint64_t ZobristBlackToMove;
ThreadSafeTT TransTable;

std::string getFEN(const Board& board) {
    std::string fen;
    for (int row = 7; row >= 0; --row) {
        int emptyCount = 0;
        for (int col = 0; col < 8; ++col) {
            int pos = row * 8 + col;
            const Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::NONE) {
                emptyCount++;
            } else {
                if (emptyCount > 0) {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                char symbol = '.';
                switch (piece.PieceType) {
                    case ChessPieceType::PAWN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'P' : 'p'; break;
                    case ChessPieceType::KNIGHT: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'N' : 'n'; break;
                    case ChessPieceType::BISHOP: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'B' : 'b'; break;
                    case ChessPieceType::ROOK: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'R' : 'r'; break;
                    case ChessPieceType::QUEEN: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'Q' : 'q'; break;
                    case ChessPieceType::KING: symbol = piece.PieceColor == ChessPieceColor::WHITE ? 'K' : 'k'; break;
                    default: symbol = '.'; break;
                }
                fen += symbol;
            }
        }
        if (emptyCount > 0) {
            fen += std::to_string(emptyCount);
        }
        if (row > 0) fen += '/';
    }
    fen += board.turn == ChessPieceColor::WHITE ? " w" : " b";
    fen += " KQkq - 0 1";
    return fen;
}

bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol, int& destRow) {
    std::string cleanMove(move);
    if (!cleanMove.empty() && (cleanMove.back() == '+' || cleanMove.back() == '#')) {
        cleanMove.pop_back();
    }
    if (cleanMove == "O-O" || cleanMove == "0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 6; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 6; destRow = 7;
        }
        return true;
    }
    if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
        if (board.turn == ChessPieceColor::WHITE) {
            srcCol = 4; srcRow = 0; destCol = 2; destRow = 0;
        } else {
            srcCol = 4; srcRow = 7; destCol = 2; destRow = 7;
        }
        return true;
    }
    ChessPieceType pieceType = ChessPieceType::PAWN;
    size_t startPos = 0;
    int disambigCol = -1, disambigRow = -1;
    if (cleanMove.length() > 0) {
        switch (cleanMove[0]) {
            case 'N': pieceType = ChessPieceType::KNIGHT; startPos = 1; break;
            case 'B': pieceType = ChessPieceType::BISHOP; startPos = 1; break;
            case 'R': pieceType = ChessPieceType::ROOK; startPos = 1; break;
            case 'Q': pieceType = ChessPieceType::QUEEN; startPos = 1; break;
            case 'K': pieceType = ChessPieceType::KING; startPos = 1; break;
            default: pieceType = ChessPieceType::PAWN; startPos = 0; break;
        }
    }
    if (pieceType == ChessPieceType::PAWN && cleanMove.length() >= 4 && cleanMove[1] == 'x') {
        destCol = cleanMove[2] - 'a';
        destRow = cleanMove[3] - '1';
        srcCol = cleanMove[0] - 'a';
        if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8 || srcCol < 0 || srcCol >= 8) return false;
        for (int row = 0; row < 8; row++) {
            int pos = row * 8 + srcCol;
            Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                int from = pos;
                int to = destRow * 8 + destCol;
                if (board.turn == ChessPieceColor::WHITE) {
                    if ((to == from + 7 && srcCol > 0) || (to == from + 9 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::BLACK) {
                            srcRow = row;
                            return true;
                        }
                    }
                } else {
                    if ((to == from - 9 && srcCol > 0) || (to == from - 7 && srcCol < 7)) {
                        if (board.squares[to].Piece.PieceType != ChessPieceType::NONE &&
                            board.squares[to].Piece.PieceColor == ChessPieceColor::WHITE) {
                            srcRow = row;
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    if (cleanMove.length() >= 4 && pieceType != ChessPieceType::PAWN) {
        char secondChar = cleanMove[1];
        if (secondChar >= 'a' && secondChar <= 'h') {
            disambigCol = secondChar - 'a';
            startPos = 2;
        } else if (secondChar >= '1' && secondChar <= '8') {
            disambigRow = secondChar - '1';
            startPos = 2;
        }
    }
    if (cleanMove.length() < startPos + 2) return false;
    if (cleanMove[startPos] == 'x') {
        destCol = cleanMove[startPos + 1] - 'a';
        destRow = cleanMove[startPos + 2] - '1';
    } else {
        destCol = cleanMove[startPos] - 'a';
        destRow = cleanMove[startPos + 1] - '1';
    }
    if (destCol < 0 || destCol >= 8 || destRow < 0 || destRow >= 8) return false;
    bool found = false;
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int pos = row * 8 + col;
            Piece& piece = board.squares[pos].Piece;
            if (piece.PieceType == pieceType && piece.PieceColor == board.turn) {
                if (disambigCol != -1 && col != disambigCol) continue;
                if (disambigRow != -1 && row != disambigRow) continue;
                for (int validDest : piece.ValidMoves) {
                    if (validDest == destRow * 8 + destCol) {
                        srcCol = col;
                        srcRow = row;
                        found = true;
                        break;
                    }
                }
            }
            if (found) break;
        }
        if (found) break;
    }
    if (found) return true;
    if (cleanMove.length() == 2 && pieceType == ChessPieceType::PAWN) {
        destCol = cleanMove[0] - 'a';
        destRow = cleanMove[1] - '1';
        if (destCol >= 0 && destCol < 8 && destRow >= 0 && destRow < 8) {
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int pos = row * 8 + col;
                    Piece& piece = board.squares[pos].Piece;
                    if (piece.PieceType == ChessPieceType::PAWN && piece.PieceColor == board.turn) {
                        int pawnRow = pos / 8;
                        int pawnCol = pos % 8;
                        if (board.turn == ChessPieceColor::WHITE) {
                            if (pawnCol == destCol && destRow > pawnRow) {
                                bool pathClear = true;
                                for (int r = pawnRow + 1; r <= destRow; r++) {
                                    if (board.squares[r * 8 + pawnCol].Piece.PieceType != ChessPieceType::NONE) {
                                        pathClear = false;
                                        break;
                                    }
                                }
                                if (pathClear && (destRow == pawnRow + 1 || (destRow == pawnRow + 2 && pawnRow == 1))) {
                                    srcCol = pawnCol;
                                    srcRow = pawnRow;
                                    return true;
                                }
                            }
                        } else {
                            if (pawnCol == destCol && destRow < pawnRow) {
                                bool pathClear = true;
                                for (int r = pawnRow - 1; r >= destRow; r--) {
                                    if (board.squares[r * 8 + pawnCol].Piece.PieceType != ChessPieceType::NONE) {
                                        pathClear = false;
                                        break;
                                    }
                                }
                                if (pathClear && (destRow == pawnRow - 1 || (destRow == pawnRow - 2 && pawnRow == 6))) {
                                    srcCol = pawnCol;
                                    srcRow = pawnRow;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
} 