#pragma once
#include <json.hpp>

namespace jotcad {
namespace geo {

struct Interval {
    double min, max;
    double size() const { return max - min; }
    double center() const { return (min + max) * 0.5; }

    static Interval from_json(const nlohmann::json& j) {
        if (j.is_array()) {
            if (j.size() == 2) return {j[0].get<double>(), j[1].get<double>()};
            if (j.size() == 1) return {0.0, j[0].get<double>()}; // [30] -> [0, 30]
        }
        if (j.is_number()) {
            double v = j.get<double>();
            return {-v * 0.5, v * 0.5}; // 10 -> [-5, 5]
        }
        return {0.0, 0.0};
    }
};

// Global from_json for nlohmann::json ADL support
inline void from_json(const nlohmann::json& j, Interval& i) {
    i = Interval::from_json(j);
}

} // namespace geo
} // namespace jotcad
