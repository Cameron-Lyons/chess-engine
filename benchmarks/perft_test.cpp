#include "benchmark_args.h"
#include "benchmark_suite.h"
#include "src/core/ChessBoard.h"
#include "src/search/search.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int kDefaultDepthLimit = 4;

struct PerftRow {
    std::string id;
    std::string positionName;
    int depth = 0;
    std::uint64_t nodes = 0;
    long long elapsedMs = 0;
    double nps = 0.0;
};

std::uint64_t perft(Board& board, ChessPieceColor color, int depth) {
    if (depth == 0) {
        return 1;
    }

    const auto moves = GetAllMoves(board, color);
    const ChessPieceColor nextColor =
        (color == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    std::uint64_t nodes = 0;

    for (const auto& move : moves) {
        const Piece movingPiece = board.squares[move.first].piece;
        Board copy = board;
        if (!applySearchMove(copy, move.first, move.second, false)) {
            continue;
        }
        if (isInCheck(copy, color)) {
            continue;
        }

        const int destinationRank = move.second / 8;
        const bool isPromotion = movingPiece.PieceType == ChessPieceType::PAWN &&
                                 (destinationRank == 0 || destinationRank == 7);
        if (isPromotion) {
            constexpr std::array<ChessPieceType, 4> kPromotionPieces = {
                ChessPieceType::QUEEN, ChessPieceType::ROOK, ChessPieceType::BISHOP,
                ChessPieceType::KNIGHT};
            for (ChessPieceType promotionPiece : kPromotionPieces) {
                Board promoted = copy;
                promoted.squares[move.second].piece = Piece(color, promotionPiece);
                promoted.squares[move.second].piece.moved = true;
                promoted.updateBitboards();
                promoted.turn = nextColor;
                nodes += perft(promoted, nextColor, depth - 1);
            }
            continue;
        }

        copy.turn = nextColor;
        nodes += perft(copy, nextColor, depth - 1);
    }

    return nodes;
}

void printTextReport(const std::vector<PerftRow>& rows, int maxDepth) {
    std::cout << "Perft benchmark\n";
    std::cout << "max_depth=" << maxDepth << '\n';
    std::cout << "id\tdepth\tnodes\telapsed_ms\tnps\n";
    for (const auto& row : rows) {
        std::cout << row.id << '\t' << row.depth << '\t' << row.nodes << '\t' << row.elapsedMs
                  << '\t' << row.nps << '\n';
    }
}

void printJsonReport(const std::vector<PerftRow>& rows, int maxDepth) {
    std::cout << "{\n";
    std::cout << "  \"benchmark\": \"perft_benchmark\",\n";
    std::cout << "  \"config\": {\"max_depth\": " << maxDepth << "},\n";
    std::cout << "  \"results\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        std::cout << "    {\"id\": \"" << BenchmarkSuite::jsonEscape(row.id)
                  << "\", \"position_name\": \"" << BenchmarkSuite::jsonEscape(row.positionName)
                  << "\", \"depth\": " << row.depth << ", \"nodes\": " << row.nodes
                  << ", \"elapsed_ms\": " << row.elapsedMs << ", \"nps\": " << row.nps << "}";
        if (i + 1 < rows.size()) {
            std::cout << ',';
        }
        std::cout << '\n';
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
}
} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    const std::vector<std::string> args(argv + 1, argv + argc);
    const int maxDepth = BenchmarkArgs::parsePositiveIntArg(args, "--depth", kDefaultDepthLimit);
    const BenchmarkSuite::OutputFormat format = BenchmarkSuite::parseOutputFormat(args);
    const std::vector<BenchmarkSuite::PositionCase> positions =
        BenchmarkSuite::selectPositions(args);

    if (positions.empty()) {
        std::cerr << "No benchmark positions matched the requested filters.\n";
        return 1;
    }

    std::vector<PerftRow> rows;
    rows.reserve(positions.size() * static_cast<std::size_t>(maxDepth));

    {
        BenchmarkSuite::ScopedStdoutSilencer silencer(format == BenchmarkSuite::OutputFormat::JSON);
        BenchmarkSuite::initializeEngineState();

        for (const auto& position : positions) {
            for (int depth = 1; depth <= maxDepth; ++depth) {
                Board board;
                board.InitializeFromFEN(position.fen);
                const auto start = std::chrono::steady_clock::now();
                const std::uint64_t nodes = perft(board, board.turn, depth);
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - start)
                                           .count();
                const double elapsedSeconds =
                    std::max(0.001, static_cast<double>(elapsedMs) / 1000.0);
                rows.push_back({position.id + ":d" + std::to_string(depth), position.name, depth,
                                nodes, elapsedMs, static_cast<double>(nodes) / elapsedSeconds});
            }
        }
    }

    if (format == BenchmarkSuite::OutputFormat::JSON) {
        printJsonReport(rows, maxDepth);
    } else {
        printTextReport(rows, maxDepth);
    }

    return 0;
}
