#include "gtest/gtest.h"
#include "search/TranspositionTableV2.h"

TEST(TranspositionTable, PackAndUnpackMoveRoundTrip) {
    const TTv2::PackedMove packed = TTv2::packMove(12, 28, 1);
    const TTv2::UnpackedMove unpacked = TTv2::unpackMove(packed);
    EXPECT_EQ(unpacked.from, 12);
    EXPECT_EQ(unpacked.to, 28);
    EXPECT_EQ(unpacked.promotion, 1);
}

TEST(TranspositionTable, ProbeStoresAndRetrievesExactEntry) {
    TTv2::TranspositionTable table;
    table.resize(1);

    constexpr uint64_t key = 0x123456789ABCDEF0ULL;
    bool found = false;
    TTv2::TTEntry* entry = table.probe(key, found);
    ASSERT_FALSE(found);
    ASSERT_NE(entry, nullptr);

    entry->save(key, 42, 7, TTv2::BOUND_EXACT, 6, TTv2::packMove(12, 28), 0);

    found = false;
    TTv2::TTEntry* retrieved = table.probe(key, found);
    ASSERT_TRUE(found);
    ASSERT_EQ(retrieved, entry);
    EXPECT_EQ(retrieved->value, 42);
    EXPECT_EQ(retrieved->eval, 7);
    EXPECT_EQ(retrieved->depth, 6);
    EXPECT_EQ(retrieved->bound(), TTv2::BOUND_EXACT);

    const TTv2::UnpackedMove move = TTv2::unpackMove(retrieved->move);
    EXPECT_EQ(move.from, 12);
    EXPECT_EQ(move.to, 28);
}

TEST(TranspositionTable, DeeperEntryReplacesShallowerEntry) {
    TTv2::TranspositionTable table;
    table.resize(1);

    constexpr uint64_t key = 0xDEADBEEFCAFE0001ULL;
    bool found = false;
    TTv2::TTEntry* entry = table.probe(key, found);
    entry->save(key, 10, 0, TTv2::BOUND_UPPER, 4, TTv2::packMove(0, 1), 0);

    found = false;
    entry = table.probe(key, found);
    entry->save(key, 20, 0, TTv2::BOUND_EXACT, 8, TTv2::packMove(2, 3), 0);

    found = false;
    entry = table.probe(key, found);
    ASSERT_TRUE(found);
    EXPECT_EQ(entry->value, 20);
    EXPECT_EQ(entry->depth, 8);
    EXPECT_EQ(entry->bound(), TTv2::BOUND_EXACT);
}

TEST(TranspositionTable, NewSearchAdvancesGeneration) {
    TTv2::TranspositionTable table;
    table.resize(1);
    EXPECT_EQ(table.generation(), 0);

    table.newSearch();
    EXPECT_EQ(table.generation(), 4);

    table.newSearch();
    EXPECT_EQ(table.generation(), 8);
}

TEST(TranspositionTable, ClearResetsTable) {
    TTv2::TranspositionTable table;
    table.resize(1);

    bool found = false;
    TTv2::TTEntry* entry = table.probe(0xABCDULL, found);
    entry->save(0xABCDULL, 99, 0, TTv2::BOUND_EXACT, 10, TTv2::packMove(4, 5), 0);

    table.clear();
    EXPECT_EQ(table.generation(), 0);

    found = false;
    entry = table.probe(0xABCDULL, found);
    EXPECT_FALSE(found);
    EXPECT_EQ(entry->depth, 0);
}

TEST(TranspositionTable, HashfullReportsUsage) {
    TTv2::TranspositionTable table;
    table.resize(16);

    for (uint64_t i = 0; i < 100; ++i) {
        bool found = false;
        TTv2::TTEntry* entry = table.probe(i * 0x10007ULL, found);
        entry->save(i * 0x10007ULL, static_cast<int16_t>(i), 0, TTv2::BOUND_EXACT, 6,
                  TTv2::packMove(static_cast<int>(i % 64), static_cast<int>((i + 1) % 64)), 0);
    }

    EXPECT_GT(table.hashfull(), 0);
    EXPECT_LE(table.hashfull(), 1000);
}
