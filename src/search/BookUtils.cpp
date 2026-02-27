#include "BookUtils.h"

#include <random>
#include <sstream>

namespace {
constexpr int kZero = 0;
constexpr int kOne = 1;
}

std::string normalizeBookFen(const std::string& fen, bool clearEnPassant) {
    std::istringstream iss(fen);
    std::string piecePlacement;
    std::string sideToMove;
    std::string castling;
    std::string enPassant;
    std::string halfmoveClock;
    std::string fullmoveNumber;

    if (!(iss >> piecePlacement >> sideToMove >> castling >> enPassant)) {
        return fen;
    }

    if (!(iss >> halfmoveClock)) {
        halfmoveClock = "0";
    }
    if (!(iss >> fullmoveNumber)) {
        fullmoveNumber = "1";
    }

    if (clearEnPassant) {
        enPassant = "-";
    }

    std::ostringstream normalized;
    normalized << piecePlacement << ' ' << sideToMove << ' ' << castling << ' ' << enPassant
               << ' ' << halfmoveClock << ' ' << fullmoveNumber;
    return normalized.str();
}

std::string lookupBookMoveString(
    const std::string& fen,
    const std::unordered_map<std::string, std::vector<std::string>>& openingBookOptions,
    const std::unordered_map<std::string, std::string>& openingBook, bool randomize) {
    auto lookupByKey = [&](const std::string& key) -> std::string {
        auto optionsIt = openingBookOptions.find(key);
        if (optionsIt != openingBookOptions.end()) {
            const auto& options = optionsIt->second;
            if (!options.empty()) {
                if (randomize) {
                    static std::random_device rd;
                    static std::mt19937 gen(static_cast<std::mt19937::result_type>(rd()));
                    std::uniform_int_distribution<std::size_t> dis(kZero, options.size() - kOne);
                    return options[dis(gen)];
                }
                return options.front();
            }
        }

        auto legacyIt = openingBook.find(key);
        if (legacyIt != openingBook.end()) {
            return legacyIt->second;
        }

        return "";
    };

    std::string move = lookupByKey(fen);
    if (!move.empty()) {
        return move;
    }

    std::string normalizedFen = normalizeBookFen(fen, true);
    if (normalizedFen != fen) {
        move = lookupByKey(normalizedFen);
        if (!move.empty()) {
            return move;
        }
    }

    return "";
}
