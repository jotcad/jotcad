#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <json.hpp>
#include "vfs_node.h"

namespace jotcad {
namespace geo {

using json = nlohmann::json;

/**
 * Operation: A semantic recipe for a JOT operation.
 * Inherits from Selector but is distinguishable at the type level.
 */
struct Operation : fs::Selector {
    Operation() = default;
    Operation(const std::string& p, const nlohmann::json& params) {
        path = p;
        parameters = params;
    }
    
    static Operation from_json(const json& j) {
        if (j.is_string()) return Operation(j.get<std::string>(), json::object());
        if (!j.contains("path")) return Operation("", json::object());
        return Operation(j.at("path").get<std::string>(), j.value("parameters", json::object()));
    }
};

/**
 * Shape: The foundational semantic container for JOT.
 */
struct Shape {
    std::optional<fs::Selector> geometry;
    std::vector<double> tf = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    nlohmann::json tags = nlohmann::json::object();
    std::vector<Shape> components;

    void add_tag(const std::string& key, const nlohmann::json& value) {
        tags[key] = value;
    }

    nlohmann::json to_json() const {
        nlohmann::json j = {{"tf", tf}, {"tags", tags}};
        if (geometry.has_value()) j["geometry"] = geometry->to_json();
        if (!components.empty()) {
            j["components"] = nlohmann::json::array();
            for (const auto& c : components) j["components"].push_back(c.to_json());
        }
        return j;
    }

    static Shape from_json(const nlohmann::json& j) {
        Shape s;
        try {
            if (j.contains("geometry") && !j.at("geometry").is_null()) {
                s.geometry = fs::Selector::from_json(j.at("geometry"));
            }
            if (j.contains("tf") && j.at("tf").is_array()) {
                s.tf = j.at("tf").get<std::vector<double>>();
            }
            if (j.contains("tags") && j.at("tags").is_object()) {
                s.tags = j.at("tags");
            }
            if (j.contains("components") && j.at("components").is_array()) {
                for (const auto& c : j.at("components")) s.components.push_back(from_json(c));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Shape::from_json] ERROR: " << e.what() << " in JSON: " << j.dump() << std::endl;
            throw;
        }
        return s;
    }
};

// ADL helpers for nlohmann::json
inline void from_json(const json& j, Shape& s) { s = Shape::from_json(j); }
inline void to_json(json& j, const Shape& s) { j = s.to_json(); }

inline void from_json(const json& j, Operation& op) { op = Operation::from_json(j); }
inline void to_json(json& j, const Operation& op) { j = op.to_json(); }

} // namespace geo
} // namespace jotcad
