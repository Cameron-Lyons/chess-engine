#pragma once

#include "../core/ChessBoard.h"
#include "../core/MoveContent.h"
#include <string>

namespace EnhancedSearch {

int iterativeDeepening(Board& board, int maxDepth, int maxTime, MoveContent& bestMove);

int alphaBeta(Board& board, int depth, int alpha, int beta, bool pvNode, bool cutNode,
              MoveContent* prevMove, bool doNull);

int quiescence(Board& board, int alpha, int beta, int depth);

bool isTimeUp();

std::string moveToString(const MoveContent& move);

int getPieceValue(ChessPieceType piece);

ChessPieceColor oppositeColor(ChessPieceColor color);
} 