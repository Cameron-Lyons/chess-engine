#ifndef EVALUATION_TUNING_H
#define EVALUATION_TUNING_H

#include "../core/ChessPiece.h"

namespace EvaluationParams {

constexpr int PAWN_VALUE = 100;
constexpr int KNIGHT_VALUE = 320;
constexpr int BISHOP_VALUE = 330;
constexpr int ROOK_VALUE = 500;
constexpr int QUEEN_VALUE = 900;
constexpr int KING_VALUE = 10000;

constexpr int DOUBLED_PAWN_PENALTY = 15;
constexpr int ISOLATED_PAWN_PENALTY = 20;
constexpr int BACKWARD_PAWN_PENALTY = 12;
constexpr int PASSED_PAWN_BONUS[8] = {0, 5, 10, 20, 35, 60, 100, 150};
constexpr int CONNECTED_PAWNS_BONUS = 8;
constexpr int PAWN_CHAIN_BONUS = 12;

constexpr int BISHOP_PAIR_BONUS = 30;
constexpr int KNIGHT_OUTPOST_BONUS = 25;
constexpr int BISHOP_LONG_DIAGONAL_BONUS = 15;
constexpr int ROOK_OPEN_FILE_BONUS = 20;
constexpr int ROOK_SEMI_OPEN_FILE_BONUS = 10;
constexpr int QUEEN_EARLY_DEVELOPMENT_PENALTY = 30;

constexpr int KING_ATTACK_WEIGHTS[6] = {0, 50, 75, 88, 94, 97};
constexpr int KING_DANGER_MULTIPLIER = 15;
constexpr int CASTLING_BONUS = 40;
constexpr int KING_PAWN_SHIELD_BONUS = 15;
constexpr int KING_OPEN_FILE_PENALTY = 25;

constexpr int PIECE_MOBILITY_WEIGHT = 4;
constexpr int CENTER_CONTROL_BONUS = 8;
constexpr int PIECE_COORDINATION_BONUS = 5;
constexpr int TACTICAL_MOTIFS_BONUS = 10;

constexpr int KING_ACTIVITY_ENDGAME_BONUS = 20;
constexpr int OPPOSITION_BONUS = 15;
constexpr int PAWN_RACE_BONUS = 50;
constexpr int PIECE_TRADE_ENDGAME_BONUS = 8;

constexpr int TEMPO_BONUS = 10;
constexpr int INITIATIVE_BONUS = 15;
constexpr int PRESSURE_BONUS = 8;

extern const int TUNED_PAWN_MG[64];
extern const int TUNED_PAWN_EG[64];

extern const int TUNED_KNIGHT_MG[64];
extern const int TUNED_KNIGHT_EG[64];

extern const int TUNED_BISHOP_MG[64];
extern const int TUNED_BISHOP_EG[64];

extern const int TUNED_ROOK_MG[64];
extern const int TUNED_ROOK_EG[64];

extern const int TUNED_QUEEN_MG[64];
extern const int TUNED_QUEEN_EG[64];

extern const int TUNED_KING_MG[64];
extern const int TUNED_KING_EG[64];

constexpr int PHASE_WEIGHTS[6] = {0, 1, 1, 2, 4, 0};
constexpr int TOTAL_PHASE = 24;

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

#include "../core/ChessBoard.h"
#include <string>
#include <vector>

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
    double getError() const { return computeError(params); }
};

#endif