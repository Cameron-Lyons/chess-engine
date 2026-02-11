#pragma once

#include <functional>
#include <string>
#include <vector>

struct TunableParam {
    std::string name;
    int* value;
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

    void add(const std::string& name, int* value, int def, int min, int max, int step) {
        *value = def;
        params.push_back({name, value, def, min, max, step});
    }

    const std::vector<TunableParam>& all() const { return params; }

    bool set(const std::string& name, int val) {
        for (auto& p : params) {
            if (p.name == name) {
                if (val >= p.minValue && val <= p.maxValue) {
                    *p.value = val;
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    const TunableParam* find(const std::string& name) const {
        for (const auto& p : params) {
            if (p.name == name) return &p;
        }
        return nullptr;
    }

    void reset() {
        for (auto& p : params) {
            *p.value = p.defaultValue;
        }
    }
};

struct TunableInit {
    TunableInit(const std::string& name, int* value, int def, int min, int max, int step) {
        TunableRegistry::instance().add(name, value, def, min, max, step);
    }
};

#define TUNABLE(name, def, min, max, step) \
    inline int TUNABLE_##name = def; \
    inline TunableInit TUNABLE_INIT_##name(#name, &TUNABLE_##name, def, min, max, step)

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
