#pragma once

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "NNUE.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

inline constexpr int KING_SAFETY_PAWN_SHIELD_BONUS = 10;
inline constexpr int KING_SAFETY_OPEN_FILE_PENALTY = 20;
inline constexpr int KING_SAFETY_SEMI_OPEN_FILE_PENALTY = 10;
inline constexpr std::array<int, 7> KING_SAFETY_ATTACK_WEIGHT = {0, 1, 2, 3, 5, 8, 12};
inline constexpr int KING_SAFETY_ATTACK_UNITS_MAX = 100;

inline constexpr std::array<int, 64> PAWN_TABLE = {
    0,  0,   0,  0, 0,  0,  0,  0,   50,  50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30,  20,
    10, 10,  5,  5, 10, 25, 25, 10,  5,   5,  0,  0,  0,  20, 20, 0,  0,  0,  5,  -5, -10, 0,
    0,  -10, -5, 5, 5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,  0,  0,  0,  0,  0};

inline constexpr std::array<int, 64> KNIGHT_TABLE = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40,
    -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30,
    -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,   10,  15,  15,  10,  5,   -30,
    -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

inline constexpr std::array<int, 64> BISHOP_TABLE = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,   0,   0,   -10,
    -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10,
    -10, 0,   10,  10,  10,  10,  0,   -10, -10, 10,  10,  10,  10,  10,  10,  -10,
    -10, 5,   0,   0,   0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

inline constexpr std::array<int, 64> ROOK_TABLE = {
    0, 0,  0,  0,  0,  0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, -5, 0,  0,  0, 0, 0,
    0, -5, -5, 0,  0,  0, 0, 0, 0, -5, -5, 0,  0,  0,  0,  0, 0,  -5, -5, 0, 0, 0,
    0, 0,  0,  -5, -5, 0, 0, 0, 0, 0,  0,  -5, 0,  0,  0,  5, 5,  0,  0,  0};

inline constexpr std::array<int, 64> QUEEN_TABLE = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10,
    -10, 0,   5,   5,  5,  5,   0,   -10, -5,  0,   5,   5,  5,  5,   0,   -5,
    0,   0,   5,   5,  5,  5,   0,   -5,  -10, 5,   5,   5,  5,  5,   0,   -10,
    -10, 0,   5,   0,  0,  0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20};

inline constexpr std::array<int, 64> KING_TABLE = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
    -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20, -20, -20, -10,
    20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};

int getPieceSquareValue(ChessPieceType pieceType, int position, ChessPieceColor color);
int evaluatePawnStructure(const Board& board);
int evaluateMobility(const Board& board);
int evaluateCenterControl(const Board& board);
int evaluateKingSafety(const Board& board, ChessPieceColor color);
int evaluateHangingPieces(const Board& board);
int evaluateQueenTrapDanger(const Board& board);
int evaluateTacticalSafety(const Board& board);
bool canPieceAttackSquare(const Board& board, int piecePos, int targetPos);
int evaluatePosition(const Board& board, int contempt = 0);
int evaluateKingSafetyForColor(const Board& board, int kingPos, ChessPieceColor color);
int evaluatePassedPawns(const Board& board);
int evaluateBishopPair(const Board& board);
int evaluateRooksOnOpenFiles(const Board& board);
int evaluateEndgame(const Board& board);

struct PawnHashEntry {
    std::uint64_t key = 0;
    int mgScore = 0;
    int egScore = 0;
};

struct PawnHashScore {
    int mgScore = 0;
    int egScore = 0;
};

struct PawnHashTable {
    static constexpr std::size_t SIZE = 16384;
    std::array<PawnHashEntry, SIZE> entries{};

    PawnHashTable() {
        clear();
    }
    void clear() {
        entries.fill({});
    }

    std::optional<PawnHashScore> probe(std::uint64_t key) const {
        const auto& e = entries[key % SIZE];
        if (e.key == key) {
            return PawnHashScore{e.mgScore, e.egScore};
        }
        return std::nullopt;
    }

    void store(std::uint64_t key, int mg, int eg) {
        auto& e = entries[key % SIZE];
        e.key = key;
        e.mgScore = mg;
        e.egScore = eg;
    }
};

std::uint64_t computePawnHash(const Board& board);

void setNNUEEnabled(bool enabled);
void setFastEvaluationMode(bool enabled);
bool isFastEvaluationMode();
