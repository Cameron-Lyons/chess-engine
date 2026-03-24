#include "gtest/gtest.h"
#include "protocol/uci.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <thread>

namespace {
class CoutCapture {
public:
    CoutCapture() : oldBuffer(std::cout.rdbuf(buffer.rdbuf())) {}

    ~CoutCapture() {
        std::cout.rdbuf(oldBuffer);
    }

    std::string str() const {
        return buffer.str();
    }

    void clear() {
        buffer.str("");
        buffer.clear();
    }

private:
    std::ostringstream buffer;
    std::streambuf* oldBuffer;
};
} // namespace

TEST(UCI, ReportsUciOptionsAndReady) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("uci");
    engine.processCommand("isready");

    const std::string output = capture.str();
    EXPECT_NE(output.find("id name ModernChess v2.0"), std::string::npos);
    EXPECT_NE(output.find("option name Hash"), std::string::npos);
    EXPECT_NE(output.find("uciok"), std::string::npos);
    EXPECT_NE(output.find("readyok"), std::string::npos);
}

TEST(UCI, RejectsMalformedPositionCommands) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("position");
    engine.processCommand("position fen invalid fen");

    const std::string output = capture.str();
    EXPECT_NE(output.find("info string Error: Invalid position command"), std::string::npos);
    EXPECT_NE(output.find("info string Error: Invalid FEN string"), std::string::npos);
}

TEST(UCI, WarnsAboutIllegalMoveInMoveList) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("position startpos moves e2e4 e7e5 g1f3 b8c6 c1g5");

    const std::string output = capture.str();
    EXPECT_NE(output.find("info string Warning: Invalid move c1g5"), std::string::npos);
}

TEST(UCI, ReportsUnknownCommand) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("notacommand");

    const std::string output = capture.str();
    EXPECT_NE(output.find("info string Unknown command: notacommand"), std::string::npos);
}

TEST(UCI, ConvertsMovesToAndFromUciNotation) {
    const Move move{12, 28};
    EXPECT_EQ(UCINotation::moveToUCI(move), "e2e4");

    const auto parsed = UCINotation::uciToMove("e2e4");
    const Move parsedMove = parsed.value_or(Move{-1, -1});
    ASSERT_GE(parsedMove.first, 0);
    ASSERT_GE(parsedMove.second, 0);
    EXPECT_EQ(parsedMove.first, 12);
    EXPECT_EQ(parsedMove.second, 28);

    EXPECT_EQ(UCINotation::moveToUCI({-1, -1}), "0000");
    EXPECT_FALSE(UCINotation::uciToMove("e9e4").has_value());
}

TEST(UCI, ReportsNnueFallbackWhenNoModelIsLoaded) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("setoption name Use Neural Network value true");

    const std::string output = capture.str();
    EXPECT_NE(output.find("info string NNUE requested but no model loaded; using classical eval"),
              std::string::npos);
}

TEST(UCI, ReturnsBestmove0000OnStalematePosition) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    engine.processCommand("go movetime 30");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const std::string output = capture.str();
    EXPECT_NE(output.find("bestmove 0000"), std::string::npos);
}

TEST(UCI, RejectsSecondGoWhileSearchIsRunning) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("position startpos");
    engine.processCommand("go movetime 200");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    engine.processCommand("go movetime 20");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const std::string output = capture.str();
    EXPECT_NE(output.find("info string Search already in progress"), std::string::npos);
}

TEST(UCI, CanSearchAgainAfterStop) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("position startpos");
    engine.processCommand("go movetime 200");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    engine.processCommand("stop");
    capture.clear();

    engine.processCommand("go movetime 30");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const std::string output = capture.str();
    EXPECT_NE(output.find("bestmove "), std::string::npos);
    EXPECT_NE(output.find("info depth "), std::string::npos);
}

TEST(UCI, SearchStillWorksAfterOptionUpdates) {
    UCIEngine engine;
    CoutCapture capture;

    engine.processCommand("setoption name Hash value 64");
    engine.processCommand("setoption name Threads value 2");
    engine.processCommand("setoption name MultiPV value 2");
    engine.processCommand("setoption name Move Overhead value 0");
    engine.processCommand("setoption name Minimum Thinking Time value 0");
    engine.processCommand("setoption name OwnBook value false");
    engine.processCommand("position startpos");
    engine.processCommand("go movetime 30");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const std::string output = capture.str();
    EXPECT_NE(output.find("bestmove "), std::string::npos);
}
