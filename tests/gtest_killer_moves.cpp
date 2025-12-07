#include "core/BitboardMoves.h"
#include "core/ChessBoard.h"
#include "gtest/gtest.h"
#include "search/search.h"

TEST(KillerMoves, Storage) {
    KillerMoves killerTest;
    std::pair<int, int> testMove1 = {12, 20};
    std::pair<int, int> testMove2 = {6, 22};
    std::pair<int, int> testMove3 = {1, 18};

    killerTest.store(0, testMove1);
    killerTest.store(0, testMove2);
    killerTest.store(0, testMove3);

    ASSERT_TRUE(killerTest.isKiller(0, testMove1));
    ASSERT_TRUE(killerTest.isKiller(0, testMove2));
    ASSERT_TRUE(killerTest.isKiller(0, testMove3));

    std::pair<int, int> nonKillerMove = {8, 16};
    ASSERT_FALSE(killerTest.isKiller(0, nonKillerMove));
}

TEST(KillerMoves, MultiPly) {
    KillerMoves multiPlyTest;

    multiPlyTest.store(0, {12, 20});
    multiPlyTest.store(1, {6, 22});
    multiPlyTest.store(2, {1, 18});

    ASSERT_TRUE(multiPlyTest.isKiller(0, {12, 20}));
    ASSERT_FALSE(multiPlyTest.isKiller(0, {6, 22}));

    ASSERT_TRUE(multiPlyTest.isKiller(1, {6, 22}));
    ASSERT_FALSE(multiPlyTest.isKiller(1, {12, 20}));

    ASSERT_TRUE(multiPlyTest.isKiller(2, {1, 18}));
    ASSERT_FALSE(multiPlyTest.isKiller(2, {6, 22}));
}
