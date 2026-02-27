#pragma once

#include "../core/BitboardMoves.h"
#include "../core/ChessBoard.h"
#include "../search/ValidMoves.h"
#include "Profiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <thread>
#include <vector>

class PerftTest {
private:
    std::atomic<uint64_t> totalNodes{0};
    std::atomic<uint64_t> captures{0};
    std::atomic<uint64_t> enPassants{0};
    std::atomic<uint64_t> castles{0};
    std::atomic<uint64_t> promotions{0};
    std::atomic<uint64_t> checks{0};
    std::atomic<uint64_t> checkmates{0};

    struct PerftResult {
        uint64_t nodes = 0;
        uint64_t captures = 0;
        uint64_t enPassants = 0;
        uint64_t castles = 0;
        uint64_t promotions = 0;
        uint64_t checks = 0;
        uint64_t checkmates = 0;
    };

    PerftResult perftHelper(Board& board, int depth) {
        PerftResult result;

        if (depth == 0) {
            result.nodes = 1;
            return result;
        }

        std::vector<MoveContent> moves;
        GenerateAllMoves(board, moves);

        for (const auto& move : moves) {
            Board newBoard = board;

            if (!MakeMove(newBoard, move)) {
                continue;
            }

            if (depth == 1) {
                result.nodes++;

                if (move.capture != ChessPieceType::NONE) {
                    result.captures++;
                }

                if (move.type == MoveType::CASTLE_KING || move.type == MoveType::CASTLE_QUEEN) {
                    result.castles++;
                }

                if (move.type == MoveType::EN_PASSANT) {
                    result.enPassants++;
                }

                if (move.promotion != ChessPieceType::NONE) {
                    result.promotions++;
                }

                if (IsInCheck(newBoard, newBoard.turn)) {
                    result.checks++;
                    std::vector<MoveContent> responseMoves;
                    GenerateAllMoves(newBoard, responseMoves);
                    bool hasLegalMove = false;

                    for (const auto& responseMove : responseMoves) {
                        Board testBoard = newBoard;
                        if (MakeMove(testBoard, responseMove)) {
                            hasLegalMove = true;
                            break;
                        }
                    }

                    if (!hasLegalMove) {
                        result.checkmates++;
                    }
                }
            } else {
                PerftResult subResult = perftHelper(newBoard, depth - 1);
                result.nodes += subResult.nodes;
                result.captures += subResult.captures;
                result.enPassants += subResult.enPassants;
                result.castles += subResult.castles;
                result.promotions += subResult.promotions;
                result.checks += subResult.checks;
                result.checkmates += subResult.checkmates;
            }
        }

        return result;
    }

    void perftThread(Board board, std::vector<MoveContent> moves, int depth,
                     std::promise<PerftResult> promise) {
        PerftResult threadResult;

        for (const auto& move : moves) {
            Board newBoard = board;

            if (!MakeMove(newBoard, move)) {
                continue;
            }

            PerftResult moveResult = perftHelper(newBoard, depth - 1);
            threadResult.nodes += moveResult.nodes;
            threadResult.captures += moveResult.captures;
            threadResult.enPassants += moveResult.enPassants;
            threadResult.castles += moveResult.castles;
            threadResult.promotions += moveResult.promotions;
            threadResult.checks += moveResult.checks;
            threadResult.checkmates += moveResult.checkmates;
        }

        promise.set_value(threadResult);
    }

public:
    uint64_t perft(Board& board, int depth, bool useThreads = true) {
        totalNodes = 0;
        captures = 0;
        enPassants = 0;
        castles = 0;
        promotions = 0;
        checks = 0;
        checkmates = 0;

        if (depth <= 0)
            return 1;

        std::vector<MoveContent> moves;
        GenerateAllMoves(board, moves);
        const int numThreads = useThreads ? std::thread::hardware_concurrency() : 1;

        if (depth > 3 && numThreads > 1 && moves.size() > numThreads) {
            std::vector<std::future<PerftResult>> futures;
            std::vector<std::thread> threads;
            int movesPerThread = (moves.size() + numThreads - 1) / numThreads;

            for (int t = 0; t < numThreads; t++) {
                int start = t * movesPerThread;
                int end = std::min(start + movesPerThread, static_cast<int>(moves.size()));

                if (start >= end)
                    break;

                std::vector<MoveContent> threadMoves(moves.begin() + start, moves.begin() + end);
                std::promise<PerftResult> promise;
                futures.push_back(promise.get_future());

                threads.emplace_back(&PerftTest::perftThread, this, board, std::move(threadMoves),
                                     depth, std::move(promise));
            }

            for (auto& thread : threads) {
                thread.join();
            }

            for (auto& future : futures) {
                PerftResult result = future.get();
                totalNodes += result.nodes;
                captures += result.captures;
                enPassants += result.enPassants;
                castles += result.castles;
                promotions += result.promotions;
                checks += result.checks;
                checkmates += result.checkmates;
            }
        } else {
            PerftResult result = perftHelper(board, depth);
            totalNodes = result.nodes;
            captures = result.captures;
            enPassants = result.enPassants;
            castles = result.castles;
            promotions = result.promotions;
            checks = result.checks;
            checkmates = result.checkmates;
        }

        return totalNodes;
    }

    void runPerftSuite(bool verbose = true) {
        struct TestPosition {
            std::string fen;
            std::string name;
            std::vector<uint64_t> expected;
        };

        std::vector<TestPosition> positions = {
            {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
             "Starting Position",
             {20, 400, 8902, 197281, 4865609}},

            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -",
             "Position 2",
             {48, 2039, 97862, 4085603}},

            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", "Position 3", {14, 191, 2812, 43238}},

            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
             "Position 4",
             {6, 264, 9467, 422333}},

            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
             "Position 5",
             {44, 1486, 62379, 2103487}}};

        std::cout << "\n========== Perft Test Suite ==========\n";
        struct DepthReport {
            size_t depth;
            uint64_t nodes;
            uint64_t expected;
            long long durationMs;
            double nps;
            bool passed;
            uint64_t captures;
            uint64_t enPassants;
            uint64_t castles;
            uint64_t promotions;
        };
        struct PositionReport {
            size_t index;
            std::string name;
            std::string fen;
            std::vector<DepthReport> depths;
        };

        const unsigned int hardware = std::thread::hardware_concurrency();
        const int fallbackThreads = 4;
        const int maxWorkers = (hardware == 0U) ? fallbackThreads : static_cast<int>(hardware);
        const int workerCount =
            std::max(1, std::min(static_cast<int>(positions.size()), maxWorkers));
        std::atomic<std::size_t> nextIndex{0};
        std::vector<std::future<std::vector<PositionReport>>> workers;
        workers.reserve(static_cast<std::size_t>(workerCount));

        for (int worker = 0; worker < workerCount; ++worker) {
            workers.emplace_back(std::async(std::launch::async, [&positions, &nextIndex]() {
                std::vector<PositionReport> reports;
                while (true) {
                    const std::size_t index = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (index >= positions.size()) {
                        break;
                    }

                    const auto& test = positions[index];
                    PositionReport report;
                    report.index = index;
                    report.name = test.name;
                    report.fen = test.fen;

                    Board board;
                    board.InitializeFromFEN(test.fen);
                    PerftTest workerPerft;
                    for (size_t depth = 1; depth <= test.expected.size(); depth++) {
                        auto start = std::chrono::high_resolution_clock::now();
                        uint64_t nodes = workerPerft.perft(board, static_cast<int>(depth), true);
                        auto end = std::chrono::high_resolution_clock::now();
                        auto duration =
                            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                        bool passed = (nodes == test.expected[depth - 1]);
                        double nps =
                            duration.count() > 0 ? (nodes * 1000.0 / duration.count()) : 0.0;
                        report.depths.push_back({depth,
                                                 nodes,
                                                 test.expected[depth - 1],
                                                 duration.count(),
                                                 nps,
                                                 passed,
                                                 workerPerft.captures.load(),
                                                 workerPerft.enPassants.load(),
                                                 workerPerft.castles.load(),
                                                 workerPerft.promotions.load()});
                        if (!passed) {
                            break;
                        }
                    }
                    reports.push_back(std::move(report));
                }
                return reports;
            }));
        }

        std::vector<PositionReport> allReports;
        for (auto& future : workers) {
            auto reports = future.get();
            allReports.insert(allReports.end(),
                              std::make_move_iterator(reports.begin()),
                              std::make_move_iterator(reports.end()));
        }

        std::sort(allReports.begin(), allReports.end(), [](const PositionReport& lhs,
                                                           const PositionReport& rhs) {
            return lhs.index < rhs.index;
        });

        for (const auto& report : allReports) {
            std::cout << "\n" << report.name << ":\n";
            std::cout << "FEN: " << report.fen << "\n\n";
            for (const auto& depthReport : report.depths) {
                std::cout << "Depth " << depthReport.depth << ": " << std::setw(10)
                          << depthReport.nodes;

                if (depthReport.passed) {
                    std::cout << " ✓";
                } else {
                    std::cout << " ✗ (expected " << depthReport.expected << ")";
                }

                std::cout << " | Time: " << std::setw(6) << depthReport.durationMs << " ms"
                          << " | NPS: " << std::fixed << std::setprecision(0) << std::setw(10)
                          << depthReport.nps << "\n";

                if (verbose && depthReport.depth <= 2) {
                    std::cout << "  Captures: " << depthReport.captures
                              << ", En passants: " << depthReport.enPassants
                              << ", Castles: " << depthReport.castles
                              << ", Promotions: " << depthReport.promotions << "\n";
                }
            }
        }

        std::cout << "\n=======================================\n";
    }

    void perftDivide(Board& board, int depth) {
        if (depth <= 0)
            return;

        std::vector<MoveContent> moves;
        GenerateAllMoves(board, moves);
        uint64_t total = 0;
        std::cout << "\nPerft divide at depth " << depth << ":\n";
        std::cout << std::string(40, '-') << "\n";

        for (const auto& move : moves) {
            Board newBoard = board;

            if (!MakeMove(newBoard, move)) {
                continue;
            }

            uint64_t nodes = depth == 1 ? 1 : perft(newBoard, depth - 1, false);
            total += nodes;
            std::cout << moveToString(move) << ": " << nodes << "\n";
        }

        std::cout << std::string(40, '-') << "\n";
        std::cout << "Total: " << total << " nodes\n";
    }

private:
    std::string moveToString(const MoveContent& move) {
        std::string result;
        result += char('a' + (move.src % 8));
        result += char('1' + (move.src / 8));
        result += char('a' + (move.dest % 8));
        result += char('1' + (move.dest / 8));

        if (move.promotion != ChessPieceType::NONE) {
            switch (move.promotion) {
                case ChessPieceType::QUEEN:
                    result += 'q';
                    break;
                case ChessPieceType::ROOK:
                    result += 'r';
                    break;
                case ChessPieceType::BISHOP:
                    result += 'b';
                    break;
                case ChessPieceType::KNIGHT:
                    result += 'n';
                    break;
                default:
                    break;
            }
        }

        return result;
    }
};
