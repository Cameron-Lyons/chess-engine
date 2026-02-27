#pragma once

#include <string>
#include <unordered_map>
#include <vector>

std::string normalizeBookFen(const std::string& fen, bool clearEnPassant);

std::string lookupBookMoveString(
    const std::string& fen,
    const std::unordered_map<std::string, std::vector<std::string>>& openingBookOptions,
    const std::unordered_map<std::string, std::string>& openingBook, bool randomize);
