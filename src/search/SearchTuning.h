#pragma once

#include "../utils/TunableParams.h"

namespace SearchTuning {

[[nodiscard]] inline int aspirationWindow() {
    return TUNABLE_ASPIRATION_WINDOW;
}

[[nodiscard]] inline int nullMoveReduction() {
    return TUNABLE_NULL_MOVE_R;
}

[[nodiscard]] inline int futilityMargin(int depth) {
    return TUNABLE_FUTILITY_BASE + (depth * (TUNABLE_FUTILITY_MULT / 2));
}

[[nodiscard]] inline double lmrBaseOffset() {
    return static_cast<double>(TUNABLE_LMR_BASE) / 100.0;
}

[[nodiscard]] inline double lmrLogDivisor() {
    return static_cast<double>(TUNABLE_LMR_DIVISOR) / 100.0;
}

[[nodiscard]] inline int historyGravity() {
    return TUNABLE_HISTORY_GRAVITY;
}

[[nodiscard]] inline int iirDepth() {
    return TUNABLE_IIR_DEPTH;
}

[[nodiscard]] inline int razoringMargin() {
    return TUNABLE_RAZORING_MARGIN;
}

[[nodiscard]] inline int deltaMargin() {
    return TUNABLE_DELTA_MARGIN;
}

} // namespace SearchTuning
