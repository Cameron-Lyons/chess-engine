#pragma once

#include "../core/ChessBoard.h"
#include "../core/ChessPiece.h"
#include "../core/MaterialValues.h"

#include <array>
#include <string>
#include <vector>

namespace EvaluationParams {

// Piece material (aliases MaterialValues — single numeric source)
constexpr int PAWN_VALUE = MaterialValues::kPawnValue;
constexpr int KNIGHT_VALUE = MaterialValues::kKnightValue;
constexpr int BISHOP_VALUE = MaterialValues::kBishopValue;
constexpr int ROOK_VALUE = MaterialValues::kRookValue;
constexpr int QUEEN_VALUE = MaterialValues::kQueenValue;
constexpr int KING_VALUE = MaterialValues::kKingValue;

// Pawn structure
constexpr int DOUBLED_PAWN_PENALTY = 15;
constexpr int ISOLATED_PAWN_PENALTY = 20;
constexpr int BACKWARD_PAWN_PENALTY = 12;
inline constexpr std::array<int, 8> PASSED_PAWN_BONUS = {0, 5, 10, 20, 35, 60, 100, 150};
constexpr int PASSED_PAWN_RANK_SCALE = 20;
constexpr int PASSED_PAWN_BASE_BONUS = 10;
constexpr int CONNECTED_PAWNS_BONUS = 8;
constexpr int PAWN_CHAIN_BONUS = 12;

// Piece placement / coordination
constexpr int BISHOP_PAIR_BONUS = 30;
constexpr int KNIGHT_OUTPOST_BONUS = 25;
constexpr int BISHOP_LONG_DIAGONAL_BONUS = 15;
constexpr int ROOK_OPEN_FILE_BONUS = 20;
constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 10;
constexpr int QUEEN_EARLY_DEVELOPMENT_PENALTY = 30;

// King safety
inline constexpr std::array<int, 6> KING_ATTACK_WEIGHTS = {0, 50, 75, 88, 94, 97};
constexpr int KING_DANGER_MULTIPLIER = 15;
constexpr int CASTLING_BONUS = 40;
constexpr int KING_PAWN_SHIELD_BONUS = 15;
constexpr int KING_OPEN_FILE_PENALTY = 25;

// Positional
constexpr int PIECE_MOBILITY_WEIGHT = 4;
constexpr int CENTER_CONTROL_BONUS = 8;
constexpr int PIECE_COORDINATION_BONUS = 5;
constexpr int TACTICAL_MOTIFS_BONUS = 10;

// Endgame
constexpr int KING_ACTIVITY_ENDGAME_BONUS = 20;
constexpr int OPPOSITION_BONUS = 15;
constexpr int PAWN_RACE_BONUS = 50;
constexpr int PIECE_TRADE_ENDGAME_BONUS = 8;
constexpr int KING_CENTER_DISTANCE_TARGET = 7;
constexpr int KING_CENTRALIZATION_SCALE = 5;

// Initiative
constexpr int TEMPO_BONUS = 10;
constexpr int INITIATIVE_BONUS = 15;
constexpr int PRESSURE_BONUS = 8;

// Game phase
inline constexpr std::array<int, 6> PHASE_WEIGHTS = {0, 1, 1, 2, 4, 0};
constexpr int TOTAL_PHASE = 24;
constexpr int OPENING_ENDGAME_BOUNDARY_PHASE = 12;
constexpr int QUEEN_GAME_PHASE_INCREMENT = 4;
constexpr int OPENING_MATERIAL_THRESHOLD = 6000;
constexpr int OPENING_PIECE_COUNT_THRESHOLD = 20;
constexpr int OPENING_MIN_QUEEN_COUNT = 1;
constexpr int ENDGAME_MATERIAL_THRESHOLD = 2000;
constexpr int ENDGAME_PIECE_COUNT_THRESHOLD = 10;

// MG/EG interpolation scales
constexpr float PAWN_MG_PASSED_SCALE = 0.8F;
constexpr float PAWN_EG_SCALE = 1.2F;
constexpr float PASSED_PAWN_EG_SCALE = 1.5F;
constexpr float MOBILITY_EG_SCALE = 0.8F;
constexpr float KING_SAFETY_EG_SCALE = 0.3F;
constexpr float BISHOP_PAIR_EG_SCALE = 1.5F;
constexpr float TACTICAL_SAFETY_EG_SCALE = 0.7F;
constexpr float QUEEN_TRAP_EG_SCALE = 0.5F;

// Hanging / tactical penalties
constexpr float HANGING_PENALTY_RATIO = 0.8F;
constexpr int HANGING_QUEEN_PENALTY = 500;
constexpr int QUEEN_UNSUPPORTED_PENALTY = 300;
constexpr int QUEEN_CROWDED_PENALTY = 200;
constexpr int QUEEN_CORNER_TRAP_PENALTY = 400;
constexpr int QUEEN_EDGE_TRAP_PENALTY = 150;
constexpr int QUEEN_ENEMY_TERRITORY_WHITE_ROW = 5;
constexpr int QUEEN_ENEMY_TERRITORY_BLACK_ROW = 2;
constexpr int NEARBY_ENEMY_RADIUS = 2;
constexpr int NEARBY_ENEMY_THRESHOLD = 2;
constexpr int DANGER_LEVEL_SCALE = 300;
constexpr int HIGH_VALUE_PIECE_THRESHOLD = 300;
constexpr int PENALTY_DIVISOR = 4;
constexpr int QUEEN_TACTICAL_PENALTY = 800;
constexpr int NEAR_ENEMY_KING_DISTANCE = 2;
constexpr int NEAR_ENEMY_KING_LOW_DEFENDER_THRESHOLD = 2;
constexpr int NEAR_ENEMY_KING_PENALTY = 250;

// Queen trap danger
constexpr int QUEEN_TRAP_NO_ESCAPE_PENALTY = 800;
constexpr int QUEEN_TRAP_LOW_ESCAPE_PENALTY = 400;
constexpr int QUEEN_TRAP_MEDIUM_ESCAPE_PENALTY = 200;
constexpr int QUEEN_TRAP_LOW_ESCAPE_THRESHOLD = 2;
constexpr int QUEEN_TRAP_MEDIUM_ESCAPE_THRESHOLD = 4;
constexpr int QUEEN_TRAP_CORNER_ESCAPE_THRESHOLD = 3;
constexpr int QUEEN_TRAP_EDGE_ESCAPE_THRESHOLD = 5;
constexpr int QUEEN_TRAP_CORNER_PENALTY = 600;
constexpr int QUEEN_TRAP_EDGE_PENALTY = 300;
constexpr int QUEEN_ESCAPE_SEARCH_RADIUS = 2;

using PieceSquareTable = std::array<int, 64>;

extern const PieceSquareTable TUNED_PAWN_MG;
extern const PieceSquareTable TUNED_PAWN_EG;

extern const PieceSquareTable TUNED_KNIGHT_MG;
extern const PieceSquareTable TUNED_KNIGHT_EG;

extern const PieceSquareTable TUNED_BISHOP_MG;
extern const PieceSquareTable TUNED_BISHOP_EG;

extern const PieceSquareTable TUNED_ROOK_MG;
extern const PieceSquareTable TUNED_ROOK_EG;

extern const PieceSquareTable TUNED_QUEEN_MG;
extern const PieceSquareTable TUNED_QUEEN_EG;

extern const PieceSquareTable TUNED_KING_MG;
extern const PieceSquareTable TUNED_KING_EG;

int getMaterialValue(ChessPieceType piece);
int getTunedPST(ChessPieceType piece, int square, bool isEndgame);
int interpolatePhase(int mgScore, int egScore, int phase);
void logEvaluationComponents(const char* component, int value);

constexpr bool ENABLE_PAWN_STRUCTURE = true;
constexpr bool ENABLE_PIECE_MOBILITY = true;
constexpr bool ENABLE_KING_SAFETY = true;
constexpr bool ENABLE_PIECE_COORDINATION = true;
constexpr bool ENABLE_ENDGAME_KNOWLEDGE = true;
constexpr bool ENABLE_TACTICAL_BONUSES = true;

} // namespace EvaluationParams

struct TuningPosition {
    std::string fen;
    double result;
};

class TexelTuner {
    std::vector<TuningPosition> positions;
    std::vector<int> params;
    double scalingK = 1.13;
    double sigmoid(double eval) const;
    double computeError(const std::vector<int>& p) const;
    int evaluateWithParams(const Board& board, const std::vector<int>& p) const;
    void findOptimalK();

public:
    bool loadPositions(const std::string& filename);
    void optimize(int iterations);
    void exportParams(const std::string& filename) const;
    void initParams();
};
