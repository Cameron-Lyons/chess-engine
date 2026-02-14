#include "AdvancedSearch.h"
#include "../core/BitboardMoves.h"
#include "../evaluation/Evaluation.h"
#include "../utils/engine_globals.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <ranges>
#include <sstream>
#include <unordered_map>

bool AdvancedSearch::futilityPruning(const Board& board, int depth, int alpha, int beta,
                                     int staticEval) {

    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }

    int futilityMargin = 150 * depth;

    if (depth <= 3) {
        int tacticalBonus = 50 * (3 - depth);
        futilityMargin += tacticalBonus;
    }

    if (staticEval - futilityMargin >= beta) {
        return true;
    }

    if (depth <= 3) {
        int deltaMargin = 975 + 50 * depth;
        if (staticEval + deltaMargin < alpha) {
            return true;
        }
    }

    return false;
}

bool AdvancedSearch::staticNullMovePruning(const Board& board, int depth, int alpha, int beta,
                                           int staticEval) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }

    if (depth <= 4) {

        int margin = 900 + 50 * depth;
        int eval = staticEval - margin;

        if (eval >= beta) {
            return true;
        }
    }

    return false;
}

bool AdvancedSearch::nullMovePruning(const Board& board, int depth, int alpha, int beta) {
    (void)alpha;
    (void)beta;

    if (isInCheck(board, board.turn) || depth < 3) {
        return false;
    }

    int materialCount = 0;
    int totalMaterial = 0;

    for (int i = 0; i < 64; i++) {
        const Piece& piece = board.squares[i].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            materialCount++;
            totalMaterial += piece.PieceValue;
        }
    }

    if (materialCount < 3 || totalMaterial < 300) {
        return false;
    }

    if (materialCount <= 4) {
        return false;
    }

    return true;
}

bool AdvancedSearch::lateMoveReduction(const Board& board, int depth, int moveNumber, int alpha,
                                       int beta) {
    (void)alpha;
    (void)beta;

    if (isInCheck(board, board.turn) || depth < 3 || moveNumber < 4) {
        return false;
    }

    if (depth <= 3) {
        return false;
    }

    int reduction = 1;
    if (moveNumber >= 8)
        reduction = 2;
    if (moveNumber >= 12)
        reduction = 3;

    if (depth - reduction < 1) {
        return false;
    }

    return true;
}

bool AdvancedSearch::multiCutPruning(Board& board, int depth, int alpha, int beta, int r) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth < 4) {
        return false;
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.size() < 8) {
        return false;
    }

    int nullMoveCount = 0;
    for (int i = 0; i < r && i < static_cast<int>(moves.size()); i++) {

        Board tempBoard = board;
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;

        ThreadSafeHistory historyTable;
        ParallelSearchContext context(1);
        int nullScore =
            -AlphaBetaSearch(tempBoard, depth - 3, -beta, -beta + 1,
                             !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);

        if (nullScore >= beta) {
            nullMoveCount++;
        }
    }

    return nullMoveCount >= 2;
}

std::pair<int, int> AdvancedSearch::internalIterativeDeepening(Board& board, int depth, int alpha,
                                                               int beta) {

    if (depth < 4) {
        return {-1, -1};
    }

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);

    int reducedDepth = std::max(1, depth - 2);
    (void)AlphaBetaSearch(board, reducedDepth, alpha, beta, board.turn == ChessPieceColor::WHITE, 0,
                          historyTable, context);

    return {-1, -1};
}

bool AdvancedSearch::singularExtension(Board& board, int depth, const std::pair<int, int>& move,
                                       int alpha, int beta) {

    if (depth < 6) {
        return false;
    }

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    ThreadSafeHistory historyTable;
    ParallelSearchContext context(1);

    int reducedDepth = depth - 1;
    int score = AlphaBetaSearch(tempBoard, reducedDepth, alpha, beta,
                                !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    int betterMoves = 0;
    for (const auto& otherMove : moves) {
        if (otherMove == move)
            continue;

        Board otherBoard = board;
        if (!otherBoard.movePiece(otherMove.first, otherMove.second))
            continue;
        otherBoard.turn = (otherBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                      : ChessPieceColor::WHITE;

        int otherScore =
            AlphaBetaSearch(otherBoard, reducedDepth - 2, alpha, beta,
                            !(board.turn == ChessPieceColor::WHITE), 0, historyTable, context);

        if (otherScore >= score) {
            betterMoves++;
            if (betterMoves >= 2)
                break;
        }
    }

    return betterMoves == 0;
}

bool AdvancedSearch::historyPruning(const Board& board, int depth, const std::pair<int, int>& move,
                                    const ThreadSafeHistory& history) {

    if (isInCheck(board, board.turn) || depth == 0) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int historyScore = history.getScore(move.first, move.second);

    if (depth <= 3 && historyScore < -1000) {
        return true;
    }

    return false;
}

bool AdvancedSearch::lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck) {
    (void)board;

    if (inCheck) {
        return false;
    }

    if (depth == 0 || depth > 3) {
        return false;
    }

    if (moveNumber >= 4 && depth <= 3) {
        return true;
    }

    return false;
}

bool AdvancedSearch::recaptureExtension(const Board& board, const std::pair<int, int>& move,
                                        int depth) {
    (void)depth;

    if (board.LastMove < 0 || board.LastMove >= 64) {
        return false;
    }

    if (move.second == board.LastMove) {

        return true;
    }

    return false;
}

bool AdvancedSearch::checkExtension(const Board& board, const std::pair<int, int>& move,
                                    int depth) {
    (void)depth;

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return false;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    return isInCheck(tempBoard, tempBoard.turn);
}

bool AdvancedSearch::pawnPushExtension(const Board& board, const std::pair<int, int>& move,
                                       int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int destRow = move.second / 8;
    if (piece.PieceColor == ChessPieceColor::WHITE && destRow >= 5) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && destRow <= 2) {
        return true;
    }

    return false;
}

bool AdvancedSearch::passedPawnExtension(const Board& board, const std::pair<int, int>& move,
                                         int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    int srcRow = move.first / 8;
    int destRow = move.second / 8;

    if (piece.PieceColor == ChessPieceColor::WHITE && destRow - srcRow > 0) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && srcRow - destRow > 0) {
        return true;
    }

    return false;
}

std::vector<EnhancedMoveOrdering::MoveScore> EnhancedMoveOrdering::scoreMoves(
    const Board& board, const std::vector<std::pair<int, int>>& moves,
    const ThreadSafeHistory& history, const KillerMoves& killers, int ply,
    const std::pair<int, int>& hashMove) {

    std::vector<MoveScore> scoredMoves;

    for (const auto& move : moves) {
        int score = 0;

        if (move == hashMove) {
            score = 10000;
        }

        else if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
            score = getSEEScore(board, move);
        }

        else if (killers.isKiller(ply, move)) {
            score = killers.getKillerScore(ply, move);
        }

        else {
            score = history.get(move.first, move.second);
        }

        score += getPositionalScore(board, move);
        score += getMobilityScore(board, move);
        score += getThreatScore(board, move);

        scoredMoves.emplace_back(move, score);
    }

    return scoredMoves;
}

int EnhancedMoveOrdering::getSEEScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& attacker = board.squares[move.first].piece;
    const Piece& victim = board.squares[move.second].piece;

    if (victim.PieceType == ChessPieceType::NONE)
        return 0;

    int score = victim.PieceValue * 10 - attacker.PieceValue;

    if (attacker.PieceValue < victim.PieceValue) {
        score += 50;
    }

    int destCol = move.second % 8;
    int destRow = move.second / 8;
    if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
        score += 10;
    }

    return score;
}

int EnhancedMoveOrdering::getThreatScore(const Board& board, const std::pair<int, int>& move) {
    int score = 0;

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return 0;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> opponentMoves = GetAllMoves(tempBoard, tempBoard.turn);

    for (const auto& oppMove : opponentMoves) {
        const Piece& threatenedPiece = board.squares[oppMove.second].piece;
        if (threatenedPiece.PieceType != ChessPieceType::NONE &&
            threatenedPiece.PieceColor == board.turn) {
            score += threatenedPiece.PieceValue / 10;
        }
    }

    return score;
}

int EnhancedMoveOrdering::getMobilityScore(const Board& board, const std::pair<int, int>& move) {

    Board tempBoard = board;
    if (!tempBoard.movePiece(move.first, move.second)) {
        return 0;
    }
    tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                : ChessPieceColor::WHITE;

    GenValidMoves(tempBoard);
    std::vector<std::pair<int, int>> moves = GetAllMoves(tempBoard, tempBoard.turn);

    return moves.size() * 2;
}

int EnhancedMoveOrdering::getPositionalScore(const Board& board, const std::pair<int, int>& move) {
    const Piece& piece = board.squares[move.first].piece;
    int destCol = move.second % 8;
    int destRow = move.second / 8;

    int score = 0;

    switch (piece.PieceType) {
        case ChessPieceType::PAWN:

            if (piece.PieceColor == ChessPieceColor::WHITE) {
                score += destRow * 5;
            } else {
                score += (7 - destRow) * 5;
            }
            break;

        case ChessPieceType::KNIGHT:
        case ChessPieceType::BISHOP:

            if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
                score += 10;
            }
            break;

        case ChessPieceType::ROOK:

            if (destRow == 6 || destRow == 1) {
                score += 15;
            }
            break;

        case ChessPieceType::QUEEN:

            if ((destCol >= 2 && destCol <= 5) && (destRow >= 2 && destRow <= 5)) {
                score += 5;
            }
            break;

        default:
            break;
    }

    return score;
}

TimeManager::TimeManager(const TimeControl& tc) : timeControl(tc), moveNumber(0), totalMoves(30) {}

int TimeManager::allocateTime(Board& board, int depth, int nodes, bool isInCheck) {

    int baseTime = calculateBaseTime();
    int increment = calculateIncrement();

    double factor = getTimeFactor(depth, nodes);

    GamePhase phase = getGamePhase(board);
    factor *= getPhaseTimeFactor(phase);

    if (isInCheck) {
        factor *= 1.5;
    }

    GenValidMoves(board);
    std::vector<std::pair<int, int>> moves = GetAllMoves(board, board.turn);

    if (moves.size() < 10) {
        factor *= 1.2;
    } else if (moves.size() > 25) {
        factor *= 0.8;
    }

    int allocatedTime = static_cast<int>((baseTime + increment) * factor);

    allocatedTime = std::max(50, std::min(allocatedTime, baseTime * 2));

    return allocatedTime;
}

int TimeManager::calculateBaseTime() {
    if (timeControl.isInfinite) {
        return 30000;
    }

    if (timeControl.movesToGo > 0) {
        return timeControl.baseTime / timeControl.movesToGo;
    }

    int movesRemaining = std::max(10, totalMoves - moveNumber);
    return timeControl.baseTime / movesRemaining;
}

int TimeManager::calculateIncrement() {
    return timeControl.increment;
}

bool TimeManager::shouldStop(int elapsedTime, int allocatedTime, int depth, int nodes) {
    (void)nodes;

    if (elapsedTime >= allocatedTime) {
        return true;
    }

    if (isEmergencyTime(timeControl.baseTime, allocatedTime)) {
        return true;
    }

    if (depth >= 20) {
        return true;
    }

    return false;
}

void TimeManager::updateGameProgress(int moveNumber, int totalMoves) {
    this->moveNumber = moveNumber;
    this->totalMoves = totalMoves;
}

bool TimeManager::isEmergencyTime(int remainingTime, int allocatedTime) {

    return allocatedTime > remainingTime * 0.8;
}

double TimeManager::getTimeFactor(int depth, int nodes) {

    double factor = 1.0;

    if (depth >= 10) {
        factor *= 1.2;
    } else if (depth <= 3) {
        factor *= 0.8;
    }

    if (nodes > 1000000) {
        factor *= 1.1;
    }

    return factor;
}

GamePhase TimeManager::getGamePhase(const Board& board) const {
    int totalMaterial = 0;
    int pieceCount = 0;
    int queenCount = 0;
    int rookCount = 0;
    int minorPieceCount = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const Piece& piece = board.squares[sq].piece;
        if (piece.PieceType != ChessPieceType::NONE && piece.PieceType != ChessPieceType::KING) {
            pieceCount++;
            switch (piece.PieceType) {
                case ChessPieceType::QUEEN:
                    totalMaterial += 900;
                    queenCount++;
                    break;
                case ChessPieceType::ROOK:
                    totalMaterial += 500;
                    rookCount++;
                    break;
                case ChessPieceType::BISHOP:
                case ChessPieceType::KNIGHT:
                    totalMaterial += 300;
                    minorPieceCount++;
                    break;
                case ChessPieceType::PAWN:
                    totalMaterial += 100;
                    break;
                default:
                    break;
            }
        }
    }

    if (totalMaterial > 6000 && queenCount >= 1 && pieceCount > 20) {
        return GamePhase::OPENING;
    }

    if (totalMaterial < 2000 || pieceCount <= 10 ||
        (queenCount == 0 && rookCount <= 1 && minorPieceCount <= 2)) {
        return GamePhase::ENDGAME;
    }

    return GamePhase::MIDDLEGAME;
}

double TimeManager::getPhaseTimeFactor(GamePhase phase) const {
    switch (phase) {
        case GamePhase::OPENING:
            return 0.7;
        case GamePhase::MIDDLEGAME:
            return 1.0;
        case GamePhase::ENDGAME:
            return 1.3;
        default:
            return 1.0;
    }
}

EnhancedOpeningBook::EnhancedOpeningBook(const std::string& bookPath) : book(), bookPath(bookPath) {
    loadBook(bookPath);
}

std::vector<EnhancedOpeningBook::BookEntry> EnhancedOpeningBook::getBookMoves(const Board& board) {
    std::string key = boardToKey(board);
    auto it = book.find(key);
    if (it != book.end()) {
        return it->second;
    }
    return {};
}

std::pair<int, int> EnhancedOpeningBook::getBestMove(const Board& board, bool randomize) {
    std::string key = boardToKey(board);
    auto it = book.find(key);
    if (it != book.end() && !it->second.empty()) {
        const auto& entries = it->second;
        if (randomize) {
            int totalWeight = 0;
            for (const auto& entry : entries) {
                totalWeight += entry.weight;
            }
            if (totalWeight > 0) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, totalWeight - 1);
                int random = dis(gen);
                int cumulative = 0;
                for (const auto& entry : entries) {
                    cumulative += entry.weight;
                    if (random < cumulative) {
                        return entry.move;
                    }
                }
            }
            return entries[0].move;
        } else {
            auto bestIt =
                std::ranges::max_element(entries, [](const BookEntry& a, const BookEntry& b) {
                    if (a.weight != b.weight) {
                        return a.weight < b.weight;
                    }
                    return a.winRate < b.winRate;
                });
            return bestIt->move;
        }
    }

    std::string fen = getFEN(board);
    auto optionsIt = OpeningBookOptions.find(fen);
    if (optionsIt != OpeningBookOptions.end()) {
        const auto& options = optionsIt->second;
        if (!options.empty()) {
            if (randomize) {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, options.size() - 1);
                std::string move = options[dis(gen)];
                return parseMove(move);
            } else {
                return parseMove(options[0]);
            }
        }
    }

    auto legacyIt = OpeningBook.find(fen);
    if (legacyIt != OpeningBook.end()) {
        return parseMove(legacyIt->second);
    }

    return {-1, -1};
}

std::pair<int, int> EnhancedOpeningBook::parseMove(const std::string& move) {

    if (move.length() < 4)
        return {-1, -1};

    int srcCol = move[0] - 'a';
    int srcRow = move[1] - '1';
    int destCol = move[2] - 'a';
    int destRow = move[3] - '1';

    if (srcCol < 0 || srcCol > 7 || srcRow < 0 || srcRow > 7 || destCol < 0 || destCol > 7 ||
        destRow < 0 || destRow > 7) {
        return {-1, -1};
    }

    return {(srcRow * 8) + srcCol, (destRow * 8) + destCol};
}

bool EnhancedOpeningBook::isInBook(const Board& board) {
    std::string key = boardToKey(board);
    return book.contains(key);
}

void EnhancedOpeningBook::addMove(const Board& board, const BookEntry& entry) {
    std::string key = boardToKey(board);
    auto it = book.find(key);
    if (it != book.end()) {
        bool found = false;
        for (auto& existingEntry : it->second) {
            if (existingEntry.move == entry.move) {
                existingEntry.weight = std::max(existingEntry.weight, entry.weight);
                existingEntry.games += entry.games;
                existingEntry.winRate = ((existingEntry.winRate * existingEntry.games) +
                                         (entry.winRate * entry.games)) /
                                        (existingEntry.games + entry.games);
                existingEntry.averageRating = ((existingEntry.averageRating * existingEntry.games) +
                                               (entry.averageRating * entry.games)) /
                                              (existingEntry.games + entry.games);
                found = true;
                break;
            }
        }
        if (!found) {
            it->second.push_back(entry);
        }
        normalizeWeights(it->second);
    } else {
        book[key] = {entry};
    }
}

void EnhancedOpeningBook::saveBook(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    size_t numPositions = book.size();
    file.write(reinterpret_cast<const char*>(&numPositions), sizeof(numPositions));

    for (const auto& [key, entries] : book) {
        size_t keyLen = key.length();
        file.write(reinterpret_cast<const char*>(&keyLen), sizeof(keyLen));
        file.write(key.c_str(), keyLen);

        size_t numMoves = entries.size();
        file.write(reinterpret_cast<const char*>(&numMoves), sizeof(numMoves));

        for (const auto& entry : entries) {
            file.write(reinterpret_cast<const char*>(&entry.move.first), sizeof(entry.move.first));
            file.write(reinterpret_cast<const char*>(&entry.move.second),
                       sizeof(entry.move.second));
            file.write(reinterpret_cast<const char*>(&entry.weight), sizeof(entry.weight));
            file.write(reinterpret_cast<const char*>(&entry.games), sizeof(entry.games));
            file.write(reinterpret_cast<const char*>(&entry.winRate), sizeof(entry.winRate));
            file.write(reinterpret_cast<const char*>(&entry.averageRating),
                       sizeof(entry.averageRating));
        }
    }

    file.close();
}

void EnhancedOpeningBook::loadBook(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    book.clear();

    size_t numPositions{0};
    file.read(reinterpret_cast<char*>(&numPositions), sizeof(numPositions));

    for (size_t i = 0; i < numPositions; ++i) {
        size_t keyLen = 0;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        std::string key(keyLen, '\0');
        file.read(key.data(), static_cast<std::streamsize>(keyLen));

        size_t numMoves = 0;
        file.read(reinterpret_cast<char*>(&numMoves), sizeof(numMoves));

        std::vector<BookEntry> entries;
        entries.reserve(numMoves);

        for (size_t j = 0; j < numMoves; ++j) {
            BookEntry entry;
            file.read(reinterpret_cast<char*>(&entry.move.first), sizeof(entry.move.first));
            file.read(reinterpret_cast<char*>(&entry.move.second), sizeof(entry.move.second));
            file.read(reinterpret_cast<char*>(&entry.weight), sizeof(entry.weight));
            file.read(reinterpret_cast<char*>(&entry.games), sizeof(entry.games));
            file.read(reinterpret_cast<char*>(&entry.winRate), sizeof(entry.winRate));
            file.read(reinterpret_cast<char*>(&entry.averageRating), sizeof(entry.averageRating));
            entries.push_back(entry);
        }

        book[key] = entries;
    }

    file.close();
    bookPath = path;
}

std::string EnhancedOpeningBook::boardToKey(const Board& board) {
    std::string fen = getFEN(board);

    std::istringstream iss(fen);
    std::string position, activeColor, castling, enPassant;
    iss >> position >> activeColor >> castling >> enPassant;

    std::ostringstream keyStream;
    keyStream << position << " " << activeColor << " " << castling << " " << enPassant;
    return keyStream.str();
}

void EnhancedOpeningBook::normalizeWeights(std::vector<BookEntry>& entries) {
    if (entries.empty()) {
        return;
    }

    int totalWeight = 0;
    for (const auto& entry : entries) {
        totalWeight += entry.weight;
    }

    if (totalWeight == 0) {
        for (auto& entry : entries) {
            entry.weight = entry.games > 0 ? entry.games : 1;
        }
        totalWeight = 0;
        for (const auto& entry : entries) {
            totalWeight += entry.weight;
        }
    }

    if (totalWeight > 0) {
        const int TARGET_SUM = 1000;
        for (auto& entry : entries) {
            entry.weight = (entry.weight * TARGET_SUM) / totalWeight;
        }
    }
}

EnhancedOpeningBook::BookStats EnhancedOpeningBook::getStats() const {
    BookStats stats{};
    stats.totalPositions = book.size();
    stats.totalMoves = 0;
    stats.totalGames = 0;
    float totalWinRate = 0.0f;
    float totalRating = 0.0f;
    int moveCount = 0;

    for (const auto& [key, entries] : book) {
        stats.totalMoves += entries.size();
        for (const auto& entry : entries) {
            stats.totalGames += entry.games;
            totalWinRate += entry.winRate;
            totalRating += entry.averageRating;
            moveCount++;
        }
    }

    if (moveCount > 0) {
        stats.averageWinRate = totalWinRate / moveCount;
        stats.averageRating = totalRating / moveCount;
    }

    return stats;
}

void EnhancedOpeningBook::analyzeBook() {
    (void)getStats();
}