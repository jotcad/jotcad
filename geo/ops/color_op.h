#pragma once
#include "protocols.h"
#include "processor.h"

#include "render/contour_utils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ColorOp : P {
    static constexpr const char* path = "jot/color";

    static std::string parse_hex_alpha(const std::string& hex, std::optional<double>& alpha_out) {
        if (hex.size() == 9 && hex[0] == '#') {
            unsigned int a_int = 255;
            std::stringstream ss;
            ss << std::hex << hex.substr(7, 2);
            ss >> a_int;
            alpha_out = a_int / 255.0;
            return hex.substr(0, 7);
        }
        return hex;
    }

    static std::string blend_colors(const std::string& existing, const std::string& target, double mix) {
        if (mix >= 1.0 || existing.empty()) {
            return target;
        }
        auto c1 = ContourUtils::parse_color(existing);
        auto c2 = ContourUtils::parse_color(target);
        double clamped_mix = std::clamp(mix, 0.0, 1.0);
        uint8_t r = (uint8_t)std::round((1.0 - clamped_mix) * c1.r + clamped_mix * c2.r);
        uint8_t g = (uint8_t)std::round((1.0 - clamped_mix) * c1.g + clamped_mix * c2.g);
        uint8_t b = (uint8_t)std::round((1.0 - clamped_mix) * c1.b + clamped_mix * c2.b);
        std::stringstream ss;
        ss << "#" << std::hex << std::setfill('0') << std::setw(2) << (int)r
           << std::setfill('0') << std::setw(2) << (int)g
           << std::setfill('0') << std::setw(2) << (int)b;
        return ss.str();
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& color = "", double mix = 1.0, double opaque = -1.0) {
        std::optional<double> alpha_extracted;
        std::string clean_color = color.empty() ? "" : parse_hex_alpha(color, alpha_extracted);

        std::optional<double> final_opaque;
        if (opaque >= 0.0) {
            final_opaque = opaque;
        } else if (alpha_extracted.has_value()) {
            final_opaque = alpha_extracted;
        }

        Shape out = in.map([&](Shape node) {
            if (!clean_color.empty()) {
                std::string existing = node.tags.contains("color") ? node.tags["color"].get<std::string>() : "";
                node.tags["color"] = blend_colors(existing, clean_color, mix);
            }
            if (final_opaque.has_value()) {
                node.tags["opacity"] = final_opaque.value();
            }
            return node;
        });

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "color", "mix", "opaque"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/color"},
            {"description", "Applies a color tag, mix wash, and/or translucency opacity to the shape."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape to color."}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "color"}, {"type", "jot:string"}, {"default", ""}, {"description", "Color name, 6-hex (#RRGGBB), or 8-hex with alpha (#RRGGBBAA)."}},
                {{"name", "mix"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "Color blend / wash factor (0.0 = keep existing, 1.0 = full replacement)."}},
                {{"name", "opaque"}, {"type", "jot:number"}, {"default", -1.0}, {"description", "Opacity level (0.0 = fully transparent, 1.0 = fully opaque)."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The colored shape."}}}}}
        };
    }
};

static void color_init(fs::VFSNode* vfs) {
    Processor::register_op<ColorOp<>, Shape, std::string, double, double>(vfs, "jot/color");
}

} // namespace geo
} // namespace jotcad
