#pragma once

#include "../core/ChessBoard.h"
#include "../search/search.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern std::unordered_map<std::string, std::vector<std::string>> OpeningBookOptions;
extern std::unordered_map<std::string, std::string> OpeningBook;

ChessPieceType getPromotionPiece(std::string_view move);
