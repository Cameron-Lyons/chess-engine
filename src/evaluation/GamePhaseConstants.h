#pragma once

#include "EvaluationTuning.h"

namespace GamePhaseConstants {
inline constexpr int kQueenMaterialValue = EvaluationParams::QUEEN_VALUE;
inline constexpr int kRookMaterialValue = EvaluationParams::ROOK_VALUE;
inline constexpr int kMinorMaterialValue = EvaluationParams::KNIGHT_VALUE;
inline constexpr int kPawnMaterialValue = EvaluationParams::PAWN_VALUE;

inline constexpr int kOpeningMaterialThreshold = EvaluationParams::OPENING_MATERIAL_THRESHOLD;
inline constexpr int kOpeningPieceCountThreshold = EvaluationParams::OPENING_PIECE_COUNT_THRESHOLD;
inline constexpr int kOpeningMinQueenCount = EvaluationParams::OPENING_MIN_QUEEN_COUNT;

inline constexpr int kEndgameMaterialThreshold = EvaluationParams::ENDGAME_MATERIAL_THRESHOLD;
inline constexpr int kEndgamePieceCountThreshold = EvaluationParams::ENDGAME_PIECE_COUNT_THRESHOLD;
} // namespace GamePhaseConstants
