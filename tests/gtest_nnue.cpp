#include "core/BitboardMoves.h"
#include "core/BitboardOnly.h"
#include "core/ChessBoard.h"
#include "evaluation/Evaluation.h"
#include "evaluation/NNUE.h"
#include "evaluation/NNUEBitboard.h"
#include "gtest/gtest.h"
#include "search/search.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::filesystem::path writeGeneratedNnueModel(int32_t outputBias = 64) {
    const auto path = std::filesystem::temp_directory_path() / "generated-test.nnue";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    const int32_t magic = 0x4E4E5545;
    const int32_t version = 1;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::vector<int16_t> ftWeights(
        static_cast<std::size_t>(NNUE::INPUT_DIMENSIONS) * static_cast<std::size_t>(NNUE::L1_SIZE),
        0);
    std::vector<int32_t> ftBiases(NNUE::L1_SIZE, 0);
    std::vector<int16_t> hidden1Weights(static_cast<std::size_t>(2) *
                                            static_cast<std::size_t>(NNUE::L1_SIZE) *
                                            static_cast<std::size_t>(NNUE::L2_SIZE),
                                        0);
    std::vector<int32_t> hidden1Biases(NNUE::L2_SIZE, 0);
    std::vector<int16_t> hidden2Weights(
        static_cast<std::size_t>(NNUE::L2_SIZE) * static_cast<std::size_t>(NNUE::L3_SIZE), 0);
    std::vector<int32_t> hidden2Biases(NNUE::L3_SIZE, 0);
    std::vector<int16_t> outputWeights(
        static_cast<std::size_t>(NNUE::L3_SIZE) * static_cast<std::size_t>(NNUE::OUTPUT_SIZE), 0);
    std::vector<int32_t> outputBiases(NNUE::OUTPUT_SIZE, outputBias);

    out.write(reinterpret_cast<const char*>(ftWeights.data()),
              static_cast<std::streamsize>(ftWeights.size() * sizeof(int16_t)));
    out.write(reinterpret_cast<const char*>(ftBiases.data()),
              static_cast<std::streamsize>(ftBiases.size() * sizeof(int32_t)));
    out.write(reinterpret_cast<const char*>(hidden1Weights.data()),
              static_cast<std::streamsize>(hidden1Weights.size() * sizeof(int16_t)));
    out.write(reinterpret_cast<const char*>(hidden1Biases.data()),
              static_cast<std::streamsize>(hidden1Biases.size() * sizeof(int32_t)));
    out.write(reinterpret_cast<const char*>(hidden2Weights.data()),
              static_cast<std::streamsize>(hidden2Weights.size() * sizeof(int16_t)));
    out.write(reinterpret_cast<const char*>(hidden2Biases.data()),
              static_cast<std::streamsize>(hidden2Biases.size() * sizeof(int32_t)));
    out.write(reinterpret_cast<const char*>(outputWeights.data()),
              static_cast<std::streamsize>(outputWeights.size() * sizeof(int16_t)));
    out.write(reinterpret_cast<const char*>(outputBiases.data()),
              static_cast<std::streamsize>(outputBiases.size() * sizeof(int32_t)));
    out.close();

    return path;
}

std::filesystem::path writeGeneratedBitboardNnueModel() {
    const auto path = std::filesystem::temp_directory_path() / "generated-test-bitboard.nnue";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    const uint32_t magic = 0x4E4E5545;
    const uint32_t version = 2;
    out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::vector<int16_t> featureWeights(static_cast<std::size_t>(NNUEBitboard::INPUT_DIMENSIONS) *
                                            static_cast<std::size_t>(NNUEBitboard::L1_SIZE),
                                        0);
    out.write(reinterpret_cast<const char*>(featureWeights.data()),
              static_cast<std::streamsize>(featureWeights.size() * sizeof(int16_t)));
    out.close();

    return path;
}
} // namespace

class NNUETest : public ::testing::Test {
protected:
    void SetUp() override {
        initKnightAttacks();
        initKingAttacks();
        InitZobrist();
        setNNUEEnabled(false);
        NNUE::globalEvaluator.reset();
        NNUEBitboard::globalEvaluator.reset();
    }

    void TearDown() override {
        setNNUEEnabled(false);
        NNUE::globalEvaluator.reset();
        NNUEBitboard::globalEvaluator.reset();
    }
};

TEST_F(NNUETest, DirectNnueEvaluateReturnsZeroWithoutModel) {
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    EXPECT_EQ(NNUE::evaluate(board), 0);
}

TEST_F(NNUETest, EvaluationFallsBackToClassicalWhenNnueEnabledWithoutModel) {
    Board board;
    board.InitializeFromFEN("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3");

    setNNUEEnabled(false);
    const int classical = evaluatePosition(board);

    setNNUEEnabled(true);
    const int fallback = evaluatePosition(board);

    EXPECT_EQ(fallback, classical);
}

TEST_F(NNUETest, FailedNnueInitDoesNotKeepEvaluatorAlive) {
    EXPECT_FALSE(NNUE::init("/definitely/missing/nnue-file.bin"));
    EXPECT_EQ(NNUE::globalEvaluator.get(), nullptr);
}

TEST_F(NNUETest, GeneratedModelLoadsAndDrivesEvaluation) {
    const auto path = writeGeneratedNnueModel();
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    ASSERT_TRUE(NNUE::init(path.string()));
    ASSERT_NE(NNUE::globalEvaluator.get(), nullptr);
    EXPECT_EQ(NNUE::evaluate(board), 1);

    setNNUEEnabled(true);
    EXPECT_EQ(evaluatePosition(board), 1);

    std::filesystem::remove(path);
}

TEST_F(NNUETest, GeneratedBitboardModelLoadsAndEvaluatesFromBoard) {
    const auto path = writeGeneratedBitboardNnueModel();
    Board board;
    board.InitializeFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    ASSERT_TRUE(NNUEBitboard::init(path.string()));
    ASSERT_NE(NNUEBitboard::globalEvaluator.get(), nullptr);

    const BitboardPosition pos = BitboardPosition::fromBoard(board);
    EXPECT_EQ(pos.getSideToMove(), ChessPieceColor::WHITE);
    EXPECT_EQ(pos.getPieceBitboard(ChessPieceType::KING, ChessPieceColor::WHITE),
              board.whiteKings);
    EXPECT_EQ(pos.getAllPieces(), board.allPieces);

    std::filesystem::remove(path);
}
