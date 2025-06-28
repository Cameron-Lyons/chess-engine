#ifndef ENGINE_GLOBALS_H
#define ENGINE_GLOBALS_H

#include "ChessBoard.h"
#include "search.h"
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cstdint>

extern std::unordered_map<std::string, std::vector<std::string>> OpeningBookOptions;
extern std::unordered_map<std::string, std::string> OpeningBook;
extern uint64_t ZobristTable[64][12];
extern uint64_t ZobristBlackToMove;
extern ThreadSafeTT TransTable;

std::string getFEN(const Board& board);
bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol, int& destRow);

ChessPieceType getPromotionPiece(std::string_view move);

#endif // ENGINE_GLOBALS_H 