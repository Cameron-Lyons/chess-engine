#include "AdvancedSearch.h"
#include "../evaluation/GamePhaseConstants.h"
#include "../utils/engine_globals.h"
#include "Bitboard.h"
#include "BookUtils.h"
#include "ChessBoard.h"
#include "ChessPiece.h"
#include "search/ValidMoves.h"
#include "search/search.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <ios>
#include <random>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr int kZero = 0;
constexpr int kOne = 1;
constexpr int kBoardSquareCount = 64;
constexpr int kBoardDimension = 8;
constexpr int kInvalidSquare = -1;

constexpr int kFutilityMarginPerDepth = 150;
constexpr int kShallowDepthLimit = 3;
constexpr int kTacticalBonusPerMissingPly = 50;
constexpr int kDeltaMarginBase = 975;
constexpr int kDeltaMarginPerDepth = 50;

constexpr int kStaticNullDepthLimit = 4;
constexpr int kStaticNullMarginBase = 900;
constexpr int kStaticNullMarginPerDepth = 50;

constexpr int kNullMoveMinDepth = 3;
constexpr int kNullMoveMinMaterialCount = 3;
constexpr int kNullMoveMinTotalMaterial = 300;
constexpr int kNullMoveMaxMaterialCount = 4;

constexpr int kMultiCutMinDepth = 4;
constexpr int kMultiCutMinMoves = 8;
constexpr int kMultiCutDepthReduction = 3;
constexpr int kMultiCutCutoffCount = 2;

constexpr int kHistoryPruningDepthLimit = 3;
constexpr int kHistoryPruningThreshold = -1000;
constexpr int kLateMovePruningMoveThreshold = 4;
constexpr int kRecaptureMaxLastMove = kBoardSquareCount - 1;
constexpr int kPawnPushWhiteAdvancedRank = 5;
constexpr int kPawnPushBlackAdvancedRank = 2;

constexpr int kEndgameRookCountThreshold = 1;
constexpr int kEndgameMinorPieceCountThreshold = 2;

constexpr int kMinBookMoveLength = 4;
constexpr int kSingleThreadContextCount = 1;
constexpr int kRootPly = 0;
constexpr int kFirstMoveIndex = 0;
constexpr int kNarrowWindowWidth = 1;
constexpr int kMoveSourceFileIndex = 0;
constexpr int kMoveSourceRankIndex = 1;
constexpr int kMoveDestFileIndex = 2;
constexpr int kMoveDestRankIndex = 3;
constexpr char kBookFileOffset = 'a';
constexpr char kBookRankOffset = '1';
constexpr float kZeroFloat = 0.0F;

struct MaterialSummary {
    int nonKingPieceCount;
    int totalMaterial;
    int queenCount;
    int rookCount;
    int minorPieceCount;
};

MaterialSummary summarizeMaterial(const Board& board) {
    const Bitboard pawns = board.whitePawns | board.blackPawns;
    const Bitboard knights = board.whiteKnights | board.blackKnights;
    const Bitboard bishops = board.whiteBishops | board.blackBishops;
    const Bitboard rooks = board.whiteRooks | board.blackRooks;
    const Bitboard queens = board.whiteQueens | board.blackQueens;
    const Bitboard nonKingPieces = board.allPieces & ~(board.whiteKings | board.blackKings);

    const int pawnCount = popcount(pawns);
    const int knightCount = popcount(knights);
    const int bishopCount = popcount(bishops);
    const int rookCount = popcount(rooks);
    const int queenCount = popcount(queens);

    MaterialSummary summary{};
    summary.nonKingPieceCount = popcount(nonKingPieces);
    summary.totalMaterial =
        (pawnCount * GamePhaseConstants::kPawnMaterialValue) +
        ((knightCount + bishopCount) * GamePhaseConstants::kMinorMaterialValue) +
        (rookCount * GamePhaseConstants::kRookMaterialValue) +
        (queenCount * GamePhaseConstants::kQueenMaterialValue);
    summary.queenCount = queenCount;
    summary.rookCount = rookCount;
    summary.minorPieceCount = knightCount + bishopCount;
    return summary;
}
} // namespace

bool AdvancedSearch::futilityPruning(const Board& board, int depth, int alpha, int beta,
                                     int staticEval) {

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    int futilityMargin = kFutilityMarginPerDepth * depth;

    if (depth <= kShallowDepthLimit) {
        int tacticalBonus = kTacticalBonusPerMissingPly * (kShallowDepthLimit - depth);
        futilityMargin += tacticalBonus;
    }

    if (staticEval - futilityMargin >= beta) {
        return true;
    }

    if (depth <= kShallowDepthLimit) {
        int deltaMargin = kDeltaMarginBase + (kDeltaMarginPerDepth * depth);
        if (staticEval + deltaMargin < alpha) {
            return true;
        }
    }

    return false;
}

bool AdvancedSearch::staticNullMovePruning(const Board& board, int depth, int alpha, int beta,
                                           int staticEval) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    if (depth <= kStaticNullDepthLimit) {

        int margin = kStaticNullMarginBase + (kStaticNullMarginPerDepth * depth);
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

    if (isInCheck(board, board.turn) || depth < kNullMoveMinDepth) {
        return false;
    }

    const MaterialSummary material = summarizeMaterial(board);
    if (material.nonKingPieceCount < kNullMoveMinMaterialCount ||
        material.totalMaterial < kNullMoveMinTotalMaterial) {
        return false;
    }

    if (material.nonKingPieceCount <= kNullMoveMaxMaterialCount) {
        return false;
    }

    return true;
}

bool AdvancedSearch::multiCutPruning(Board& board, int depth, int alpha, int beta, int r) {
    (void)alpha;

    if (isInCheck(board, board.turn) || depth < kMultiCutMinDepth) {
        return false;
    }

    GenValidMoves(board);
    std::vector<Move> moves = GetAllMoves(board, board.turn);

    if (moves.size() < kMultiCutMinMoves) {
        return false;
    }

    int nullMoveCount = kZero;
    for (int i = kZero; i < r && i < static_cast<int>(moves.size()); i++) {

        Board tempBoard = board;
        tempBoard.turn = (tempBoard.turn == ChessPieceColor::WHITE) ? ChessPieceColor::BLACK
                                                                    : ChessPieceColor::WHITE;

        ThreadSafeHistory historyTable;
        ParallelSearchContext context(kSingleThreadContextCount);
        int nullScore = -AlphaBetaSearch(
            tempBoard, depth - kMultiCutDepthReduction, -beta, -beta + kNarrowWindowWidth,
            !(board.turn == ChessPieceColor::WHITE), kRootPly, historyTable, context);

        if (nullScore >= beta) {
            nullMoveCount++;
        }
    }

    return nullMoveCount >= kMultiCutCutoffCount;
}

bool AdvancedSearch::historyPruning(const Board& board, int depth, const Move& move,
                                    const ThreadSafeHistory& history) {

    if (isInCheck(board, board.turn) || depth == kZero) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int historyScore = history.getScore(move.first, move.second);
    return depth <= kHistoryPruningDepthLimit && historyScore < kHistoryPruningThreshold;
}

bool AdvancedSearch::lateMovePruning(const Board& board, int depth, int moveNumber, bool inCheck) {
    (void)board;

    if (inCheck) {
        return false;
    }

    if (depth == kZero || depth > kShallowDepthLimit) {
        return false;
    }

    if (moveNumber >= kLateMovePruningMoveThreshold && depth <= kShallowDepthLimit) {
        return true;
    }

    return false;
}

bool AdvancedSearch::recaptureExtension(const Board& board, const Move& move, int depth) {
    (void)depth;

    if (board.LastMove < kZero || board.LastMove > kRecaptureMaxLastMove) {
        return false;
    }

    if (move.second == board.LastMove) {

        return true;
    }

    return false;
}

bool AdvancedSearch::checkExtension(const Board& board, const Move& move, int depth) {
    (void)depth;
    return givesCheck(board, move.first, move.second);
}

bool AdvancedSearch::pawnPushExtension(const Board& board, const Move& move, int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    if (board.squares[move.second].piece.PieceType != ChessPieceType::NONE) {
        return false;
    }

    int destRow = move.second / kBoardDimension;
    if (piece.PieceColor == ChessPieceColor::WHITE && destRow >= kPawnPushWhiteAdvancedRank) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && destRow <= kPawnPushBlackAdvancedRank) {
        return true;
    }

    return false;
}

bool AdvancedSearch::passedPawnExtension(const Board& board, const Move& move, int depth) {
    (void)depth;
    const Piece& piece = board.squares[move.first].piece;

    if (piece.PieceType != ChessPieceType::PAWN) {
        return false;
    }

    int srcRow = move.first / kBoardDimension;
    int destRow = move.second / kBoardDimension;

    if (piece.PieceColor == ChessPieceColor::WHITE && destRow - srcRow > kZero) {
        return true;
    }
    if (piece.PieceColor == ChessPieceColor::BLACK && srcRow - destRow > kZero) {
        return true;
    }

    return false;
}

TimeManager::TimeManager(const TimeControl& tc) {
    (void)tc;
}

GamePhase TimeManager::getGamePhase(const Board& board) const {
    const MaterialSummary material = summarizeMaterial(board);

    if (material.totalMaterial > GamePhaseConstants::kOpeningMaterialThreshold &&
        material.queenCount >= GamePhaseConstants::kOpeningMinQueenCount &&
        material.nonKingPieceCount > GamePhaseConstants::kOpeningPieceCountThreshold) {
        return GamePhase::OPENING;
    }

    if (material.totalMaterial < GamePhaseConstants::kEndgameMaterialThreshold ||
        material.nonKingPieceCount <= GamePhaseConstants::kEndgamePieceCountThreshold ||
        (material.queenCount == kZero && material.rookCount <= kEndgameRookCountThreshold &&
         material.minorPieceCount <= kEndgameMinorPieceCountThreshold)) {
        return GamePhase::ENDGAME;
    }

    return GamePhase::MIDDLEGAME;
}
EnhancedOpeningBook::EnhancedOpeningBook(const std::string& bookPath) : bookPath(bookPath) {
    loadBook(bookPath);
}

Move EnhancedOpeningBook::getBestMove(const Board& board, bool randomize) {
    auto it = book.find(boardToKey(board));
    if (it != book.end() && !it->second.empty()) {
        const auto& entries = it->second;
        if (randomize) {
            int totalWeight = kZero;
            for (const auto& entry : entries) {
                totalWeight += entry.weight;
            }
            if (totalWeight > kZero) {
                static std::random_device rd;
                static std::mt19937 gen(static_cast<std::mt19937::result_type>(rd()));
                std::uniform_int_distribution<> dis(kZero, totalWeight - kOne);
                int random = dis(gen);
                int cumulative = kZero;
                for (const auto& entry : entries) {
                    cumulative += entry.weight;
                    if (random < cumulative) {
                        return entry.move;
                    }
                }
            }
            return entries[kFirstMoveIndex].move;
        }

        auto bestIt = std::ranges::max_element(entries, [](const BookEntry& a, const BookEntry& b) {
            if (a.weight != b.weight) {
                return a.weight < b.weight;
            }
            return a.winRate < b.winRate;
        });
        return bestIt->move;
    }

    auto parseBookMoveString = [&](const std::string& moveStr) -> Move {
        if (auto parsed = parseAlgebraicMove(moveStr, board)) {
            return {(parsed->srcRow * kBoardDimension) + parsed->srcCol,
                    (parsed->destRow * kBoardDimension) + parsed->destCol};
        }
        return parseMove(moveStr);
    };

    std::string fen = getFEN(board);
    std::string fallbackMove =
        lookupBookMoveString(fen, OpeningBookOptions, OpeningBook, randomize);
    if (!fallbackMove.empty()) {
        return parseBookMoveString(fallbackMove);
    }

    return {kInvalidSquare, kInvalidSquare};
}

Move EnhancedOpeningBook::parseMove(const std::string& move) {

    if (move.length() < kMinBookMoveLength) {
        return {kInvalidSquare, kInvalidSquare};
    }

    int srcCol{move[kMoveSourceFileIndex] - kBookFileOffset};
    int srcRow{move[kMoveSourceRankIndex] - kBookRankOffset};
    int destCol{move[kMoveDestFileIndex] - kBookFileOffset};
    int destRow{move[kMoveDestRankIndex] - kBookRankOffset};

    if (srcCol < kZero || srcCol >= kBoardDimension || srcRow < kZero ||
        srcRow >= kBoardDimension || destCol < kZero || destCol >= kBoardDimension ||
        destRow < kZero || destRow >= kBoardDimension) {
        return {kInvalidSquare, kInvalidSquare};
    }

    return {(srcRow * kBoardDimension) + srcCol, (destRow * kBoardDimension) + destCol};
}

bool EnhancedOpeningBook::isInBook(const Board& board) {
    return book.contains(boardToKey(board));
}

void EnhancedOpeningBook::loadBook(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return;
    }

    book.clear();
    size_t numPositions{static_cast<size_t>(kZero)};
    file.read(reinterpret_cast<char*>(&numPositions), sizeof(numPositions));

    for (auto i = static_cast<size_t>(kZero); i < numPositions; ++i) {
        auto keyLen = static_cast<size_t>(kZero);
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        std::string key(keyLen, '\0');
        file.read(key.data(), static_cast<std::streamsize>(keyLen));
        auto numMoves = static_cast<size_t>(kZero);
        file.read(reinterpret_cast<char*>(&numMoves), sizeof(numMoves));
        std::vector<BookEntry> entries;
        entries.reserve(numMoves);

        for (auto j = static_cast<size_t>(kZero); j < numMoves; ++j) {
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
    int fieldCount = kZero;
    for (std::size_t i = kZero; i < fen.size(); ++i) {
        if (fen[i] == ' ') {
            fieldCount++;
            if (fieldCount == 4) {
                return fen.substr(kZero, i);
            }
        }
    }
    return fen;
}

EnhancedOpeningBook::BookStats EnhancedOpeningBook::getStats() const {
    BookStats stats{};
    stats.totalPositions = book.size();
    stats.totalMoves = kZero;
    stats.totalGames = kZero;
    float totalWinRate = kZeroFloat;
    float totalRating = kZeroFloat;
    int moveCount = kZero;

    for (const auto& [key, entries] : book) {
        stats.totalMoves += entries.size();
        for (const auto& entry : entries) {
            stats.totalGames += entry.games;
            totalWinRate += entry.winRate;
            totalRating += static_cast<float>(entry.averageRating);
            moveCount++;
        }
    }

    if (moveCount > kZero) {
        stats.averageWinRate = totalWinRate / static_cast<float>(moveCount);
        stats.averageRating = totalRating / static_cast<float>(moveCount);
    }

    return stats;
}
