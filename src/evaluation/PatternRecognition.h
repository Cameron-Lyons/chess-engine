#pragma once

#include "../core/Bitboard.h"
#include "../core/ChessBoard.h"

#include <array>
#include <unordered_map>
#include <vector>

class PatternRecognition {
public:
    struct TacticalPattern {
        std::string name;
        int bonus;
        bool isWhite;
        std::vector<std::pair<int, int>> squares;

        TacticalPattern(const std::string& n, int b, bool white,
                        const std::vector<std::pair<int, int>>& sqs)
            : name(n), bonus(b), isWhite(white), squares(sqs) {}
    };

    struct StrategicPattern {
        std::string name;
        int bonus;
        float phaseWeight;
        std::vector<std::pair<int, int>> squares;

        StrategicPattern(const std::string& n, int b, float pw,
                         const std::vector<std::pair<int, int>>& sqs)
            : name(n), bonus(b), phaseWeight(pw), squares(sqs) {}
    };

    struct EndgamePattern {
        std::string name;
        int bonus;
        std::vector<ChessPieceType> requiredPieces;

        EndgamePattern(const std::string& n, int b, const std::vector<ChessPieceType>& pieces)
            : name(n), bonus(b), requiredPieces(pieces) {}
    };

    struct PatternResult {
        std::vector<TacticalPattern> tacticalPatterns;
        std::vector<StrategicPattern> strategicPatterns;
        std::vector<EndgamePattern> endgamePatterns;
        int totalBonus;

        PatternResult() : totalBonus(0) {}
        void clear();
        void addTacticalPattern(const TacticalPattern& pattern);
        void addStrategicPattern(const StrategicPattern& pattern);
        void addEndgamePattern(const EndgamePattern& pattern);
    };

    static PatternResult recognizePatterns(const Board& board);

    static std::vector<TacticalPattern> recognizeTacticalPatterns(const Board& board);
    static std::vector<TacticalPattern> recognizeForkPatterns(const Board& board);
    static std::vector<TacticalPattern> recognizePinPatterns(const Board& board);
    static std::vector<TacticalPattern> recognizeSkewerPatterns(const Board& board);
    static std::vector<TacticalPattern> recognizeDiscoveredAttackPatterns(const Board& board);
    static std::vector<TacticalPattern> recognizeDoubleCheckPatterns(const Board& board);

    static std::vector<StrategicPattern> recognizeStrategicPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeOutpostPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeWeakSquarePatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeHolePatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeBatteryPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeOverloadedPiecePatterns(const Board& board);

    static std::vector<EndgamePattern> recognizeEndgamePatterns(const Board& board);
    static std::vector<EndgamePattern> recognizePawnEndgamePatterns(const Board& board);
    static std::vector<EndgamePattern> recognizeRookEndgamePatterns(const Board& board);
    static std::vector<EndgamePattern> recognizeBishopEndgamePatterns(const Board& board);
    static std::vector<EndgamePattern> recognizeKnightEndgamePatterns(const Board& board);
    static std::vector<EndgamePattern> recognizeQueenEndgamePatterns(const Board& board);

    static std::vector<StrategicPattern> recognizePawnStructurePatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeIsolatedPawnPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeDoubledPawnPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeBackwardPawnPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizePassedPawnPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeConnectedPawnPatterns(const Board& board);

    static std::vector<StrategicPattern> recognizeKingSafetyPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizePawnShieldPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeOpenFilePatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeKingAttackPatterns(const Board& board);

    static std::vector<StrategicPattern> recognizePieceCoordinationPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeBishopPairPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeRookCoordinationPatterns(const Board& board);
    static std::vector<StrategicPattern> recognizeKnightCoordinationPatterns(const Board& board);

    static bool isSquareAttacked(const Board& board, int square, ChessPieceColor byColor);
    static bool isSquareDefended(const Board& board, int square, ChessPieceColor byColor);
    static int getAttackCount(const Board& board, int square, ChessPieceColor byColor);
    static int getDefenseCount(const Board& board, int square, ChessPieceColor byColor);
    static bool isPiecePinned(const Board& board, int square, ChessPieceColor color);
    static bool isPieceOverloaded(const Board& board, int square, ChessPieceColor color);
    static float getGamePhase(const Board& board);
    static bool isEndgame(const Board& board);
    static bool isPawnEndgame(const Board& board);

    static Bitboard getPawnAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getKnightAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getBishopAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getRookAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getQueenAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getKingAttacks(ChessPieceColor color, const Board& board);
    static Bitboard getAllAttacks(ChessPieceColor color, const Board& board);

    static void initializePatternDatabase();
    static std::unordered_map<std::string, std::vector<std::pair<int, int>>> tacticalPatternDB;
    static std::unordered_map<std::string, std::vector<std::pair<int, int>>> strategicPatternDB;
    static std::unordered_map<std::string, std::vector<ChessPieceType>> endgamePatternDB;

private:
    static bool matchesPattern(const Board& board, const std::vector<std::pair<int, int>>& pattern,
                               ChessPieceColor color);
    static bool matchesPiecePattern(const Board& board,
                                    const std::vector<std::pair<int, int>>& pattern,
                                    ChessPieceType pieceType, ChessPieceColor color);
    static int calculatePatternBonus(const Board& board,
                                     const std::vector<std::pair<int, int>>& pattern,
                                     int baseBonus);
    static bool isPatternThreatened(const Board& board,
                                    const std::vector<std::pair<int, int>>& pattern,
                                    ChessPieceColor color);
    static bool isPatternDefended(const Board& board,
                                  const std::vector<std::pair<int, int>>& pattern,
                                  ChessPieceColor color);

    static bool isFork(const Board& board, int fromSquare, int toSquare);
    static bool isPin(const Board& board, int fromSquare, int toSquare);
    static bool isSkewer(const Board& board, int fromSquare, int toSquare);
    static bool isDiscoveredAttack(const Board& board, int fromSquare, int toSquare);
    static bool isDoubleCheck(const Board& board, int fromSquare, int toSquare);

    static int evaluateSquareControl(const Board& board, int square);
    static int evaluatePieceMobility(const Board& board, ChessPieceColor color);
    static int evaluateKingDistance(const Board& board, ChessPieceColor color);
    static int evaluatePawnStructure(const Board& board, ChessPieceColor color);
    static int evaluatePieceCoordination(const Board& board, ChessPieceColor color);
};

namespace PatternScores {

constexpr int FORK_BONUS = 150;
constexpr int PIN_BONUS = 100;
constexpr int SKEWER_BONUS = 120;
constexpr int DISCOVERED_ATTACK_BONUS = 80;
constexpr int DOUBLE_CHECK_BONUS = 200;

constexpr int OUTPOST_BONUS = 50;
constexpr int WEAK_SQUARE_PENALTY = -30;
constexpr int HOLE_PENALTY = -40;
constexpr int BATTERY_BONUS = 60;
constexpr int OVERLOADED_PIECE_PENALTY = -50;

constexpr int ISOLATED_PAWN_PENALTY = -20;
constexpr int DOUBLED_PAWN_PENALTY = -15;
constexpr int BACKWARD_PAWN_PENALTY = -12;
constexpr int PASSED_PAWN_BONUS = 30;
constexpr int CONNECTED_PAWN_BONUS = 8;

constexpr int PAWN_SHIELD_BONUS = 15;
constexpr int OPEN_FILE_PENALTY = -25;
constexpr int KING_ATTACK_BONUS = 100;

constexpr int BISHOP_PAIR_BONUS = 30;
constexpr int ROOK_COORDINATION_BONUS = 20;
constexpr int KNIGHT_COORDINATION_BONUS = 15;
} // namespace PatternScores
