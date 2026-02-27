#pragma once

#include "../core/ChessBoard.h"
#include "../evaluation/Evaluation.h"
#include "TranspositionTableV2.h"
#include "ValidMoves.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace SearchConstants {
constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kTwo = 2;
constexpr int kThree = 3;
constexpr int kFour = 4;
constexpr int kInvalidSquare = -1;
constexpr int kDefaultTranspositionTableMb = 32;
constexpr int kBoardSquareCount = 64;
constexpr int kPieceTypeCount = 6;
constexpr int kPieceTypeOffset = 1;
constexpr int kContinuationDecayDivisor = 16384;
constexpr std::size_t kMoveMaskDimension = 64ULL;
constexpr std::size_t kMoveMaskSize = kMoveMaskDimension * kMoveMaskDimension;
} // namespace SearchConstants

std::string getFEN(const Board& board);
bool parseAlgebraicMove(std::string_view move, Board& board, int& srcCol, int& srcRow, int& destCol,
                        int& destRow);

struct TTEntry {
    int depth;
    int value;
    int flag;
    std::pair<int, int> bestMove;
    uint64_t zobristKey;

    TTEntry()
        : depth(SearchConstants::kInvalidSquare), value(SearchConstants::kZero),
          flag(SearchConstants::kZero),
          bestMove({SearchConstants::kInvalidSquare, SearchConstants::kInvalidSquare}),
          zobristKey(SearchConstants::kZero) {}
    TTEntry(int d, int v, int f,
            std::pair<int, int> move = {SearchConstants::kInvalidSquare,
                                        SearchConstants::kInvalidSquare},
            uint64_t key = SearchConstants::kZero)
        : depth(d), value(v), flag(f), bestMove(move), zobristKey(key) {}
};

class TranspositionTableAdapter {
    TTv2::TranspositionTable tt;

    static TTv2::Bound flagToBound(int flag) {
        if (flag == SearchConstants::kZero) {
            return TTv2::BOUND_EXACT;
        }
        if (flag == SearchConstants::kInvalidSquare) {
            return TTv2::BOUND_UPPER;
        }
        if (flag == SearchConstants::kOne) {
            return TTv2::BOUND_LOWER;
        }
        return TTv2::BOUND_NONE;
    }

    static int boundToFlag(TTv2::Bound b) {
        if (b == TTv2::BOUND_EXACT) {
            return SearchConstants::kZero;
        }
        if (b == TTv2::BOUND_UPPER) {
            return SearchConstants::kInvalidSquare;
        }
        if (b == TTv2::BOUND_LOWER) {
            return SearchConstants::kOne;
        }
        return SearchConstants::kZero;
    }

public:
    TranspositionTableAdapter() {
        tt.resize(SearchConstants::kDefaultTranspositionTableMb);
    }

    void insert(uint64_t hash, const TTEntry& entry) {
        bool found = false;
        TTv2::TTEntry* tte{tt.probe(hash, found)};
        TTv2::PackedMove pm = TTv2::packMove(
            entry.bestMove.first >= SearchConstants::kZero ? entry.bestMove.first
                                                           : SearchConstants::kZero,
            entry.bestMove.second >= SearchConstants::kZero ? entry.bestMove.second
                                                            : SearchConstants::kZero);
        if (entry.bestMove.first < SearchConstants::kZero) {
            pm = SearchConstants::kZero;
        }
        tte->save(hash, static_cast<int16_t>(entry.value), static_cast<int16_t>(entry.value),
                  flagToBound(entry.flag),
                  static_cast<int8_t>(
                      std::clamp(entry.depth, static_cast<int>(std::numeric_limits<int8_t>::min()),
                                 static_cast<int>(std::numeric_limits<int8_t>::max()))),
                  pm, tt.generation());
    }

    bool find(uint64_t hash, TTEntry& entry) const {
        bool found = false;
        TTv2::TTEntry* tte{const_cast<TTv2::TranspositionTable&>(tt).probe(hash, found)};
        if (!found) {
            return false;
        }
        entry.depth = static_cast<int>(static_cast<unsigned char>(tte->depth));
        entry.value = tte->value;
        entry.flag = boundToFlag(tte->bound());
        if (tte->move) {
            int from = SearchConstants::kZero;
            int to = SearchConstants::kZero;
            int promo = SearchConstants::kZero;
            TTv2::unpackMove(tte->move, from, to, promo);
            entry.bestMove = {from, to};
        } else {
            entry.bestMove = {SearchConstants::kInvalidSquare, SearchConstants::kInvalidSquare};
        }
        entry.zobristKey = hash;
        return true;
    }

    void clear() {
        tt.clear();
    }

    void resize(size_t mbSize) {
        tt.resize(mbSize);
    }

    void newSearch() {
        tt.newSearch();
    }

    void prefetch(uint64_t key) const {
        tt.prefetch(key);
    }

    int hashfull() const {
        return tt.hashfull();
    }
};

struct ThreadSafeHistory {
    std::array<int, SearchConstants::kMoveMaskSize> table{};
    ThreadSafeHistory();
    void update(int srcPos, int destPos, int bonus);
    int get(int srcPos, int destPos) const;
    int getScore(int srcPos, int destPos) const {
        return get(srcPos, destPos);
    }
    void updateScore(int srcPos, int destPos, int score) {
        update(srcPos, destPos, score);
    }

private:
    static constexpr int index(int srcPos, int destPos) {
        return (srcPos * SearchConstants::kBoardSquareCount) + destPos;
    }
};

struct KillerMoves {
    static const int MAX_KILLER_MOVES = SearchConstants::kTwo;
    static const int MAX_PLY = SearchConstants::kBoardSquareCount;
    std::pair<int, int> killers[MAX_PLY][MAX_KILLER_MOVES];
    KillerMoves();
    void store(int ply, std::pair<int, int> move);
    bool isKiller(int ply, std::pair<int, int> move) const;
    int getKillerScore(int ply, std::pair<int, int> move) const;
};

struct ParallelSearchContext {
    std::atomic<bool> stopSearch;
    std::atomic<int> nodeCount;
    TranspositionTableAdapter transTable;
    ThreadSafeHistory historyTable;
    KillerMoves killerMoves;
    std::chrono::steady_clock::time_point startTime;
    int timeLimitMs;
    int numThreads;
    int ply;
    std::pair<int, int> counterMoves[SearchConstants::kTwo][SearchConstants::kBoardSquareCount];
    int contempt = SearchConstants::kZero;
    int multiPV = SearchConstants::kOne;
    std::vector<std::pair<int, int>> excludedRootMoves;
    int optimalTimeMs = SearchConstants::kZero;
    int maxTimeMs = SearchConstants::kZero;
    std::atomic<int> tbHits{SearchConstants::kZero};
    bool useSyzygy = false;

    int continuationHistory[SearchConstants::kPieceTypeCount][SearchConstants::kBoardSquareCount]
                           [SearchConstants::kPieceTypeCount][SearchConstants::kBoardSquareCount];
    int captureHistory[SearchConstants::kPieceTypeCount][SearchConstants::kPieceTypeCount]
                      [SearchConstants::kBoardSquareCount];
    int prevPieceAtPly[SearchConstants::kBoardSquareCount];
    int prevToAtPly[SearchConstants::kBoardSquareCount];
    ParallelSearchContext(int threads = SearchConstants::kZero);

    void updateContinuationHistory(int prevPiece, int prevTo, int piece, int to, int bonus) {
        if (prevPiece < SearchConstants::kZero || prevPiece >= SearchConstants::kPieceTypeCount ||
            prevTo < SearchConstants::kZero || prevTo >= SearchConstants::kBoardSquareCount) {
            return;
        }
        if (piece < SearchConstants::kZero || piece >= SearchConstants::kPieceTypeCount ||
            to < SearchConstants::kZero || to >= SearchConstants::kBoardSquareCount) {
            return;
        }
        int& entry = continuationHistory[prevPiece][prevTo][piece][to];
        entry += bonus - (entry * std::abs(bonus) / SearchConstants::kContinuationDecayDivisor);
    }

    int getContinuationHistory(int prevPiece, int prevTo, int piece, int to) const {
        if (prevPiece < SearchConstants::kZero || prevPiece >= SearchConstants::kPieceTypeCount ||
            prevTo < SearchConstants::kZero || prevTo >= SearchConstants::kBoardSquareCount) {
            return SearchConstants::kZero;
        }
        if (piece < SearchConstants::kZero || piece >= SearchConstants::kPieceTypeCount ||
            to < SearchConstants::kZero || to >= SearchConstants::kBoardSquareCount) {
            return SearchConstants::kZero;
        }
        return continuationHistory[prevPiece][prevTo][piece][to];
    }

    void updateCaptureHistory(int attacker, int victim, int to, int bonus) {
        if (attacker < SearchConstants::kZero || attacker >= SearchConstants::kPieceTypeCount ||
            victim < SearchConstants::kZero || victim >= SearchConstants::kPieceTypeCount) {
            return;
        }
        if (to < SearchConstants::kZero || to >= SearchConstants::kBoardSquareCount) {
            return;
        }
        int& entry = captureHistory[attacker][victim][to];
        entry += bonus - (entry * std::abs(bonus) / SearchConstants::kContinuationDecayDivisor);
    }

    int getCaptureHistory(int attacker, int victim, int to) const {
        if (attacker < SearchConstants::kZero || attacker >= SearchConstants::kPieceTypeCount ||
            victim < SearchConstants::kZero || victim >= SearchConstants::kPieceTypeCount) {
            return SearchConstants::kZero;
        }
        if (to < SearchConstants::kZero || to >= SearchConstants::kBoardSquareCount) {
            return SearchConstants::kZero;
        }
        return captureHistory[attacker][victim][to];
    }
};

struct ScoredMove {
    std::pair<int, int> move;
    int score;
    ScoredMove(std::pair<int, int> m, int s) : move(m), score(s) {}
    bool operator<(const ScoredMove& other) const {
        return score < other.score;
    }
    bool operator>(const ScoredMove& other) const {
        return score > other.score;
    }
};

struct SearchResult {
    std::pair<int, int> bestMove;
    int score;
    int depth;
    int nodes;
    int timeMs;
    SearchResult();
};

extern uint64_t ZobristTable[64][12];
extern uint64_t ZobristBlackToMove;
extern uint64_t ZobristCastling[4];
extern uint64_t ZobristEnPassant[64];
extern TranspositionTableAdapter TransTable;

void InitZobrist();
int pieceToZobristIndex(const Piece& p);
uint64_t ComputeZobrist(const Board& board);

bool isCapture(const Board& board, int srcPos, int destPos);
bool applySearchMove(Board& board, int fromSquare, int toSquare,
                     bool autoPromoteToQueen = true);

inline int pieceTypeIndex(ChessPieceType pt) {
    switch (pt) {
        case ChessPieceType::PAWN:
            return SearchConstants::kZero;
        case ChessPieceType::KNIGHT:
            return SearchConstants::kOne;
        case ChessPieceType::BISHOP:
            return SearchConstants::kTwo;
        case ChessPieceType::ROOK:
            return SearchConstants::kThree;
        case ChessPieceType::QUEEN:
            return SearchConstants::kFour;
        case ChessPieceType::KING:
            return SearchConstants::kPieceTypeCount - SearchConstants::kOne;
        default:
            return SearchConstants::kInvalidSquare;
    }
}
bool givesCheck(const Board& board, int srcPos, int destPos);
bool isInCheck(const Board& board, ChessPieceColor color);
void updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth);
bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs);
int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply,
                    ThreadSafeHistory& historyTable, ParallelSearchContext& context,
                    uint64_t zobristKey = std::numeric_limits<uint64_t>::max());
int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply,
                     uint64_t zobristKey = std::numeric_limits<uint64_t>::max());
std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color);
std::string getBookMove(const std::string& fen);
SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs,
                                        int numThreads = SearchConstants::kZero,
                                        int contempt = SearchConstants::kZero,
                                        int multiPV = SearchConstants::kOne,
                                        int optimalTimeMs = SearchConstants::kZero,
                                        int maxTimeMs = SearchConstants::kZero,
                                        int hashSizeMb =
                                            SearchConstants::kDefaultTranspositionTableMb);
std::pair<int, int> findBestMove(Board& board, int depth);
int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer,
                             int ply, ThreadSafeHistory& historyTable,
                             ParallelSearchContext& context, bool isPVNode = true,
                             uint64_t zobristKey = std::numeric_limits<uint64_t>::max());

int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare);

bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
std::vector<ScoredMove> scoreMovesOptimized(
    const Board& board, const std::vector<std::pair<int, int>>& moves,
    const ThreadSafeHistory& historyTable, const KillerMoves& killerMoves, int ply,
    const std::pair<int, int>& ttMove = {SearchConstants::kInvalidSquare,
                                         SearchConstants::kInvalidSquare},
    const std::pair<int, int>& counterMove = {SearchConstants::kInvalidSquare,
                                              SearchConstants::kInvalidSquare},
    const ParallelSearchContext* context = nullptr);

enum class MovePickerStage : std::uint8_t {
    HASH_MOVE,
    GEN_CAPTURES,
    GOOD_CAPTURES,
    KILLERS,
    COUNTERMOVE,
    GEN_QUIETS,
    QUIETS,
    BAD_CAPTURES,
    DONE
};

class MovePicker {
    Board& board;
    std::pair<int, int> hashMove;
    const KillerMoves& killerMoves;
    int ply;
    const ThreadSafeHistory& history;
    ParallelSearchContext& context;
    MovePickerStage stage;
    std::vector<ScoredMove> goodCaptures;
    std::vector<ScoredMove> badCaptures;
    std::vector<ScoredMove> quietMoves;
    size_t captureIdx = 0;
    size_t quietIdx = 0;
    size_t badCaptureIdx = 0;
    bool movesGenerated = false;
    static constexpr std::size_t MOVE_MASK_SIZE = SearchConstants::kMoveMaskSize;
    std::array<bool, MOVE_MASK_SIZE> returnedMask{};

    static constexpr std::size_t moveIndex(const std::pair<int, int>& m) {
        return (static_cast<std::size_t>(m.first) *
                static_cast<std::size_t>(SearchConstants::kMoveMaskDimension)) +
               static_cast<std::size_t>(m.second);
    }

    bool alreadyReturned(const std::pair<int, int>& m) const {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return false;
        }
        return returnedMask[moveIndex(m)];
    }

    void markReturned(const std::pair<int, int>& m) {
        if (m.first < SearchConstants::kZero || m.first >= SearchConstants::kBoardSquareCount ||
            m.second < SearchConstants::kZero || m.second >= SearchConstants::kBoardSquareCount) {
            return;
        }
        returnedMask[moveIndex(m)] = true;
    }

    void generateAndPartitionMoves();

public:
    MovePicker(Board& b, std::pair<int, int> hm, const KillerMoves& km, int p,
               const ThreadSafeHistory& h, ParallelSearchContext& ctx);

    std::pair<int, int> next();
};

struct SearchTechniques {
    static const int FUTILITY_MARGIN_PAWN = 100;
    static const int FUTILITY_MARGIN_KNIGHT = 300;
    static const int FUTILITY_MARGIN_BISHOP = 300;
    static const int FUTILITY_MARGIN_ROOK = 500;
    static const int FUTILITY_MARGIN_QUEEN = 900;
    static const int NULL_MOVE_R = 3;
    static const int NULL_MOVE_MARGIN = 300;
    static const int LMR_MIN_DEPTH = 3;
    static const int LMR_MIN_MOVE = 4;
    static const int STATIC_NULL_MARGIN = 900;
    static const int IID_MIN_DEPTH = 4;
};

namespace EnhancedMoveOrdering {
static const int HASH_MOVE_SCORE = 1000000;
static const int CAPTURE_SCORE_BASE = 900000;
static const int KILLER_SCORE = 800000;
static const int HISTORY_SCORE_BASE = 0;
static const int QUIET_SCORE_BASE = -1000000;

extern const int MVV_LVA_SCORES[6][6];

int getMVVLVA_Score(const Board& board, int fromSquare, int toSquare);
int getHistoryScore(const ThreadSafeHistory& history, int fromSquare, int toSquare);
int getKillerScore(const KillerMoves& killers, int ply, int fromSquare, int toSquare);
int getPositionalScore(const Board& board, int fromSquare, int toSquare);
} // namespace EnhancedMoveOrdering
