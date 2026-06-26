#include "gtest/gtest.h"
#include "search/BookUtils.h"

#include <unordered_map>
#include <vector>

TEST(BookUtils, NormalizeBookFenClearsEnPassantWhenRequested) {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    EXPECT_EQ(normalizeBookFen(fen, true),
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
}

TEST(BookUtils, NormalizeBookFenPreservesEnPassantWhenNotCleared) {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    EXPECT_EQ(normalizeBookFen(fen, false), fen);
}

TEST(BookUtils, NormalizeBookFenFillsMissingMoveCounters) {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -";
    EXPECT_EQ(normalizeBookFen(fen, false),
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST(BookUtils, LookupBookMoveFindsExactFenMatch) {
    std::unordered_map<std::string, std::vector<std::string>> options;
    std::unordered_map<std::string, std::string> legacy;
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    options[fen] = {"e2e4", "d2d4"};

    EXPECT_EQ(lookupBookMoveString(fen, options, legacy, false), "e2e4");
}

TEST(BookUtils, LookupBookMoveFallsBackToNormalizedFen) {
    std::unordered_map<std::string, std::vector<std::string>> options;
    std::unordered_map<std::string, std::string> legacy;
    const std::string normalized =
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";
    options[normalized] = {"e7e5"};

    const std::string fenWithEp =
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    EXPECT_EQ(lookupBookMoveString(fenWithEp, options, legacy, false), "e7e5");
}

TEST(BookUtils, LookupBookMoveUsesLegacyMapWhenOptionsMissing) {
    std::unordered_map<std::string, std::vector<std::string>> options;
    std::unordered_map<std::string, std::string> legacy;
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    legacy[fen] = "g1f3";

    EXPECT_EQ(lookupBookMoveString(fen, options, legacy, false), "g1f3");
}

TEST(BookUtils, LookupBookMoveReturnsEmptyWhenNoMatch) {
    std::unordered_map<std::string, std::vector<std::string>> options;
    std::unordered_map<std::string, std::string> legacy;
    const std::string fen = "8/8/8/8/8/8/8/8 w - - 0 1";

    EXPECT_TRUE(lookupBookMoveString(fen, options, legacy, false).empty());
}
