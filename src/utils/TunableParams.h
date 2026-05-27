#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct TunableParam {
    std::string name;
    std::reference_wrapper<int> value;
    int defaultValue;
    int minValue;
    int maxValue;
    int step;
};

class TunableRegistry {
    std::vector<TunableParam> params;
    TunableRegistry() = default;

public:
    static TunableRegistry& instance() {
        static TunableRegistry reg;
        return reg;
    }

    void add(std::string_view name, std::reference_wrapper<int> value, int def, int min, int max,
             int step) {
        value.get() = def;
        params.push_back({std::string(name), value, def, min, max, step});
    }

    [[nodiscard]] const std::vector<TunableParam>& all() const {
        return params;
    }

    bool set(std::string_view name, int val) {
        for (auto& param : params) {
            if (param.name == name) {
                if (val >= param.minValue && val <= param.maxValue) {
                    param.value.get() = val;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    [[nodiscard]] const TunableParam* find(std::string_view name) const {
        for (const auto& param : params) {
            if (param.name == name) {
                return &param;
            }
        }
        return nullptr;
    }

    void reset() {
        for (auto& param : params) {
            param.value.get() = param.defaultValue;
        }
    }
};

#define TUNABLE(name, def, min, max, step)                                                         \
    inline int TUNABLE_##name = def;                                                               \
    inline const bool TUNABLE_REGISTERED_##name = [] {                                             \
        TunableRegistry::instance().add(#name, std::ref(TUNABLE_##name), def, min, max, step);     \
        return true;                                                                               \
    }()

TUNABLE(LMR_BASE, 75, 50, 120, 5);
TUNABLE(LMR_DIVISOR, 300, 200, 500, 10);
TUNABLE(FUTILITY_BASE, 100, 50, 200, 10);
TUNABLE(FUTILITY_MULT, 100, 50, 200, 10);
TUNABLE(NULL_MOVE_R, 3, 2, 4, 1);
TUNABLE(ASPIRATION_WINDOW, 50, 20, 100, 5);
TUNABLE(SE_MARGIN_BASE, 2, 1, 4, 1);
TUNABLE(RAZORING_MARGIN, 300, 200, 500, 20);
TUNABLE(DELTA_MARGIN, 200, 100, 400, 20);
TUNABLE(HISTORY_GRAVITY, 16384, 8192, 32768, 1024);
TUNABLE(IIR_DEPTH, 4, 3, 6, 1);
