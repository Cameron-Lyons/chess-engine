#pragma once

// Canonical piece material values. EvaluationParams in EvaluationTuning.h re-exports
// these for tuning; search and board code should use MaterialValues or EvaluationParams.
namespace MaterialValues {
inline constexpr int kPawnValue = 100;
inline constexpr int kKnightValue = 320;
inline constexpr int kBishopValue = 330;
inline constexpr int kRookValue = 500;
inline constexpr int kQueenValue = 900;
inline constexpr int kKingValue = 10000;
} // namespace MaterialValues
