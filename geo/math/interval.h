#pragma once
#include <json.hpp>

namespace jotcad {
namespace geo {

struct Interval {
    double min = 1e18, max = -1e18;
    bool empty() const { return min > max; }
    double size() const { return empty() ? 0.0 : (max - min); }
    double center() const { return empty() ? 0.0 : (min + max) * 0.5; }

    void expand(double v) {
        if (v < min) min = v;
        if (v > max) max = v;
    }

    static Interval from_json(const nlohmann::json& j) {
        if (j.is_array()) {
            if (j.size() == 2) return {j[0].get<double>(), j[1].get<double>()};
            if (j.size() == 1) return {0.0, j[0].get<double>()}; // [30] -> [0, 30]
        }
        if (j.is_number()) {
            double v = j.get<double>();
            return {-v * 0.5, v * 0.5}; // 10 -> [-5, 5]
        }
        return {1e18, -1e18}; // Empty
    }
};

// Global from_json for nlohmann::json ADL support
inline void from_json(const nlohmann::json& j, Interval& i) {
    i = Interval::from_json(j);
}

} // namespace geo
} // namespace jotcad
