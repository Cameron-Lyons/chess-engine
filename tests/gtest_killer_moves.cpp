#include "../src/ChessBoard.h"
#include "../src/search.h"
#include "../src/BitboardMoves.h"
#include "gtest/gtest.h"

TEST(KillerMoves, Storage) {
    KillerMoves killerTest;
    std::pair<int, int> testMove1 = {12, 20}; // e2-e4
    std::pair<int, int> testMove2 = {6, 22};  // g1-f3
    std::pair<int, int> testMove3 = {1, 18};  // b1-c3

    killerTest.store(0, testMove1);
    killerTest.store(0, testMove2);
    killerTest.store(0, testMove3);

    ASSERT_TRUE(killerTest.isKiller(0, testMove1));
    ASSERT_TRUE(killerTest.isKiller(0, testMove2));
    ASSERT_TRUE(killerTest.isKiller(0, testMove3));

    std::pair<int, int> nonKillerMove = {8, 16}; // a2-a3
    ASSERT_FALSE(killerTest.isKiller(0, nonKillerMove));
}

TEST(KillerMoves, MultiPly) {
    KillerMoves multiPlyTest;

    multiPlyTest.store(0, {12, 20}); // e2-e4 at ply 0
    multiPlyTest.store(1, {6, 22});  // g1-f3 at ply 1
    multiPlyTest.store(2, {1, 18});  // b1-c3 at ply 2

    ASSERT_TRUE(multiPlyTest.isKiller(0, {12, 20}));
    ASSERT_FALSE(multiPlyTest.isKiller(0, {6, 22}));

    ASSERT_TRUE(multiPlyTest.isKiller(1, {6, 22}));
    ASSERT_FALSE(multiPlyTest.isKiller(1, {12, 20}));

    ASSERT_TRUE(multiPlyTest.isKiller(2, {1, 18}));
    ASSERT_FALSE(multiPlyTest.isKiller(2, {6, 22}));
}
