#pragma once

#include <string>
#include <vector>
#include <map>
#include <json.hpp>

namespace jotcad {
namespace geo {

/**
 * Color Utility: Standardizes color handling across the C++ mesh.
 * Aligns with JotCAD's standardColorDefinitions.js.
 */
class Color {
public:
    using RGB = std::vector<double>;

    static RGB from_tags(const nlohmann::json& tags, const RGB& fallback = {0,0,0}) {
        if (!tags.is_object()) return fallback;
        
        // Proper key-value split: {"color": "red"} or {"color": "#ff0000"}
        if (tags.contains("color")) {
            auto val = tags["color"];
            if (val.is_string()) {
                std::string c = val.get<std::string>();
                return from_string(c, fallback);
            }
        }
        
        // Support legacy/flat tags if present: {"color:red": true}
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            if (it.key().find("color:") == 0) {
                return from_string(it.key().substr(6), fallback);
            }
        }

        return fallback;
    }

    static RGB from_string(const std::string& color, const RGB& fallback = {0,0,0}) {
        if (color.empty()) return fallback;
        
        // 1. Hex parsing
        if (color[0] == '#') {
            return from_hex(color, fallback);
        }

        // 2. Named color lookup
        auto& map = get_map();
        if (map.count(color)) {
            return from_hex(map.at(color), fallback);
        }

        return fallback;
    }

    static RGB from_hex(const std::string& hex, const RGB& fallback = {0,0,0}) {
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        if (h.size() == 6) {
            try {
                int r = std::stoi(h.substr(0, 2), nullptr, 16);
                int g = std::stoi(h.substr(2, 2), nullptr, 16);
                int b = std::stoi(h.substr(4, 2), nullptr, 16);
                return {r / 255.0, g / 255.0, b / 255.0};
            } catch (...) {}
        }
        return fallback;
    }

private:
    static std::map<std::string, std::string>& get_map() {
        static std::map<std::string, std::string> map;
        if (map.empty()) {
            map["black"] = "#000000";
            map["white"] = "#ffffff";
            map["red"] = "#ff0000";
            map["green"] = "#008000";
            map["blue"] = "#0000ff";
            map["yellow"] = "#ffff00";
            map["cyan"] = "#00ffff";
            map["magenta"] = "#ff00ff";
            map["orange"] = "#ffa500";
            map["purple"] = "#800080";
            map["brown"] = "#a52a2a";
            map["gray"] = "#808080";
            map["grey"] = "#808080";
            map["pink"] = "#ffc0cb";
            map["gold"] = "#ffd700";
            map["silver"] = "#c0c0c0";
        }
        return map;
    }
};

} // namespace geo
} // namespace jotcad
