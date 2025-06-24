#include "../src/ChessBoard.h"
#include "../src/search.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

using namespace std::chrono;

struct PerftResult {
    uint64_t nodes;
    duration<double> time;
    double nps; // nodes per second
};

class PerftBenchmark {
public:
    static PerftResult runPerft(Board& board, int depth) {
        auto start = high_resolution_clock::now();
        uint64_t nodes = perft(board, depth);
        auto end = high_resolution_clock::now();
        
        duration<double> time = end - start;
        double nps = nodes / time.count();
        
        return {nodes, time, nps};
    }
    
    static uint64_t perft(Board& board, int depth) {
        if (depth == 0) return 1;
        
        uint64_t nodes = 0;
        
        
        for (int from = 0; from < 64; from++) {
            for (int to = 0; to < 64; to++) {
                if (isValidMove(board, from, to)) {
                    Board backup = board;
                    makeMove(board, from, to);
                    
                    nodes += perft(board, depth - 1);
                    
                    board = backup;
                }
            }
        }
        
        return nodes;
    }
    
private:
    static bool isValidMove(const Board& board, int from, int to) {
        if (from == to) return false;
        if (board.squares[from].Piece.PieceType == ChessPieceType::NONE) return false;
        return true;
    }
    
    static void makeMove(Board& board, int from, int to) {
        board.squares[to] = board.squares[from];
        board.squares[from].Piece = {ChessPieceType::NONE, ChessPieceColor::WHITE};
        board.turn = (board.turn == ChessPieceColor::WHITE) ? 
                     ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    }
};

class SearchBenchmark {
public:
    struct SearchResult {
        int bestMove;
        int score;
        uint64_t nodes;
        duration<double> time;
    };
    
    static SearchResult runSearchBenchmark(Board& board, int depth, int timeMs) {
        auto start = high_resolution_clock::now();
        
        auto result = iterativeDeepeningParallel(board, depth, timeMs, 1);
        
        auto end = high_resolution_clock::now();
        duration<double> time = end - start;
        
        return {
            result.bestMove.first * 8 + result.bestMove.second,
            result.score,
            result.nodes,
            time
        };
    }
};

void runBenchmarkSuite() {
    std::cout << "🚀 Chess Engine Performance Benchmark Suite\n";
    std::cout << "=============================================\n\n";
    
    std::vector<std::pair<std::string, std::string>> testPositions = {
        {"Starting Position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
        {"Position 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"},
        {"Position 4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"}
    };
    
    Board testBoard;
    InitializeBoard(testBoard);
    
    std::cout << "📊 PERFT (Move Generation) Tests:\n";
    std::cout << "Position                 | Depth | Nodes      | Time(s) | NPS        \n";
    std::cout << "-------------------------|-------|------------|---------|------------\n";
    
    for (const auto& [name, fen] : testPositions) {
        InitializeBoard(testBoard);
        
        for (int depth = 1; depth <= 4; depth++) {
            auto result = PerftBenchmark::runPerft(testBoard, depth);
            
            std::cout << std::left << std::setw(24) << name.substr(0, 23)
                      << "| " << std::setw(5) << depth
                      << "| " << std::setw(10) << result.nodes
                      << "| " << std::setw(7) << std::fixed << std::setprecision(3) << result.time.count()
                      << "| " << std::setw(10) << std::scientific << std::setprecision(2) << result.nps
                      << "\n";
        }
        std::cout << "-------------------------|-------|------------|---------|------------\n";
    }
    
    std::cout << "\n🧠 Search Performance Tests:\n";
    std::cout << "Position           | Depth | Score | Nodes    | Time(s) | Best Move\n";
    std::cout << "-------------------|-------|-------|----------|---------|----------\n";
    
    for (const auto& [name, fen] : testPositions) {
        InitializeBoard(testBoard);
        
        auto result = SearchBenchmark::runSearchBenchmark(testBoard, 6, 5000);
        
        std::cout << std::left << std::setw(18) << name.substr(0, 17)
                  << "| " << std::setw(5) << 6
                  << "| " << std::setw(5) << result.score
                  << "| " << std::setw(8) << result.nodes
                  << "| " << std::setw(7) << std::fixed << std::setprecision(3) << result.time.count()
                  << "| " << std::setw(8) << result.bestMove
                  << "\n";
    }
    
    std::cout << "\n✅ Benchmark suite completed!\n";
}

int main() {
    try {
        runBenchmarkSuite();
    } catch (const std::exception& e) {
        std::cerr << "❌ Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 