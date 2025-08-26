#pragma once

#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <iostream>
#include <iomanip>
#include <chrono>
#include "../core/ChessBoard.h"
#include "../search/ValidMoves.h"
#include "../core/BitboardMoves.h"
#include "Profiler.h"

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
        
        if (depth <= 0) return 1;
        
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
                
                if (start >= end) break;
                
                std::vector<MoveContent> threadMoves(moves.begin() + start, moves.begin() + end);
                std::promise<PerftResult> promise;
                futures.push_back(promise.get_future());
                
                threads.emplace_back(&PerftTest::perftThread, this, board, 
                                   std::move(threadMoves), depth, std::move(promise));
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
             "Starting Position", {20, 400, 8902, 197281, 4865609}},
            
            {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 
             "Position 2", {48, 2039, 97862, 4085603}},
            
            {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 
             "Position 3", {14, 191, 2812, 43238}},
            
            {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
             "Position 4", {6, 264, 9467, 422333}},
            
            {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
             "Position 5", {44, 1486, 62379, 2103487}}
        };
        
        std::cout << "\n========== Perft Test Suite ==========\n";
        
        for (const auto& test : positions) {
            Board board;
            board.InitializeFromFEN(test.fen);
            
            std::cout << "\n" << test.name << ":\n";
            std::cout << "FEN: " << test.fen << "\n\n";
            
            for (size_t depth = 1; depth <= test.expected.size(); depth++) {
                auto start = std::chrono::high_resolution_clock::now();
                uint64_t result = perft(board, depth, true);
                auto end = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                double nps = duration.count() > 0 ? (result * 1000.0 / duration.count()) : 0;
                
                bool passed = (result == test.expected[depth - 1]);
                
                std::cout << "Depth " << depth << ": " 
                          << std::setw(10) << result;
                
                if (passed) {
                    std::cout << " ✓";
                } else {
                    std::cout << " ✗ (expected " << test.expected[depth - 1] << ")";
                }
                
                std::cout << " | Time: " << std::setw(6) << duration.count() << " ms"
                          << " | NPS: " << std::fixed << std::setprecision(0) 
                          << std::setw(10) << nps << "\n";
                
                if (verbose && depth <= 2) {
                    std::cout << "  Captures: " << captures.load() 
                              << ", En passants: " << enPassants.load()
                              << ", Castles: " << castles.load()
                              << ", Promotions: " << promotions.load() << "\n";
                }
                
                if (!passed) break;
            }
        }
        
        std::cout << "\n=======================================\n";
    }
    
    void perftDivide(Board& board, int depth) {
        if (depth <= 0) return;
        
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
                case ChessPieceType::QUEEN: result += 'q'; break;
                case ChessPieceType::ROOK: result += 'r'; break;
                case ChessPieceType::BISHOP: result += 'b'; break;
                case ChessPieceType::KNIGHT: result += 'n'; break;
                default: break;
            }
        }
        
        return result;
    }
};