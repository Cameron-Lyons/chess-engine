#include "BitboardOnly.h"
#include "ChessBoard.h"

BitboardPosition::BitboardPosition() : sideToMove(WHITE) {
    for (auto& color : pieces) {
        for (auto& piece : color) {
            piece = 0;
        }
    }

    for (auto& occ : occupancy) {
        occ = 0;
    }
}

BitboardPosition BitboardPosition::fromBoard(const Board& board) {
    BitboardPosition pos;

    pos.pieces[WHITE][PAWN] = board.whitePawns;
    pos.pieces[WHITE][KNIGHT] = board.whiteKnights;
    pos.pieces[WHITE][BISHOP] = board.whiteBishops;
    pos.pieces[WHITE][ROOK] = board.whiteRooks;
    pos.pieces[WHITE][QUEEN] = board.whiteQueens;
    pos.pieces[WHITE][KING] = board.whiteKings;
    pos.pieces[BLACK][PAWN] = board.blackPawns;
    pos.pieces[BLACK][KNIGHT] = board.blackKnights;
    pos.pieces[BLACK][BISHOP] = board.blackBishops;
    pos.pieces[BLACK][ROOK] = board.blackRooks;
    pos.pieces[BLACK][QUEEN] = board.blackQueens;
    pos.pieces[BLACK][KING] = board.blackKings;

    pos.occupancy[WHITE] = board.whitePieces;
    pos.occupancy[BLACK] = board.blackPieces;
    pos.occupancy[2] = board.allPieces;

    pos.sideToMove = (board.turn == ChessPieceColor::WHITE) ? WHITE : BLACK;

    return pos;
}
