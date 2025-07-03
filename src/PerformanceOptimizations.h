#ifndef PERFORMANCE_OPTIMIZATIONS_H
#define PERFORMANCE_OPTIMIZATIONS_H

#include "ChessBoard.h"
#include "search.h"
#include <vector>
#include <array>
#ifdef __x86_64__
#include <immintrin.h> // For SIMD instructions
#endif
#include <thread>
#include <future>
#include <atomic>

// Performance optimizations for chess engine
class PerformanceOptimizations {
public:
    // SIMD-optimized bitboard operations
    static uint64_t bitboardAND_SIMD(uint64_t a, uint64_t b);
    static uint64_t bitboardOR_SIMD(uint64_t a, uint64_t b);
    static uint64_t bitboardXOR_SIMD(uint64_t a, uint64_t b);
    static uint64_t bitboardNOT_SIMD(uint64_t a);
    static int bitboardPopCount_SIMD(uint64_t bb);
    static int bitboardLSB_SIMD(uint64_t bb);
    static int bitboardMSB_SIMD(uint64_t bb);
    
    // Cache-optimized board representation
    static void optimizeBoardLayout(Board& board);
    static void prefetchBoard(const Board& board);
    static void alignBoardData(Board& board);
    
    // Parallel move generation
    static std::vector<std::pair<int, int>> generateMovesParallel(const Board& board, ChessPieceColor color, int numThreads);
    static std::vector<std::pair<int, int>> generateCapturesParallel(const Board& board, ChessPieceColor color, int numThreads);
    static std::vector<std::pair<int, int>> generateQuietMovesParallel(const Board& board, ChessPieceColor color, int numThreads);
    
    // Parallel evaluation
    static int evaluatePositionParallel(const Board& board, int numThreads);
    static int evaluateMaterialParallel(const Board& board, int numThreads);
    static int evaluatePositionalParallel(const Board& board, int numThreads);
    
    // Memory pool for move lists
    class MovePool {
    public:
        MovePool(size_t initialSize = 1000);
        ~MovePool();
        
        std::vector<std::pair<int, int>>* allocate();
        void deallocate(std::vector<std::pair<int, int>>* moves);
        void clear();
        
    private:
        std::vector<std::vector<std::pair<int, int>>*> pool;
        std::vector<std::vector<std::pair<int, int>>*> freeList;
        std::mutex poolMutex;
    };
    
    // Lock-free transposition table
    class LockFreeTT {
    public:
        struct Entry {
            uint64_t key;
            int depth;
            int value;
            int flag;
            std::pair<int, int> bestMove;
            std::atomic<int> age;
        };
        
        LockFreeTT(size_t size = 1000000);
        ~LockFreeTT();
        
        void store(uint64_t key, int depth, int value, int flag, const std::pair<int, int>& bestMove);
        bool probe(uint64_t key, int& depth, int& value, int& flag, std::pair<int, int>& bestMove);
        void clear();
        
    private:
        std::vector<Entry> table;
        std::atomic<int> currentAge;
        size_t mask;
    };
    
    // Work-stealing thread pool
    class WorkStealingPool {
    public:
        struct Task {
            std::function<void()> func;
            int priority;
            Task(std::function<void()> f, int p = 0) : func(f), priority(p) {}
        };
        
        WorkStealingPool(int numThreads = 0);
        ~WorkStealingPool();
        
        void submit(std::function<void()> task, int priority = 0);
        void waitForAll();
        void shutdown();
        
    private:
        struct ThreadData {
            std::queue<Task> localQueue;
            std::mutex queueMutex;
            std::condition_variable cv;
            bool shutdown;
        };
        
        std::vector<std::thread> threads;
        std::vector<std::unique_ptr<ThreadData>> threadData;
        std::atomic<bool> globalShutdown;
        
        void workerThread(int threadId);
        bool stealWork(int threadId, std::function<void()>& task);
    };
    
    // SIMD-optimized evaluation
    class SIMDEvaluation {
    public:
        // SIMD-optimized piece-square table lookup
        static uint64_t getPieceSquareValues_SIMD(const Board& board, ChessPieceType pieceType, ChessPieceColor color);
        
        // SIMD-optimized material counting
        static uint64_t countMaterial_SIMD(const Board& board);
        
        // SIMD-optimized mobility calculation
        static uint64_t calculateMobility_SIMD(const Board& board, ChessPieceColor color);
        
        // SIMD-optimized attack detection
        static uint64_t detectAttacks_SIMD(const Board& board, int square, ChessPieceColor color);
        
    private:
        // Pre-computed SIMD constants
        static const uint64_t PIECE_VALUES;
        static const uint64_t MOBILITY_WEIGHTS;
        static const uint64_t ATTACK_WEIGHTS;
    };
    
    // Cache-friendly data structures
    class CacheOptimizedBoard {
    public:
        // Aligned board representation
        alignas(64) struct AlignedBoard {
            uint64_t bitboards[12];  // 12 piece types
            uint8_t squares[64];     // Square occupancy
            uint8_t colors[64];      // Piece colors
            uint64_t zobristKey;     // Position hash
            uint8_t turn;            // Side to move
            uint8_t castlingRights;  // Castling availability
            uint8_t enPassantFile;   // En passant file
            uint8_t moveCount;       // Move counter
        };
        
        static void convertToAligned(const Board& board, AlignedBoard& aligned);
        static void convertFromAligned(const AlignedBoard& aligned, Board& board);
        static void prefetchAligned(const AlignedBoard& aligned);
        
    private:
        static constexpr size_t CACHE_LINE_SIZE = 64;
    };
    
    // Branch prediction hints
    class BranchPrediction {
    public:
        static bool likely(bool condition);
        static bool unlikely(bool condition);
        static void prefetch(const void* ptr);
        static void prefetchWrite(const void* ptr);
        
    private:
        #if defined(__GNUC__) || defined(__clang__)
            #define LIKELY(x) __builtin_expect(!!(x), 1)
            #define UNLIKELY(x) __builtin_expect(!!(x), 0)
        #else
            #define LIKELY(x) (x)
            #define UNLIKELY(x) (x)
        #endif
    };
    
    // Performance monitoring
    class PerformanceMonitor {
    public:
        struct Metrics {
            uint64_t nodesSearched;
            uint64_t transpositionHits;
            uint64_t cacheMisses;
            uint64_t branchMispredictions;
            double averageBranchPredictionAccuracy;
            double cacheHitRate;
            double nodesPerSecond;
        };
        
        PerformanceMonitor();
        ~PerformanceMonitor();
        
        void startMeasurement();
        void endMeasurement();
        void recordNode();
        void recordTTHit();
        void recordCacheMiss();
        void recordBranchMisprediction();
        
        Metrics getMetrics() const;
        void reset();
        void printReport() const;
        
    private:
        Metrics metrics;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
    };
};

// Compile-time optimizations
namespace CompileTimeOptimizations {
    // Compile-time bitboard operations
    template<uint64_t BB>
    constexpr int popCount() {
        return (BB & 1) + popCount<(BB >> 1)>();
    }
    
    template<>
    constexpr int popCount<0>() {
        return 0;
    }
    
    // Compile-time piece-square table generation
    template<int Square, ChessPieceType Piece>
    constexpr int getPieceSquareValue() {
        // Compile-time piece-square table lookup
        return 0; // Placeholder
    }
    
    // Compile-time move generation
    template<uint64_t FromBB, uint64_t ToBB>
    constexpr uint64_t generateMoves() {
        return FromBB & ToBB;
    }
}

#endif // PERFORMANCE_OPTIMIZATIONS_H 