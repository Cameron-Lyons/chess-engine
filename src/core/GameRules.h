#pragma once

#include "ChessBoard.h"

#include <cstdint>

class Engine;

enum class GameState : std::uint8_t {
    ONGOING,
    CHECKMATE_WHITE_WINS,
    CHECKMATE_BLACK_WINS,
    STALEMATE,
    DRAW_INSUFFICIENT_MATERIAL,
    DRAW_THREEFOLD_REPETITION,
    DRAW_FIFTY_MOVE_RULE,
};

bool hasAnyLegalMoves(Board& board, ChessPieceColor sideToMove);
GameState checkGameState(Board& board);
GameState checkGameState(const Engine& engine);
void announceGameResult(GameState state);
