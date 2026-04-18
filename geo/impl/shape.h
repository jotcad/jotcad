#pragma once
#include <string>
#include <vector>
#include <array>
#include <json.hpp>
#include "vfs_node.h"

namespace jotcad {
namespace geo {

using json = nlohmann::json;

/**
 * Shape: The foundational semantic container for JOT.
 */
struct Shape {
    jotcad::fs::Selector geometry;

    std::vector<double> tf = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    json tags = json::object();
    
    // Components for hierarchical assemblies
    std::vector<Shape> components;

    // Helpers
    void add_tag(const std::string& key, const json& value) {
        tags[key] = value;
    }

    json to_json() const {
        json j = {
            {"geometry", geometry.to_json()},
            {"tf", tf},
            {"tags", tags}
        };
        if (!components.empty()) {
            json comps = json::array();
            for (const auto& c : components) comps.push_back(c.to_json());
            j["components"] = comps;
        }
        return j;
    }

    static Shape from_json(const json& j) {
        Shape s;
        if (j.contains("geometry")) s.geometry = jotcad::fs::Selector::from_json(j.at("geometry"));
        s.tf = j.value("tf", std::vector<double>{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1});
        s.tags = j.value("tags", json::object());
        if (j.contains("components")) {
            for (const auto& c : j.at("components")) s.components.push_back(from_json(c));
        }
        return s;
    }
};

} // namespace geo
} // namespace jotcad
