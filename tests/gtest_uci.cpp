#include "gtest/gtest.h"
#include "protocol/uci.h"

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

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
