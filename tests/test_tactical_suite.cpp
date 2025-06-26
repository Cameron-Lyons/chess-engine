#include "ChessBoard.h"
#include "search.h"
#include "engine_globals.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <string>

using namespace std::chrono;

struct TacticalPuzzle {
    std::string name;
    std::string fen;
    std::string bestMove; // In algebraic notation like "Qh5+" or from-to like "f6h8"
    std::string description;
    int difficulty; // 1-5, where 5 is hardest
    int maxTimeMs; // Maximum time to solve
};

class TacticalTestSuite {
public:
    static std::vector<TacticalPuzzle> getPuzzles() {
        return {
            // Mate in 1 puzzles
            {
                "Mate in 1 - Back Rank",
                "6k1/5ppp/8/8/8/8/8/R7 w - - 0 1",
                "a1a8",
                "Simple back rank mate",
                1, 1000
            },
            {
                "Mate in 1 - Queen & King",
                "6k1/5ppp/6q1/8/8/8/8/7K b - - 0 1",
                "g6g1",
                "Back rank mate with queen",
                1, 1000
            },
            {
                "Mate in 1 - Smothered",
                "6rk/6pp/8/8/8/8/8/6QK w - - 0 1",
                "g1g8",
                "Simple queen mate",
                1, 1000
            },
            
            // Tactical motifs
            {
                "Fork - Knight",
                "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R b KQkq - 0 4",
                "f6d5",
                "Knight forks bishop and pawn",
                2, 2000
            },
            {
                "Pin - Bishop",
                "rnbqkb1r/ppp2ppp/3p1n2/4p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 4",
                "c4f7",
                "Bishop pins knight to king",
                2, 2000
            },
            {
                "Skewer - Queen",
                "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 4",
                "c4f7",
                "Check leads to skewer",
                3, 3000
            },
            
            // Material winning combinations
            {
                "Win Queen - Discovered Attack",
                "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4",
                "f3d4",
                "Knight move discovers bishop attack on queen",
                3, 3000
            },
            {
                "Win Rook - Double Attack",
                "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/3P1N2/PPP2PPP/RNBQKB1R w KQkq - 0 4",
                "f1b5",
                "Bishop attacks knight and rook",
                3, 3000
            },
            
            // Mate in 2 puzzles
            {
                "Mate in 2 - Queen Sacrifice",
                "6k1/5ppp/8/8/8/8/5PPP/R6K w - - 0 1",
                "a1a8",
                "Prepare back rank mate",
                4, 5000
            },
            {
                "Mate in 2 - Smothered Mate",
                "6rk/6pp/8/8/8/8/6PP/5RQK w - - 0 1",
                "f1f8",
                "Classic smothered mate pattern",
                4, 5000
            },
            
            // Advanced tactical themes
            {
                "Deflection",
                "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
                "a1a8",
                "Deflect rook from defense",
                4, 4000
            },
            {
                "Clearance",
                "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 3",
                "f3e5",
                "Clear the line for attack",
                3, 3000
            },
            
            // Defensive tactics
            {
                "Counter-attack",
                "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/3P1N2/PPP2PPP/RNBQKB1R b KQkq - 0 4",
                "f6e4",
                "Counter-attack in center",
                3, 3000
            },
            {
                "Escape - King Safety",
                "rnbqk2r/pppp1ppp/5n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R b KQkq - 0 4",
                "e8g8",
                "Castle to safety",
                2, 2000
            }
        };
    }
    
    static bool parseMove(const std::string& moveStr, int& from, int& to) {
        if (moveStr.length() != 4) return false;
        
        int fromCol = moveStr[0] - 'a';
        int fromRow = moveStr[1] - '1';
        int toCol = moveStr[2] - 'a';
        int toRow = moveStr[3] - '1';
        
        if (fromCol < 0 || fromCol > 7 || fromRow < 0 || fromRow > 7 ||
            toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7) {
            return false;
        }
        
        from = fromRow * 8 + fromCol;
        to = toRow * 8 + toCol;
        return true;
    }
    
    static std::string moveToString(int from, int to) {
        char fromFile = 'a' + (from % 8);
        char fromRank = '1' + (from / 8);
        char toFile = 'a' + (to % 8);
        char toRank = '1' + (to / 8);
        
        return std::string() + fromFile + fromRank + toFile + toRank;
    }
    
    static void runTacticalTest() {
        std::cout << "🎯 TACTICAL TEST SUITE\n";
        std::cout << "======================\n\n";
        
        InitZobrist(); // Initialize hash tables
        
        auto puzzles = getPuzzles();
        int solved = 0;
        int total = puzzles.size();
        
        std::cout << "Testing " << total << " tactical puzzles...\n\n";
        
        for (size_t i = 0; i < puzzles.size(); i++) {
            const auto& puzzle = puzzles[i];
            
            std::cout << "Puzzle " << (i+1) << ": " << puzzle.name << "\n";
            std::cout << "Difficulty: " << std::string(puzzle.difficulty, '*') << "\n";
            std::cout << "Description: " << puzzle.description << "\n";
            std::cout << "Position: " << puzzle.fen << "\n";
            
            Board testBoard;
            testBoard.InitializeFromFEN(puzzle.fen);
            
            // Parse expected move
            int expectedFrom, expectedTo;
            if (!parseMove(puzzle.bestMove, expectedFrom, expectedTo)) {
                std::cout << "❌ INVALID expected move format: " << puzzle.bestMove << "\n\n";
                continue;
            }
            
            // Search for best move with time limit
            auto start = high_resolution_clock::now();
            
            // Use increasing depth until time limit
            std::pair<int, int> bestMove = {-1, -1};
            for (int depth = 4; depth <= 10; depth++) {
                auto current = high_resolution_clock::now();
                auto elapsed = duration_cast<milliseconds>(current - start).count();
                
                if (elapsed > puzzle.maxTimeMs) break;
                
                bestMove = findBestMove(testBoard, depth);
                
                // Check if we found the solution
                if (bestMove.first == expectedFrom && bestMove.second == expectedTo) {
                    break;
                }
            }
            
            auto end = high_resolution_clock::now();
            auto elapsed = duration_cast<milliseconds>(end - start).count();
            
            std::string foundMove = moveToString(bestMove.first, bestMove.second);
            
            if (bestMove.first == expectedFrom && bestMove.second == expectedTo) {
                std::cout << "✅ SOLVED in " << elapsed << "ms";
                std::cout << " - Found: " << foundMove << "\n";
                solved++;
            } else {
                std::cout << "❌ FAILED after " << elapsed << "ms";
                std::cout << " - Expected: " << puzzle.bestMove;
                std::cout << ", Found: " << foundMove << "\n";
            }
            std::cout << "\n";
        }
        
        std::cout << "=== TACTICAL TEST RESULTS ===\n";
        std::cout << "Solved: " << solved << "/" << total;
        std::cout << " (" << (100 * solved / total) << "%)\n";
        
        if (solved >= total * 0.8) {
            std::cout << "🏆 EXCELLENT tactical strength!\n";
        } else if (solved >= total * 0.6) {
            std::cout << "🥉 GOOD tactical strength\n";
        } else if (solved >= total * 0.4) {
            std::cout << "⚠️  AVERAGE tactical strength\n";
        } else {
            std::cout << "🔧 NEEDS IMPROVEMENT in tactics\n";
        }
        
        std::cout << "\nTactical Rating Estimate:\n";
        if (solved >= total * 0.9) {
            std::cout << "🎯 Expert level (2000+ Elo)\n";
        } else if (solved >= total * 0.7) {
            std::cout << "🎯 Advanced level (1800-2000 Elo)\n";
        } else if (solved >= total * 0.5) {
            std::cout << "🎯 Intermediate level (1500-1800 Elo)\n";
        } else if (solved >= total * 0.3) {
            std::cout << "🎯 Beginner+ level (1200-1500 Elo)\n";
        } else {
            std::cout << "🎯 Beginner level (<1200 Elo)\n";
        }
        
        std::cout << "\n✅ Tactical test suite completed!\n";
    }
};

int main() {
    try {
        TacticalTestSuite::runTacticalTest();
    } catch (const std::exception& e) {
        std::cerr << "❌ Tactical test failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 