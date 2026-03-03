#pragma once

namespace GamePhaseConstants {
inline constexpr int kQueenMaterialValue = 900;
inline constexpr int kRookMaterialValue = 500;
inline constexpr int kMinorMaterialValue = 300;
inline constexpr int kPawnMaterialValue = 100;

inline constexpr int kOpeningMaterialThreshold = 6000;
inline constexpr int kOpeningPieceCountThreshold = 20;
inline constexpr int kOpeningMinQueenCount = 1;

inline constexpr int kEndgameMaterialThreshold = 2000;
inline constexpr int kEndgamePieceCountThreshold = 10;
} // namespace GamePhaseConstants
