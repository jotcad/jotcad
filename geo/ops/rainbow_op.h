#pragma once
#include "protocols.h"
#include "processor.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RainbowOp : P {
    static constexpr const char* path = "jot/rainbow";

    // Helper to convert HSL to CSS Hex string
    static std::string hsl_to_hex(double h, double s, double l) {
        double c = (1.0 - std::abs(2.0 * l - 1.0)) * s;
        double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
        double m = l - c / 2.0;
        double r = 0, g = 0, b = 0;
        if (h >= 0 && h < 60) {
            r = c; g = x; b = 0;
        } else if (h >= 60 && h < 120) {
            r = x; g = c; b = 0;
        } else if (h >= 120 && h < 180) {
            r = 0; g = c; b = x;
        } else if (h >= 180 && h < 240) {
            r = 0; g = x; b = c;
        } else if (h >= 240 && h < 300) {
            r = x; g = 0; b = c;
        } else if (h >= 300 && h <= 360) {
            r = c; g = 0; b = x;
        }
        int ri = std::round((r + m) * 255.0);
        int gi = std::round((g + m) * 255.0);
        int bi = std::round((b + m) * 255.0);
        
        ri = std::max(0, std::min(255, ri));
        gi = std::max(0, std::min(255, gi));
        bi = std::max(0, std::min(255, bi));

        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", ri, gi, bi);
        return std::string(buf);
    }

    static void count_geometry_components(const Shape& s, size_t& count) {
        if (s.geometry.has_value()) {
            count++;
        }
        for (const auto& child : s.components) {
            count_geometry_components(child, count);
        }
    }

    static void color_shapes_recursive(const std::vector<std::string>& colors, size_t& color_idx, Shape& s) {
        if (s.geometry.has_value() && color_idx < colors.size()) {
            s.add_tag("color", colors[color_idx]);
            color_idx++;
        }
        for (auto& child : s.components) {
            color_shapes_recursive(colors, color_idx, child);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        
        // 1. Count components with geometry
        size_t total_geom = 0;
        count_geometry_components(out, total_geom);

        // 2. Generate evenly spaced colors in HSL space (Saturation=0.85, Lightness=0.55)
        std::vector<std::string> colors;
        if (total_geom > 0) {
            colors.reserve(total_geom);
            for (size_t i = 0; i < total_geom; ++i) {
                double hue = (double)i * (360.0 / (double)total_geom);
                colors.push_back(hsl_to_hex(hue, 0.85, 0.55));
            }
        }

        // 3. Apply colors recursively
        size_t color_idx = 0;
        color_shapes_recursive(colors, color_idx, out);

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rainbow"},
            {"description", "Recursively assigns a distinct color to each shape component that has geometry, maximizing color distance using an HSL model."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape hierarchy to color."}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void rainbow_init(fs::VFSNode* vfs) {
    Processor::register_op<RainbowOp<>, Shape>(vfs, "jot/rainbow");
}

} // namespace geo
} // namespace jotcad
