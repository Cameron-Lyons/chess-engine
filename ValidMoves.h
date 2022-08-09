#include "PieceMoves.h"
#include "ChessBoard.h"

static bool BlackAttackBoard[64];
static bool WhiteAttackBoard[64];

static int BlackKingPosition;
static int WhiteKingPosition;

static bool AnalyzeMove(Board board, int dest, Piece piece){
    // pawns cannot attack where they move
    if (piece.PieceColor == WHITE) {
        WhiteAttackBoard[dest] = true;
    } else {
        BlackAttackBoard[dest] = true;
    }

    if (board.squares[dest].Piece.PieceType == NONE) {
        piece.ValidMoves.push_back(dest);
        return true;
    }
}