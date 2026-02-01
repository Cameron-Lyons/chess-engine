#include "search.h"
#include "../utils/engine_globals.h"
#include "AdvancedSearch.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <thread>

void ThreadSafeTT::insert(uint64_t hash, const TTEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = table.find(hash);
    if (it != table.end()) {
        const TTEntry& existing = it->second;

        if (existing.zobristKey != 0 && existing.zobristKey != entry.zobristKey) {
            int replaceScore = 0;

            replaceScore += (entry.depth - existing.depth) * 4;

            if (entry.flag == 0)
                replaceScore += 2;
            if (existing.flag == 0)
                replaceScore -= 2;

            if (replaceScore > 0) {
                TTEntry newEntry = entry;
                newEntry.zobristKey = entry.zobristKey;
                table[hash] = newEntry;
            }
            return;
        }

        bool shouldReplace = false;

        if (entry.depth > existing.depth + 1) {
            shouldReplace = true;
        } else if (entry.depth == existing.depth && entry.flag == 0 && existing.flag != 0) {
            shouldReplace = true;
        } else if (entry.depth >= existing.depth) {
            shouldReplace = true;
        }

        if (shouldReplace) {
            TTEntry newEntry = entry;
            newEntry.zobristKey = entry.zobristKey;
            table[hash] = newEntry;
        }
    } else {
        const size_t MAX_TABLE_SIZE = 500000;
        if (table.size() >= MAX_TABLE_SIZE) {
            int minDepth = 100;
            auto victimIt = table.begin();

            auto checkIt = table.begin();
            for (int i = 0; i < 100 && checkIt != table.end(); ++i, ++checkIt) {
                if (checkIt->second.depth < minDepth) {
                    minDepth = checkIt->second.depth;
                    victimIt = checkIt;
                }
            }

            table.erase(victimIt);
        }
        TTEntry newEntry = entry;
        newEntry.zobristKey = entry.zobristKey;
        table[hash] = newEntry;
    }
}

bool ThreadSafeTT::find(uint64_t hash, TTEntry& entry) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = table.find(hash);
    if (it != table.end()) {
        entry = it->second;
        if (entry.zobristKey != 0 && entry.zobristKey != hash) {
            return false;
        }
        return true;
    }
    return false;
}

void ThreadSafeTT::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    table.clear();
}

ThreadSafeHistory::ThreadSafeHistory() : table(64, std::vector<int>(64, 0)) {}
void ThreadSafeHistory::update(int srcPos, int destPos, int depth) {
    std::lock_guard<std::mutex> lock(mutex);
    table[srcPos][destPos] += depth * depth;
}
int ThreadSafeHistory::get(int srcPos, int destPos) const {
    std::lock_guard<std::mutex> lock(mutex);
    return table[srcPos][destPos];
}

KillerMoves::KillerMoves() {
    for (int ply = 0; ply < MAX_PLY; ply++) {
        for (int slot = 0; slot < MAX_KILLER_MOVES; slot++) {
            killers[ply][slot] = {-1, -1};
        }
    }
}

void KillerMoves::store(int ply, std::pair<int, int> move) {
    if (ply < 0 || ply >= MAX_PLY)
        return;

    if (isKiller(ply, move))
        return;

    for (int i = MAX_KILLER_MOVES - 1; i > 0; i--) {
        killers[ply][i] = killers[ply][i - 1];
    }
    killers[ply][0] = move;
}

bool KillerMoves::isKiller(int ply, std::pair<int, int> move) const {
    if (ply < 0 || ply >= MAX_PLY)
        return false;

    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return true;
        }
    }
    return false;
}

int KillerMoves::getKillerScore(int ply, std::pair<int, int> move) const {
    if (ply < 0 || ply >= MAX_PLY)
        return 0;

    for (int i = 0; i < MAX_KILLER_MOVES; i++) {
        if (killers[ply][i] == move) {
            return 5000 - i * 100;
        }
    }
    return 0;
}

ParallelSearchContext::ParallelSearchContext(int threads)
    : stopSearch(false), nodeCount(0), numThreads(threads), ply(0), contempt(0), multiPV(1) {
    if (numThreads == 0)
        numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0)
        numThreads = 4;
    for (int c = 0; c < 2; ++c)
        for (int sq = 0; sq < 64; ++sq)
            counterMoves[c][sq] = {-1, -1};
}

SearchResult::SearchResult() : bestMove({-1, -1}), score(0), depth(0), nodes(0), timeMs(0) {}

void InitZobrist() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;
    std::mt19937_64 rng(202406);
    std::uniform_int_distribution<uint64_t> dist;
    for (int sq = 0; sq < 64; ++sq) {
        for (int pt = 0; pt < 12; ++pt) {
            ZobristTable[sq][pt] = dist(rng);
        }
    }
    ZobristBlackToMove = dist(rng);
}

int pieceToZobristIndex(const Piece& p) {
    if (p.PieceType == ChessPieceType::NONE)
        return -1;
    return (static_cast<int>(p.PieceType) - 1) + (p.PieceColor == ChessPieceColor::BLACK ? 6 : 0);
}

uint64_t ComputeZobrist(const Board& board) {
    uint64_t h = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int idx = pieceToZobristIndex(board.squares[sq].piece);
        if (idx >= 0)
            h ^= ZobristTable[sq][idx];
    }
    if (board.turn == ChessPieceColor::BLACK)
        h ^= ZobristBlackToMove;
    return h;
}

int getMVVLVA_Score(const Board& board, int srcPos, int destPos) {
    const Piece& attacker = board.squares[srcPos].piece;
    const Piece& victim = board.squares[destPos].piece;
    if (victim.PieceType == ChessPieceType::NONE)
        return 0;
    return victim.PieceValue * 10 - attacker.PieceValue;
}

bool isCapture(const Board& board, int, int destPos) {
    return board.squares[destPos].piece.PieceType != ChessPieceType::NONE;
}

bool givesCheck(const Board& board, int srcPos, int destPos) {
    Board tempBoard = board;
    tempBoard.movePiece(srcPos, destPos);
    tempBoard.updateBitboards();
    ChessPieceColor kingColor =
        (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK : ChessPieceColor::WHITE;
    return IsKingInCheck(tempBoard, kingColor);
}

bool isInCheck(const Board& board, ChessPieceColor color) {
    return IsKingInCheck(board, color);
}

int getHistoryScore(const ThreadSafeHistory& historyTable, int srcPos, int destPos) {
    return historyTable.get(srcPos, destPos);
}

int getPieceValue(ChessPieceType pieceType) {
    switch (pieceType) {
        case ChessPieceType::PAWN:
            return 100;
        case ChessPieceType::KNIGHT:
            return 300;
        case ChessPieceType::BISHOP:
            return 300;
        case ChessPieceType::ROOK:
            return 500;
        case ChessPieceType::QUEEN:
            return 975;
        case ChessPieceType::KING:
            return 10000;
        case ChessPieceType::NONE:
            return 0;
        default:
            return 0;
    }
}

std::vector<ScoredMove> scoreMovesOptimized(const Board& board,
                                            const std::vector<std::pair<int, int>>& moves,
                                            const ThreadSafeHistory& historyTable,
                                            const KillerMoves& killerMoves, int ply,
                                            const std::pair<int, int>& ttMove,
                                            const std::pair<int, int>& counterMove) {
    std::vector<ScoredMove> scoredMoves;
    scoredMoves.reserve(moves.size());

    for (const auto& move : moves) {
        int score = 0;

        if (move == ttMove) {
            score = EnhancedMoveOrdering::HASH_MOVE_SCORE;
        }

        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            score = EnhancedMoveOrdering::CAPTURE_SCORE_BASE + mvvLva * 1000 + see;
        }

        else if (killerMoves.isKiller(ply, move)) {
            score = EnhancedMoveOrdering::KILLER_SCORE +
                    EnhancedMoveOrdering::getKillerScore(killerMoves, ply, move.first, move.second);
        }

        else if (counterMove.first >= 0 && move == counterMove) {
            score = 850000;
        }

        else {
            score = EnhancedMoveOrdering::HISTORY_SCORE_BASE +
                    EnhancedMoveOrdering::getHistoryScore(historyTable, move.first, move.second);

            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
        }

        scoredMoves.push_back({move, score});
    }

    return scoredMoves;
}

MovePicker::MovePicker(Board& b, std::pair<int, int> hm, const KillerMoves& km,
                       int p, const ThreadSafeHistory& h, ParallelSearchContext& ctx)
    : board(b), hashMove(hm), killerMoves(km), ply(p), history(h), context(ctx),
      stage(MovePickerStage::HASH_MOVE) {}

void MovePicker::generateAndPartitionMoves() {
    if (movesGenerated) return;
    movesGenerated = true;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    for (const auto& move : moves) {
        if (move == hashMove) continue;
        if (killerMoves.isKiller(ply, move)) continue;

        int colorIdx = (board.turn == ChessPieceColor::WHITE) ? 0 : 1;
        int prevDest = board.LastMove;
        std::pair<int, int> cm = {-1, -1};
        if (prevDest >= 0 && prevDest < 64)
            cm = context.counterMoves[colorIdx][prevDest];
        if (cm.first >= 0 && move == cm) continue;

        bool isCap = isCapture(board, move.first, move.second);
        if (isCap) {
            int mvvLva = EnhancedMoveOrdering::getMVVLVA_Score(board, move.first, move.second);
            int see = staticExchangeEvaluation(board, move.first, move.second);
            int score = mvvLva * 1000 + see;
            if (see >= 0) {
                goodCaptures.push_back({move, score});
            } else {
                badCaptures.push_back({move, score});
            }
        } else {
            int score = EnhancedMoveOrdering::getHistoryScore(history, move.first, move.second);
            score += EnhancedMoveOrdering::getPositionalScore(board, move.first, move.second);
            quietMoves.push_back({move, score});
        }
    }

    std::sort(goodCaptures.begin(), goodCaptures.end(), std::greater<ScoredMove>());
    std::sort(quietMoves.begin(), quietMoves.end(), std::greater<ScoredMove>());
    std::sort(badCaptures.begin(), badCaptures.end(), std::greater<ScoredMove>());
}

std::pair<int, int> MovePicker::next() {
    while (true) {
        switch (stage) {
        case MovePickerStage::HASH_MOVE:
            stage = MovePickerStage::GEN_CAPTURES;
            if (hashMove.first >= 0 && hashMove.second >= 0) {
                returned.push_back(hashMove);
                return hashMove;
            }
            break;

        case MovePickerStage::GEN_CAPTURES:
            generateAndPartitionMoves();
            captureIdx = 0;
            stage = MovePickerStage::GOOD_CAPTURES;
            break;

        case MovePickerStage::GOOD_CAPTURES:
            if (captureIdx < goodCaptures.size()) {
                auto m = goodCaptures[captureIdx++].move;
                if (!alreadyReturned(m)) {
                    returned.push_back(m);
                    return m;
                }
                break;
            }
            stage = MovePickerStage::KILLERS;
            break;

        case MovePickerStage::KILLERS:
            stage = MovePickerStage::COUNTERMOVE;
            for (int i = 0; i < KillerMoves::MAX_KILLER_MOVES; ++i) {
                auto k = killerMoves.killers[ply][i];
                if (k.first >= 0 && !alreadyReturned(k)) {
                    returned.push_back(k);
                    stage = MovePickerStage::KILLERS;
                    return k;
                }
            }
            break;

        case MovePickerStage::COUNTERMOVE: {
            stage = MovePickerStage::GEN_QUIETS;
            int colorIdx = (board.turn == ChessPieceColor::WHITE) ? 0 : 1;
            int prevDest = board.LastMove;
            if (prevDest >= 0 && prevDest < 64) {
                auto cm = context.counterMoves[colorIdx][prevDest];
                if (cm.first >= 0 && !alreadyReturned(cm)) {
                    returned.push_back(cm);
                    return cm;
                }
            }
            break;
        }

        case MovePickerStage::GEN_QUIETS:
            quietIdx = 0;
            stage = MovePickerStage::QUIETS;
            break;

        case MovePickerStage::QUIETS:
            if (quietIdx < quietMoves.size()) {
                auto m = quietMoves[quietIdx++].move;
                if (!alreadyReturned(m)) {
                    returned.push_back(m);
                    return m;
                }
                break;
            }
            badCaptureIdx = 0;
            stage = MovePickerStage::BAD_CAPTURES;
            break;

        case MovePickerStage::BAD_CAPTURES:
            if (badCaptureIdx < badCaptures.size()) {
                auto m = badCaptures[badCaptureIdx++].move;
                if (!alreadyReturned(m)) {
                    returned.push_back(m);
                    return m;
                }
                break;
            }
            stage = MovePickerStage::DONE;
            break;

        case MovePickerStage::DONE:
            return {-1, -1};
        }
    }
}

void updateHistoryTable(ThreadSafeHistory& historyTable, int fromSquare, int toSquare, int depth) {
    historyTable.updateScore(fromSquare, toSquare, depth * depth);
}

bool isTimeUp(const std::chrono::steady_clock::time_point& startTime, int timeLimitMs) {
    if (timeLimitMs <= 0)
        return false;

    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return elapsed.count() >= timeLimitMs;
}

std::string getBookMove(const std::string& fen) {

    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, options.size() - 1);
            return options[dis(gen)];
        }
    }

    auto it = OpeningBook.find(fen);
    if (it != OpeningBook.end())
        return it->second;
    return "";
}

bool SearchForMate(ChessPieceColor movingSide, Board& board, bool& BlackMate, bool& WhiteMate,
                   bool& StaleMate) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    for (int depth = 1; depth <= 4; depth++) {
        int result =
            AlphaBetaSearch(board, depth, -10000, 10000, (movingSide == ChessPieceColor::WHITE), 0,
                            historyTable, context);
        if (result >= 9000) {
            if (movingSide == ChessPieceColor::WHITE) {
                BlackMate = true;
                return true;
            }
        } else if (result <= -9000) {
            if (movingSide == ChessPieceColor::BLACK) {
                WhiteMate = true;
                return true;
            }
        }
    }
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, movingSide);
    if (moves.empty()) {
        StaleMate = true;
        return true;
    }
    return false;
}

std::vector<std::pair<int, int>> GetQuietMoves(Board& board, ChessPieceColor color) {
    std::vector<std::pair<int, int>> allMoves = generateBitboardMoves(board, color);
    std::vector<std::pair<int, int>> noisyMoves;

    for (const auto& move : allMoves) {
        int srcPos = move.first;
        int destPos = move.second;

        if (isCapture(board, srcPos, destPos)) {
            noisyMoves.push_back(move);
            continue;
        }

        if (givesCheck(board, srcPos, destPos)) {
            noisyMoves.push_back(move);
            continue;
        }

        const Piece& piece = board.squares[srcPos].piece;
        if (piece.PieceType == ChessPieceType::PAWN) {
            int destRow = destPos / 8;
            if ((piece.PieceColor == ChessPieceColor::WHITE && destRow == 7) ||
                (piece.PieceColor == ChessPieceColor::BLACK && destRow == 0)) {
                noisyMoves.push_back(move);
                continue;
            }
        }
    }

    return noisyMoves;
}

int QuiescenceSearch(Board& board, int alpha, int beta, bool maximizingPlayer,
                     ThreadSafeHistory& historyTable, ParallelSearchContext& context, int ply) {
    if (context.stopSearch.load())
        return 0;

    if (ply < 0 || ply >= 50) {
        return evaluatePosition(board);
    }

    context.nodeCount.fetch_add(1);

    int standPat = evaluatePosition(board);

    if (maximizingPlayer) {
        if (standPat >= beta)
            return beta;
        alpha = std::max(alpha, standPat);
    } else {
        if (standPat <= alpha)
            return alpha;
        beta = std::min(beta, standPat);
    }

    const int DELTA_MARGIN = 975;
    const int BIG_DELTA_MARGIN = 1200;

    if (maximizingPlayer) {
        if (standPat + DELTA_MARGIN < alpha) {

            return alpha;
        }
        if (standPat + BIG_DELTA_MARGIN < alpha) {

            return standPat;
        }
    } else {
        if (standPat - DELTA_MARGIN > beta) {
            return beta;
        }
        if (standPat - BIG_DELTA_MARGIN > beta) {
            return standPat;
        }
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves =
        GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);

    std::vector<std::pair<int, int>> tacticalMoves;
    for (const auto& move : moves) {
        if (isCapture(board, move.first, move.second)) {
            tacticalMoves.push_back(move);
        }
    }

    if (tacticalMoves.empty()) {
        return standPat;
    }

    std::vector<ScoredMove> scoredMoves;
    for (const auto& move : tacticalMoves) {
        int score = 0;
        bool isCapt = isCapture(board, move.first, move.second);

        if (isCapt) {
            int victimValue = getPieceValue(board.squares[move.second].piece.PieceType);
            int attackerValue = getPieceValue(board.squares[move.first].piece.PieceType);

            score = victimValue * 100 - attackerValue;

            int seeValue = staticExchangeEvaluation(board, move.first, move.second);

            int seeThreshold = -25 - ply * 15;
            if (seeValue < seeThreshold) {
                continue;
            }

            int expectedGain = std::max(seeValue, victimValue);
            if (maximizingPlayer && standPat + expectedGain + 150 < alpha) {
                continue;
            }
            if (!maximizingPlayer && standPat - expectedGain - 150 > beta) {
                continue;
            }

            score += seeValue;

            if (seeValue > victimValue) {
                score += 50;
            }

            if (seeValue >= 0) {
                score += 200;
            }
        }

        scoredMoves.push_back({move, score});
    }

    std::sort(scoredMoves.begin(), scoredMoves.end());

    int bestValue = standPat;

    for (const auto& scoredMove : scoredMoves) {
        if (context.stopSearch.load())
            return 0;

        const auto& move = scoredMove.move;
        Board newBoard = board;
        newBoard.movePiece(move.first, move.second);

        int eval = QuiescenceSearch(newBoard, alpha, beta, !maximizingPlayer, historyTable, context,
                                    ply + 1);
        if (context.stopSearch.load())
            return 0;

        if (maximizingPlayer) {
            bestValue = std::max(bestValue, eval);
            alpha = std::max(alpha, eval);
        } else {
            bestValue = std::min(bestValue, eval);
            beta = std::min(beta, eval);
        }

        if (beta <= alpha) {
            break;
        }
    }

    return bestValue;
}

int PrincipalVariationSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer,
                             int ply, ThreadSafeHistory& historyTable,
                             ParallelSearchContext& context, bool isPVNode) {

    if (depth < 0 || depth > 20) {
        return 0;
    }

    if (ply < 0 || ply >= 50) {
        return 0;
    }

    if (isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch = true;
        return 0;
    }

    if (depth == 0) {
        return QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
    }

    context.nodeCount++;

    uint64_t zobristKey = ComputeZobrist(board);
    context.transTable.prefetch(zobristKey);
    TTEntry ttEntry;
    if (context.transTable.find(zobristKey, ttEntry) && ttEntry.depth >= depth) {
        if (ttEntry.flag == 0) {
            return ttEntry.value;
        } else if (ttEntry.flag == -1 && ttEntry.value <= alpha) {
            return alpha;
        } else if (ttEntry.flag == 1 && ttEntry.value >= beta) {
            return beta;
        }
    }

    int staticEval = evaluatePosition(board);

    if (!isPVNode) {

        if (AdvancedSearch::staticNullMovePruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::futilityPruning(board, depth, alpha, beta, staticEval)) {
            return beta;
        }

        if (AdvancedSearch::nullMovePruning(board, depth, alpha, beta)) {
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;
            int nullScore =
                -PrincipalVariationSearch(board, depth - 3, -beta, -beta + 1, !maximizingPlayer,
                                          ply + 1, historyTable, context, false);
            board.turn = (board.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

            if (nullScore >= beta) {
                return beta;
            }
        }

        if (AdvancedSearch::multiCutPruning(board, depth, alpha, beta, 3)) {
            return beta;
        }
    }

    GenValidMoves(board);
    MovePicker picker(board, ttEntry.bestMove, context.killerMoves, ply, historyTable, context);

    int colorIdx = (board.turn == ChessPieceColor::WHITE) ? 0 : 1;
    int prevDest = board.LastMove;

    int bestValue = maximizingPlayer ? -10000 : 10000;
    std::pair<int, int> bestMove = {-1, -1};
    int flag = -1;
    size_t movesSearched = 0;

    std::pair<int, int> move;
    while ((move = picker.next()).first >= 0) {
        if (ply == 0) {
            bool excluded = false;
            for (const auto& ex : context.excludedRootMoves) {
                if (ex == move) { excluded = true; break; }
            }
            if (excluded) continue;
        }

        bool isCaptureMove = isCapture(board, move.first, move.second);

        if (!isPVNode && movesSearched > 0) {
            if (depth <= 6) {
                int seeVal = staticExchangeEvaluation(board, move.first, move.second);
                if (seeVal < -depth * 80) {
                    continue;
                }
            }

            if (AdvancedSearch::historyPruning(board, depth, move, historyTable)) {
                continue;
            }

            if (AdvancedSearch::lateMovePruning(board, depth, movesSearched, isInCheck(board, board.turn))) {
                continue;
            }
        }

        Board tempBoard = board;
        tempBoard.movePiece(move.first, move.second);
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;

        int extension = 0;
        if (AdvancedSearch::checkExtension(board, move, depth)) {
            extension = 1;
        } else if (AdvancedSearch::recaptureExtension(board, move, depth)) {
            extension = 1;
        } else if (AdvancedSearch::pawnPushExtension(board, move, depth)) {
            extension = 1;
        } else if (AdvancedSearch::passedPawnExtension(board, move, depth)) {
            extension = 1;
        } else if (AdvancedSearch::singularExtension(board, depth, move, alpha, beta)) {
            extension = 1;
        }

        int searchDepth = depth - 1 + extension;

        if (!isPVNode && movesSearched >= 4 && searchDepth >= 3) {
            if (AdvancedSearch::lateMoveReduction(board, searchDepth, movesSearched, alpha, beta)) {
                searchDepth = std::max(1, searchDepth - 1);
            }
        }

        int score;
        if (movesSearched == 0 || isPVNode) {
            score =
                -PrincipalVariationSearch(tempBoard, searchDepth, -beta, -alpha, !maximizingPlayer,
                                          ply + 1, historyTable, context, isPVNode);
        } else {
            score =
                -PrincipalVariationSearch(tempBoard, searchDepth, -alpha - 1, -alpha,
                                          !maximizingPlayer, ply + 1, historyTable, context, false);
            if (score > alpha && score < beta) {
                score = -PrincipalVariationSearch(tempBoard, searchDepth, -beta, -alpha,
                                                  !maximizingPlayer, ply + 1, historyTable, context,
                                                  true);
            }
        }

        movesSearched++;

        if (maximizingPlayer) {
            if (score > bestValue) {
                bestValue = score;
                bestMove = move;
                if (score > alpha) {
                    alpha = score;
                    flag = 0;
                }
            }
        } else {
            if (score < bestValue) {
                bestValue = score;
                bestMove = move;
                if (score < beta) {
                    beta = score;
                    flag = 0;
                }
            }
        }

        if (alpha >= beta) {
            flag = 1;
            if (!isCaptureMove) {
                context.killerMoves.store(ply, move);
                if (prevDest >= 0 && prevDest < 64) {
                    context.counterMoves[colorIdx][prevDest] = move;
                }
            }
            updateHistoryTable(historyTable, move.first, move.second, depth);
            break;
        }
    }

    if (movesSearched == 0) {
        if (isInCheck(board, board.turn)) {
            return maximizingPlayer ? -10000 + ply : 10000 - ply;
        } else {
            return -context.contempt;
        }
    }

    context.transTable.insert(zobristKey, TTEntry(depth, bestValue, flag, bestMove, zobristKey));

    return bestValue;
}

int AlphaBetaSearch(Board& board, int depth, int alpha, int beta, bool maximizingPlayer, int ply,
                    ThreadSafeHistory& historyTable, ParallelSearchContext& context) {

    if (depth < 0 || depth > 20) {
        return 0;
    }

    if (ply < 0 || ply >= 50) {
        return 0;
    }

    if (context.stopSearch.load() || isTimeUp(context.startTime, context.timeLimitMs)) {
        context.stopSearch.store(true);
        return 0;
    }
    context.nodeCount.fetch_add(1);

    int extension = 0;
    ChessPieceColor currentColor =
        maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK;

    if (depth == 1 && ply < 10 && isInCheck(board, currentColor)) {
        extension = 1;
    }

    else if (depth >= 6 && ply < 10) {
        TTEntry ttEntry;
        uint64_t hash = ComputeZobrist(board);
        if (context.transTable.find(hash, ttEntry) && ttEntry.depth >= depth - 3) {
            if (ttEntry.bestMove.first != -1 && ttEntry.value > alpha && ttEntry.value < beta) {

                int singularBeta = ttEntry.value - 50;
                if (singularBeta > alpha) {

                    Board testBoard = board;
                    int singularValue =
                        AlphaBetaSearch(testBoard, depth / 2, singularBeta - 1, singularBeta,
                                        maximizingPlayer, ply + 1, historyTable, context);

                    if (singularValue < singularBeta) {

                        extension = 1;
                    }
                }
            }
        }
    }

    depth += extension;

    if (depth > 20 || ply >= 50) {
        return 0;
    }

    uint64_t hash = ComputeZobrist(board);
    context.transTable.prefetch(hash);
    TTEntry entry;
    std::pair<int, int> hashMove = {-1, -1};

    if (context.transTable.find(hash, entry)) {
        if (entry.depth >= depth) {
            if (entry.flag == 0)
                return entry.value;
            if (entry.flag == -1 && entry.value <= alpha)
                return alpha;
            if (entry.flag == 1 && entry.value >= beta)
                return beta;
        }

        if (entry.bestMove.first != -1 && entry.bestMove.second != -1) {
            hashMove = entry.bestMove;
        }
    }
    if (depth == 0) {
        int eval =
            QuiescenceSearch(board, alpha, beta, maximizingPlayer, historyTable, context, ply);
        context.transTable.insert(hash, TTEntry(depth, eval, 0, {-1, -1}, hash));
        return eval;
    }
    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves =
        GetAllMoves(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK);
    if (moves.empty()) {
        if (isInCheck(board, maximizingPlayer ? ChessPieceColor::WHITE : ChessPieceColor::BLACK)) {
            int mateScore = maximizingPlayer ? -10000 + ply : 10000 - ply;
            context.transTable.insert(hash, TTEntry(depth, mateScore, 0, {-1, -1}, hash));
            return mateScore;
        }
        int drawScore = -context.contempt;
        context.transTable.insert(hash, TTEntry(depth, drawScore, 0, {-1, -1}, hash));
        return drawScore;
    }

    if (depth >= 3 && depth <= 10 && ply < 30 &&
        !isInCheck(board, currentColor)) {
        Board nullBoard = board;
        nullBoard.turn = (nullBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;
        int R = 3;
        int reducedDepth = depth - 1 - R;
        if (reducedDepth > 0 && reducedDepth <= 10) {
            int nullValue = AlphaBetaSearch(nullBoard, reducedDepth, alpha, beta, !maximizingPlayer,
                                            ply + 1, historyTable, context);
            if (context.stopSearch.load())
                return 0;
            if (maximizingPlayer) {
                if (nullValue >= beta) {
                    if (nullValue >= beta + 300) {
                        return beta;
                    }

                    if (depth - 1 <= 10 && ply + 1 < 30) {
                        int verificationValue =
                            AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer,
                                            ply + 1, historyTable, context);
                        if (context.stopSearch.load())
                            return 0;
                        if (verificationValue >= beta) {
                            return beta;
                        }
                    }
                }
            } else {
                if (nullValue <= alpha) {
                    if (nullValue <= alpha - 300) {
                        return alpha;
                    }

                    if (depth - 1 <= 10 && ply + 1 < 30) {
                        int verificationValue =
                            AlphaBetaSearch(nullBoard, depth - 1, alpha, beta, !maximizingPlayer,
                                            ply + 1, historyTable, context);
                        if (context.stopSearch.load())
                            return 0;
                        if (verificationValue <= alpha) {
                            return alpha;
                        }
                    }
                }
            }
        }
    }
    int abColorIdx = (board.turn == ChessPieceColor::WHITE) ? 0 : 1;
    int abPrevDest = board.LastMove;
    std::pair<int, int> abCounterMove = {-1, -1};
    if (abPrevDest >= 0 && abPrevDest < 64) {
        abCounterMove = context.counterMoves[abColorIdx][abPrevDest];
    }
    std::vector<ScoredMove> scoredMoves =
        scoreMovesOptimized(board, moves, historyTable, context.killerMoves, ply, hashMove, abCounterMove);
    std::sort(scoredMoves.begin(), scoredMoves.end(), std::greater<ScoredMove>());
    int origAlpha = alpha;
    int bestValue = maximizingPlayer ? -10000 : 10000;
    int flag = 0;
    bool foundPV = false;
    std::pair<int, int> bestMoveFound = {-1, -1};

    if (maximizingPlayer) {
        int moveCount = 0;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load())
                return 0;
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= 3 && !foundPV && !isCaptureMove && !isCheckMove &&
                !isInCheck(board, currentColor)) {

                int staticEval = evaluatePosition(board);

                const int futilityMargins[4] = {0, 200, 400, 600};
                int futilityValue = staticEval + futilityMargins[depth];

                if (futilityValue <= alpha) {
                    moveCount++;
                    continue;
                }
            }

            if (depth <= 3 && isCaptureMove && !isCheckMove && !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);

                if (seeValue < -100 && depth <= 2) {
                    moveCount++;
                    continue;
                }

                if (seeValue < -50 && depth == 3) {
                    eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, false, ply + 1,
                                           historyTable, context);
                    moveCount++;
                    if (context.stopSearch.load())
                        return 0;
                    if (eval > bestValue) {
                        bestValue = eval;
                        bestMoveFound = move;
                    }
                    if (eval > alpha) {
                        alpha = eval;
                        foundPV = true;
                        updateHistoryTable(historyTable, move.first, move.second, depth);
                    }
                    if (beta <= alpha) {
                        if (!isCaptureMove) {
                            context.killerMoves.store(ply, move);
                        }
                        break;
                    }
                    continue;
                }
            }

            if (moveCount == 0 || foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1,
                                       historyTable, context);
            } else {

                if (depth >= 3 && depth <= 10 && moveCount > 0 && !isCaptureMove && !isCheckMove &&
                    !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor) &&
                    ply < 30) {

                    int reduction = 1;
                    if (moveCount > 3)
                        reduction = 2;
                    if (moveCount > 6)
                        reduction = 3;

                    if (scoredMove.score > 1000)
                        reduction = std::max(1, reduction - 1);

                    int reducedDepth = depth - 1 - reduction;
                    if (reducedDepth >= 0 && reducedDepth <= 10) {

                        int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, alpha, alpha + 1,
                                                          false, ply + 1, historyTable, context);

                        if (reducedEval > alpha) {
                            eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1,
                                                   historyTable, context);
                        } else {
                            eval = reducedEval;
                        }
                    } else {

                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1,
                                               historyTable, context);
                    }
                } else {

                    eval = AlphaBetaSearch(newBoard, depth - 1, alpha, alpha + 1, false, ply + 1,
                                           historyTable, context);

                    if (eval > alpha && eval < beta) {
                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, false, ply + 1,
                                               historyTable, context);
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load())
                return 0;
            if (eval > bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval > alpha) {
                alpha = eval;
                foundPV = true;
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {

                if (!isCaptureMove) {
                    context.killerMoves.store(ply, move);
                    if (abPrevDest >= 0 && abPrevDest < 64) {
                        context.counterMoves[abColorIdx][abPrevDest] = move;
                    }
                }
                break;
            }
        }
        if (bestValue <= origAlpha)
            flag = -1;
        else if (bestValue >= beta)
            flag = 1;
        else
            flag = 0;
    } else {
        int moveCount = 0;
        for (const auto& scoredMove : scoredMoves) {
            if (context.stopSearch.load())
                return 0;
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);
            int eval;
            bool isCaptureMove = isCapture(board, move.first, move.second);
            bool isCheckMove = givesCheck(board, move.first, move.second);

            if (depth <= 3 && isCaptureMove && !isCheckMove && !isInCheck(board, currentColor)) {
                int seeValue = staticExchangeEvaluation(board, move.first, move.second);

                if (seeValue < -100 && depth <= 2) {
                    moveCount++;
                    continue;
                }

                if (seeValue < -50 && depth == 3) {
                    eval = AlphaBetaSearch(newBoard, depth - 2, alpha, beta, true, ply + 1,
                                           historyTable, context);
                    moveCount++;
                    if (context.stopSearch.load())
                        return 0;
                    if (eval < bestValue) {
                        bestValue = eval;
                        bestMoveFound = move;
                    }
                    if (eval < beta) {
                        beta = eval;
                        foundPV = true;
                        updateHistoryTable(historyTable, move.first, move.second, depth);
                    }
                    if (beta <= alpha) {
                        if (!isCaptureMove) {
                            context.killerMoves.store(ply, move);
                        }
                        break;
                    }
                    continue;
                }
            }

            if (moveCount == 0 || foundPV) {

                eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1,
                                       historyTable, context);
            } else {

                if (depth >= 3 && depth <= 10 && moveCount > 0 && !isCaptureMove && !isCheckMove &&
                    !context.killerMoves.isKiller(ply, move) && !isInCheck(board, currentColor) &&
                    ply < 30) {

                    int reduction = 1;
                    if (moveCount > 3)
                        reduction = 2;
                    if (moveCount > 6)
                        reduction = 3;

                    if (scoredMove.score > 1000)
                        reduction = std::max(1, reduction - 1);

                    int reducedDepth = depth - 1 - reduction;
                    if (reducedDepth >= 0 && reducedDepth <= 10) {

                        int reducedEval = AlphaBetaSearch(newBoard, reducedDepth, beta - 1, beta,
                                                          true, ply + 1, historyTable, context);

                        if (reducedEval < beta) {
                            eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1,
                                                   historyTable, context);
                        } else {
                            eval = reducedEval;
                        }
                    } else {

                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1,
                                               historyTable, context);
                    }
                } else {

                    eval = AlphaBetaSearch(newBoard, depth - 1, beta - 1, beta, true, ply + 1,
                                           historyTable, context);

                    if (eval < beta && eval > alpha) {
                        eval = AlphaBetaSearch(newBoard, depth - 1, alpha, beta, true, ply + 1,
                                               historyTable, context);
                    }
                }
            }

            moveCount++;
            if (context.stopSearch.load())
                return 0;
            if (eval < bestValue) {
                bestValue = eval;
                bestMoveFound = move;
            }
            if (eval < beta) {
                beta = eval;
                foundPV = true;
                updateHistoryTable(historyTable, move.first, move.second, depth);
            }
            if (beta <= alpha) {

                if (!isCaptureMove) {
                    context.killerMoves.store(ply, move);
                    if (abPrevDest >= 0 && abPrevDest < 64) {
                        context.counterMoves[abColorIdx][abPrevDest] = move;
                    }
                }
                break;
            }
        }
        if (bestValue >= beta)
            flag = 1;
        else if (bestValue <= origAlpha)
            flag = -1;
        else
            flag = 0;
    }

    context.transTable.insert(hash, TTEntry(depth, bestValue, flag, bestMoveFound, hash));
    return bestValue;
}

std::vector<std::pair<int, int>> GetAllMoves(Board& board, ChessPieceColor color) {
    return generateBitboardMoves(board, color);
}

SearchResult iterativeDeepeningParallel(Board& board, int maxDepth, int timeLimitMs,
                                        int numThreads, int contempt, int multiPV) {
    SearchResult result;
    ParallelSearchContext context(numThreads);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = timeLimitMs;
    context.contempt = contempt;
    context.multiPV = multiPV;
    context.transTable.newSearch();

    int lastScore = 0;
    const int ASPIRATION_WINDOW = 50;

    std::string fen = getFEN(board);
    std::string bookMove = getBookMove(fen);
    if (!bookMove.empty()) {
        int srcCol, srcRow, destCol, destRow;
        if (parseAlgebraicMove(bookMove, board, srcCol, srcRow, destCol, destRow)) {
            result.bestMove = {srcRow * 8 + srcCol, destRow * 8 + destCol};
            result.score = 0;
            result.depth = 1;
            result.nodes = 1;
            result.timeMs = 0;
            return result;
        }
    }

    std::pair<int, int> previousBestMove = {-1, -1};
    int bestMoveStableCount = 0;
    int bestMoveChangeCount = 0;

    for (int pvIdx = 0; pvIdx < context.multiPV; ++pvIdx) {

    lastScore = 0;
    context.stopSearch = false;

    for (int depth = 1; depth <= std::max(maxDepth, 10); depth++) {
        int alpha, beta;

        if (depth <= 3) {

            alpha = -10000;
            beta = 10000;
        } else {

            alpha = lastScore - ASPIRATION_WINDOW;
            beta = lastScore + ASPIRATION_WINDOW;
        }

        int searchScore;
        bool failedHigh = false, failedLow = false;
        int searchAttempts = 0;
        const int MAX_SEARCH_ATTEMPTS = 4;

        do {
            searchScore =
                AlphaBetaSearch(board, depth, alpha, beta, board.turn == ChessPieceColor::WHITE, 0,
                                context.historyTable, context);
            searchAttempts++;

            if (context.stopSearch.load()) {
                break;
            }

            if (searchScore <= alpha) {

                alpha = alpha - ASPIRATION_WINDOW * (1 << searchAttempts);
                failedLow = true;
                if (alpha < -10000)
                    alpha = -10000;
            } else if (searchScore >= beta) {

                beta = beta + ASPIRATION_WINDOW * (1 << searchAttempts);
                failedHigh = true;
                if (beta > 10000)
                    beta = 10000;
            } else {

                break;
            }

            if (searchAttempts >= MAX_SEARCH_ATTEMPTS) {

                alpha = -10000;
                beta = 10000;
            }

        } while ((failedHigh || failedLow) && searchAttempts < MAX_SEARCH_ATTEMPTS &&
                 !context.stopSearch.load());

        if (context.stopSearch.load()) {
            break;
        }

        lastScore = searchScore;
        result.score = searchScore;
        result.depth = depth;
        result.nodes = context.nodeCount.load();

        GenValidMoves(board);
        std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);
        if (!moves.empty()) {
            std::vector<ScoredMove> scoredMoves =
                scoreMovesOptimized(board, moves, context.historyTable, context.killerMoves, 0);
            std::sort(scoredMoves.begin(), scoredMoves.end(), std::greater<ScoredMove>());
            result.bestMove = scoredMoves[0].move;
        }

        if (pvIdx == 0) {
            if (result.bestMove == previousBestMove) {
                bestMoveStableCount++;
            } else {
                bestMoveChangeCount++;
                bestMoveStableCount = 0;
            }
            previousBestMove = result.bestMove;
        }

        if (context.multiPV > 1) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - context.startTime)
                               .count();
            int nps = elapsed > 0 ? static_cast<int>(result.nodes * 1000 / elapsed) : 0;
            std::cout << "info depth " << depth
                      << " multipv " << (pvIdx + 1)
                      << " score cp " << result.score
                      << " nodes " << result.nodes
                      << " nps " << nps
                      << " hashfull " << context.transTable.hashfull()
                      << std::endl;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - context.startTime)
                           .count();

        double timeRatio = (double)elapsed / timeLimitMs;
        bool timeToStop = false;

        double stabilityFactor = 1.0;
        if (bestMoveChangeCount >= 2 && bestMoveStableCount < 2) {
            stabilityFactor = 1.5;
        } else if (bestMoveStableCount >= 4) {
            stabilityFactor = 0.5;
        }

        bool isTactical = false;
        if (isInCheck(board, board.turn)) {
            isTactical = true;
        } else {

            for (int i = 0; i < 64 && !isTactical; i++) {
                const Piece& piece = board.squares[i].piece;
                if (piece.PieceType != ChessPieceType::NONE &&
                    piece.PieceType != ChessPieceType::PAWN) {
                    ChessPieceColor enemyColor = (piece.PieceColor == ChessPieceColor::WHITE)
                                                     ? ChessPieceColor::BLACK
                                                     : ChessPieceColor::WHITE;
                    for (int j = 0; j < 64; j++) {
                        const Piece& enemyPiece = board.squares[j].piece;
                        if (enemyPiece.PieceColor == enemyColor &&
                            canPieceAttackSquare(board, j, i)) {
                            isTactical = true;
                            break;
                        }
                    }
                }
            }
        }

        double threshold1 = (isTactical ? 0.85 : 0.8) * stabilityFactor;
        double threshold2 = (isTactical ? 0.95 : 0.9) * stabilityFactor;

        if (depth >= 8 && timeRatio > threshold1) {
            timeToStop = true;
        } else if (depth >= 6 && timeRatio > threshold2) {
            timeToStop = true;
        } else if (timeRatio > 0.98) {
            timeToStop = true;
        }

        if (timeToStop) {
            break;
        }
    }

    if (pvIdx < context.multiPV - 1 && result.bestMove.first >= 0) {
        context.excludedRootMoves.push_back(result.bestMove);
    }

    }

    auto endTime = std::chrono::steady_clock::now();
    result.timeMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime - context.startTime).count();

    return result;
}

std::pair<int, int> findBestMove(Board& board, int depth) {
    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);
    context.startTime = std::chrono::steady_clock::now();
    context.timeLimitMs = 30000;

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.empty()) {
        return {-1, -1};
    }

    int previousScore = 0;
    int aspirationWindow = 50;
    int maxAspirationWindow = 400;

    std::vector<ScoredMove> scoredMoves =
        scoreMovesOptimized(board, moves, historyTable, context.killerMoves, 0);
    std::sort(scoredMoves.begin(), scoredMoves.end(), std::greater<ScoredMove>());

    int bestEval = (board.turn == ChessPieceColor::WHITE) ? -10000 : 10000;
    std::pair<int, int> bestMove = {-1, -1};

    for (int currentDepth = 1; currentDepth <= depth; currentDepth++) {

        int alpha, beta;

        if (currentDepth == 1) {

            alpha = -10000;
            beta = 10000;
        } else {

            alpha = previousScore - aspirationWindow;
            beta = previousScore + aspirationWindow;
        }

        bool searchFailed = false;
        int currentBestEval = bestEval;
        std::pair<int, int> currentBestMove = bestMove;

        for (const auto& scoredMove : scoredMoves) {
            const auto& move = scoredMove.move;
            Board newBoard = board;
            newBoard.movePiece(move.first, move.second);

            int eval =
                AlphaBetaSearch(newBoard, currentDepth - 1, alpha, beta,
                                (board.turn == ChessPieceColor::BLACK), 0, historyTable, context);

            if (board.turn == ChessPieceColor::WHITE) {
                if (eval > currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;

                    if (eval > alpha) {
                        alpha = eval;
                    }

                    if (eval >= beta) {
                        searchFailed = true;
                        break;
                    }
                }
            } else {
                if (eval < currentBestEval) {
                    currentBestEval = eval;
                    currentBestMove = move;

                    if (eval < beta) {
                        beta = eval;
                    }

                    if (eval <= alpha) {
                        searchFailed = true;
                        break;
                    }
                }
            }
        }

        if (searchFailed && currentDepth > 1) {

            aspirationWindow = std::min(aspirationWindow * 2, maxAspirationWindow);

            if (board.turn == ChessPieceColor::WHITE && currentBestEval >= beta) {

                beta = 10000;
            } else if (board.turn == ChessPieceColor::BLACK && currentBestEval <= alpha) {

                alpha = -10000;
            } else {

                alpha = -10000;
                beta = 10000;
            }

            currentDepth--;
            continue;
        }

        bestEval = currentBestEval;
        bestMove = currentBestMove;
        previousScore = bestEval;

        aspirationWindow = std::max(50, aspirationWindow / 2);
    }

    return bestMove;
}

int staticExchangeEvaluation(const Board& board, int fromSquare, int toSquare) {
    if (fromSquare < 0 || fromSquare >= 64 || toSquare < 0 || toSquare >= 64) {
        return 0;
    }

    const Piece& victim = board.squares[toSquare].piece;
    const Piece& attacker = board.squares[fromSquare].piece;

    if (victim.PieceType == ChessPieceType::NONE || attacker.PieceType == ChessPieceType::NONE) {
        return 0;
    }

    int score = victim.PieceValue;
    ChessPieceColor sideToMove = attacker.PieceColor;

    Board tempBoard = board;
    tempBoard.squares[toSquare].piece = attacker;
    tempBoard.squares[fromSquare].piece = Piece();
    tempBoard.updateBitboards();

    ChessPieceColor currentSide = sideToMove;

    while (true) {

        int smallestAttacker = getSmallestAttacker(tempBoard, toSquare, currentSide);
        if (smallestAttacker == -1) {
            break;
        }

        const Piece& currentAttacker = tempBoard.squares[smallestAttacker].piece;

        tempBoard.squares[toSquare].piece = currentAttacker;
        tempBoard.squares[smallestAttacker].piece = Piece();
        tempBoard.updateBitboards();

        if (currentSide == sideToMove) {
            score = currentAttacker.PieceValue - score;
        } else {
            score = currentAttacker.PieceValue - score;
        }

        currentSide = (currentSide == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                              : ChessPieceColor::WHITE;
    }

    return score;
}

int getSmallestAttacker(const Board& board, int targetSquare, ChessPieceColor color) {
    int smallestValue = 10000;
    int smallestAttacker = -1;

    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType == ChessPieceType::NONE || piece.PieceColor != color) {
            continue;
        }

        if (canPieceAttackSquare(board, i, targetSquare)) {
            if (piece.PieceValue < smallestValue) {
                smallestValue = piece.PieceValue;
                smallestAttacker = i;
            }
        }
    }

    return smallestAttacker;
}

bool isGoodCapture(const Board& board, int fromSquare, int toSquare) {
    int see = staticExchangeEvaluation(board, fromSquare, toSquare);
    return see >= 0;
}

bool isCaptureProfitable(const Board& board, int fromSquare, int toSquare, int threshold) {
    int see = staticExchangeEvaluation(board, fromSquare, toSquare);
    return see >= threshold;
}

bool isDiscoveredCheck(const Board& board, int from, int to);
bool isPromotion(const Board& board, int from, int to);
bool isCastling(const Board& board, int from, int to);
int staticExchangeEval(const Board& board, int fromSquare, int toSquare);

bool isDiscoveredCheck(const Board& board, int from, int) {

    ChessPieceType movingPiece = board.squares[from].piece.PieceType;
    return (movingPiece != ChessPieceType::QUEEN && movingPiece != ChessPieceType::ROOK &&
            movingPiece != ChessPieceType::BISHOP);
}

bool isPromotion(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    ChessPieceColor color = board.squares[from].piece.PieceColor;

    if (piece != ChessPieceType::PAWN)
        return false;

    int destRow = to / 8;
    return (color == ChessPieceColor::WHITE && destRow == 7) ||
           (color == ChessPieceColor::BLACK && destRow == 0);
}

bool isCastling(const Board& board, int from, int to) {
    ChessPieceType piece = board.squares[from].piece.PieceType;
    if (piece != ChessPieceType::KING)
        return false;

    return std::abs(to - from) == 2;
}

const int EnhancedMoveOrdering::MVV_LVA_SCORES[6][6] = {{0, 0, 0, 0, 0, 0},  {50, 0, 0, 0, 0, 0},
                                                        {50, 0, 0, 0, 0, 0}, {52, 2, 2, 0, 0, 0},
                                                        {54, 4, 4, 2, 0, 0}, {53, 3, 3, 1, 1, 0}};

int EnhancedMoveOrdering::getMVVLVA_Score(const Board& board, int fromSquare, int toSquare) {
    const Piece& attacker = board.squares[fromSquare].piece;
    const Piece& victim = board.squares[toSquare].piece;

    if (victim.PieceType == ChessPieceType::NONE)
        return 0;

    int attackerIndex = static_cast<int>(attacker.PieceType) - 1;
    int victimIndex = static_cast<int>(victim.PieceType) - 1;

    if (attackerIndex < 0 || attackerIndex >= 6 || victimIndex < 0 || victimIndex >= 6) {
        return 0;
    }

    return MVV_LVA_SCORES[attackerIndex][victimIndex];
}

int EnhancedMoveOrdering::getHistoryScore(const ThreadSafeHistory& history, int fromSquare,
                                          int toSquare) {
    return history.get(fromSquare, toSquare);
}

int EnhancedMoveOrdering::getKillerScore(const KillerMoves& killers, int ply, int fromSquare,
                                         int toSquare) {
    std::pair<int, int> move = {fromSquare, toSquare};
    if (killers.isKiller(ply, move)) {
        return killers.getKillerScore(ply, move);
    }
    return 0;
}

int EnhancedMoveOrdering::getPositionalScore(const Board& board, int fromSquare, int toSquare) {
    const Piece& piece = board.squares[fromSquare].piece;
    if (piece.PieceType == ChessPieceType::NONE)
        return 0;

    int toSquareAdjusted = (piece.PieceColor == ChessPieceColor::WHITE) ? toSquare : 63 - toSquare;
    return getPieceSquareValue(piece.PieceType, toSquareAdjusted, piece.PieceColor) / 10;
}
#include "LazySMP.h"

SearchResult lazySMPSearch(Board& board, int maxDepth, int timeLimitMs, int numThreads) {
    LazySMP smp(numThreads);
    return smp.search(board, maxDepth, timeLimitMs);
}
