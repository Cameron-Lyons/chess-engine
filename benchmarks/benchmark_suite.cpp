#include "benchmark_suite.h"

#include "benchmark_args.h"
#include "src/core/BitboardMoves.h"
#include "src/search/search.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <sstream>

namespace BenchmarkSuite {
namespace {
bool hasAnyTag(const PositionCase& position, std::span<const std::string> tagFilters) {
    if (tagFilters.empty()) {
        return true;
    }
    return std::ranges::any_of(tagFilters, [&](const std::string& filter) {
        return std::find(position.tags.begin(), position.tags.end(), filter) != position.tags.end();
    });
}

bool hasRequestedId(const PositionCase& position, std::span<const std::string> idFilters) {
    if (idFilters.empty()) {
        return true;
    }
    return std::find(idFilters.begin(), idFilters.end(), position.id) != idFilters.end() ||
           std::find(idFilters.begin(), idFilters.end(), position.name) != idFilters.end();
}
} // namespace

std::vector<PositionCase> defaultCorpus() {
    return {
        {"startpos",
         "Start Position",
         "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
         {"opening", "baseline"}},
        {"italian",
         "Italian Development",
         "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3",
         {"opening", "tactical"}},
        {"kiwipete",
         "Kiwipete",
         "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
         {"middlegame", "castling", "perft"}},
        {"position3",
         "Position 3",
         "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
         {"endgame", "perft", "legality"}},
        {"sharp",
         "Sharp Tactical",
         "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
         {"middlegame", "tactical"}},
    };
}

std::vector<PositionCase> selectPositions(std::span<const std::string> args) {
    const std::vector<std::string> requestedIds = BenchmarkArgs::parseCsvArg(args, "--positions");
    const std::vector<std::string> requestedTags = BenchmarkArgs::parseCsvArg(args, "--tags");

    std::vector<PositionCase> selected;
    for (const auto& position : defaultCorpus()) {
        if (!hasRequestedId(position, requestedIds)) {
            continue;
        }
        if (!hasAnyTag(position, requestedTags)) {
            continue;
        }
        selected.push_back(position);
    }
    return selected;
}

OutputFormat parseOutputFormat(std::span<const std::string> args) {
    const std::string format = BenchmarkArgs::parseStringArg(args, "--format", "text");
    return (format == "json") ? OutputFormat::JSON : OutputFormat::TEXT;
}

std::string outputFormatName(OutputFormat format) {
    return (format == OutputFormat::JSON) ? "json" : "text";
}

std::string jsonEscape(const std::string& value) {
    std::ostringstream escaped;
    for (char c : value) {
        switch (c) {
            case '\\':
                escaped << "\\\\";
                break;
            case '"':
                escaped << "\\\"";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                escaped << c;
                break;
        }
    }
    return escaped.str();
}

std::string joinTags(std::span<const std::string> tags, std::string_view separator) {
    std::ostringstream out;
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) {
            out << separator;
        }
        out << tags[i];
    }
    return out.str();
}

void initializeEngineState() {
    initKnightAttacks();
    initKingAttacks();
    InitZobrist();
}

BenchmarkSuite::ScopedStdoutSilencer::ScopedStdoutSilencer(bool enabledIn) : enabled(enabledIn) {
    if (!enabled) {
        return;
    }
    sinkStorage = std::make_unique<std::ostringstream>();
    originalBuffer = std::cout.rdbuf(sinkStorage->rdbuf());
}

BenchmarkSuite::ScopedStdoutSilencer::~ScopedStdoutSilencer() {
    if (!enabled) {
        return;
    }
    std::cout.rdbuf(originalBuffer);
}

} // namespace BenchmarkSuite
